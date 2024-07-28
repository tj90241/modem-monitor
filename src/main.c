/*
 * src/main.c
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_dms.h"
#include "mm_log.h"
#include "mm_netlink.h"
#include "mm_qmux.h"
#include "mm_run_helpers.h"
#include "mm_sdbus.h"
#include "mm_wds.h"

#include <sd-bus.h>

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROFILE_3GPP_VZWINTERNET 3

static bool exit_requested;
static void handle_signal(int signal);
static int initialize(CtlService *, struct mm_netlink *, sd_bus *);

static int run_up_ipv4(struct mm_dms_service *, struct mm_netlink *,
        struct mm_wds_session *, sd_bus *, CtlService *);

static int run_up_ipv6(struct mm_dms_service *, struct mm_netlink *,
        sd_bus *, CtlService *);

static int run_sessions_up(struct mm_dms_service *, struct mm_netlink *,
        struct mm_wds_session *, struct mm_wds_session *);

void handle_signal(int signal) {
    if (signal == SIGINT) {
        exit_requested = true;
    }
}

static int initialize(CtlService *ctl, struct mm_netlink *mm_nl, sd_bus *bus) {
    struct mm_dms_service dms;
    enum mm_dms_operation_mode mode;
    int status, check;

    /* Wireguard interface should start link down; we'll up it later. */
    if ((status = mm_netlink_ensure_wg0_interface_state(mm_nl, false))) {
        MM_LOG("%s%s\n", "Failed to put down the Wireguard interface");
        return status;
    }

    /* Indicate that any cached fields in DMS need to be generated first. */
    memset(&dms, 0, sizeof(dms));

    if ((status = mm_configure_autoconnect_and_roaming(ctl))) {
        return status;
    }

    /*
     * Core initialization loop which calls into main loop:
     *
     * So long as an exit has not been signaled, the core loop can return with
     * exit_requested=false to reinitialize the WWAN host inteface and modem
     * services to serve as a reset mechanism if needed.
     */
    while (!exit_requested) {
        if ((status = mm_netlink_reload_link_cache(mm_nl))) {
            MM_LOG("%s%s\n", "Failed to reload the netlink link cache");
            break;
        }

        /*
         * Stop chrony and unbound before bringing up the connection: either
         * certain carriers or the modem get upset about UDP traffic that is
         * presumably sourced from nonsense? In flushing the DNS cache, we can
         * also validate connectivity by ensuring that name resolution works
         * after the modem comes up (and restart the connection if it fails).
         */
        if ((status = mm_sdbus_manage_service(bus, "StopUnit",
                "chrony.service"))) {
            MM_LOG("%s%s\n", "Failed to stop chrony before starting up");
            break;
        }

        if ((status = mm_sdbus_manage_service(bus, "StopUnit",
                "unbound.service"))) {
            MM_LOG("%s%s\n", "Failed to stop unbound before starting up");
            break;
        }

        /* Put WWAN host interface up; ensure that it has no addresses. */
        if ((status = mm_netlink_ensure_wwan_interface_state(mm_nl, true))) {
            MM_LOG("%s%s\n", "Failed to bring up the WWAN host interface");
            break;
        }

        if ((status = mm_netlink_addr_flush(mm_nl))) {
            MM_LOG("%s%s\n", "Failed to flush WWAN host interface addresses");
            break;
        }

        /* Ensure that the modem is in an online state. */
        if ((status = mm_dms_initialize(&dms, ctl)) != eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to initialize the DMS service object");
            break;
        }

        if ((status = mm_dms_set_power_sync(&dms,
                MM_DMS_OPERATION_MODE_ONLINE, &mode)) != eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to query/adjust modem operating state");
            break;
        }

        if (mode != MM_DMS_OPERATION_MODE_ONLINE) {
            MM_LOG("%s%s\n", "Modem operating mode cannot be set to online");
            status = -1;
            break;
        }

        /* Successfully initialized; proceed to bring up data sessions. */
        status = run_up_ipv6(&dms, mm_nl, bus, ctl);

        if ((check = mm_dms_shutdown(&dms, ctl,
                exit_requested)) != eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to shutdown the DMS service object");
            status = check;
            break;
        }

        /* Put the WWAN host and Wireguard interfaces down to kill routing. */
        if ((check = mm_netlink_reload_link_cache(mm_nl))) {
            MM_LOG("%s%s\n", "Failed to reload the netlink link cache");
            status = check;
            break;
        }

        if ((check = mm_netlink_ensure_wwan_interface_state(mm_nl, false))) {
            MM_LOG("%s%s\n", "Failed to put down the WWAN host interface");
            status = check;
            break;
        }

        if ((check = mm_netlink_ensure_wg0_interface_state(mm_nl, false))) {
            MM_LOG("%s%s\n", "Failed to put down the Wireguard interface");
            status = check;
            break;
        }

        /* Stop chrony/unbound (as per above) now that we have no internet. */
        if ((check = mm_sdbus_manage_service(bus, "StopUnit",
                "chrony.service"))) {
            MM_LOG("%s%s\n", "Failed to stop chrony when shutting down");
            exit_requested = true;
            status = check;
        }

        if ((check = mm_sdbus_manage_service(bus, "StopUnit",
                "unbound.service"))) {
            MM_LOG("%s%s\n", "Failed to stop unbound when shutting down");
            exit_requested = true;
            status = check;
        }

        /*
         * If the run function ever terminates early due to it failing its
         * own setup, we sleep here to e.g. avoid excessive modem operations
         * that might upset the network operator.
         */
        if (!exit_requested) {
            sleep(10);
        }
    }

    return status;
}

int main(int argc, char **argv) {
    struct sigaction sa;
    int status;

    CtlService ctl;
    QmuxTransport qmux;
    struct mm_netlink mm_nl;
    sd_bus *bus;

    /* Register a handler for SIGINT and clear the exit flag. */
    memset(&sa, 0, sizeof(sa));
    exit_requested = false;

    sa.sa_handler = &handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    if (sigaction(SIGINT, &sa, NULL)) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    /* Initialize the Qmux transport and CtlService. */
    if (mm_qmux_transport_initialize(&qmux) != eQCWWAN_ERR_NONE) {
        MM_LOG("%s%s\n", "Failed to initialize the QMI transport");
        return EXIT_FAILURE;
    }

    if (mm_ctl_initialize(&ctl, &qmux) != eQCWWAN_ERR_NONE) {
        MM_LOG("%s%s\n", "Failed to initialize a CtlService client");
        status = EXIT_FAILURE;
    }

    /* Initialize netlink, sd-bus interfaces and continue startup. */
    else {
        if (mm_netlink_initialize(&mm_nl)) {
            MM_LOG("%s%s\n", "Failed to initialize netlink layer");
            status = EXIT_FAILURE;
        }

        else {
            if (sd_bus_open_system(&bus) < 0) {
                perror("sd_bus_open_system");
                status = EXIT_FAILURE;
            }

            else {
                status = initialize(&ctl, &mm_nl, bus)
                    ? EXIT_FAILURE
                    : EXIT_SUCCESS;

                sd_bus_close(bus);
                sd_bus_unref(bus);
            }

            /* Flush the addresses on the WWAN interface and mark it down. */
            if (mm_netlink_addr_flush(&mm_nl) ||
                    mm_netlink_reload_link_cache(&mm_nl) ||
                    mm_netlink_ensure_wwan_interface_state(&mm_nl, false)) {
                MM_LOG("%s%s\n", "Failed to shutdown the WWAN host interface");
            }

            mm_netlink_shutdown(&mm_nl);
        }

        mm_ctl_shutdown(&ctl);
    }

    mm_qmux_transport_shutdown(&qmux);
    return status;
}

int run_up_ipv4(struct mm_dms_service *dms, struct mm_netlink *mm_nl,
        struct mm_wds_session *session_v6, sd_bus *bus, CtlService *ctl) {
    bool address_present, gateway_present;
    struct mm_wds_session session_v4;
    int status, check;

    /* Setup a WDS service instance and connect to the network. */
    memset(&session_v4, 0, sizeof(session_v4));

    if ((status = mm_wds_initialize(&session_v4.wds, ctl, &session_v4))) {
        MM_LOG("%s%s\n", "Failed to initialize the IPv4 WDS service object");
        exit_requested = true;
        return status;
    }

    if ((status = mm_start_session(&session_v4, PROFILE_3GPP_VZWINTERNET,
            MM_WDS_IP_FAMILY_PREFERENCE_IPV4)) == eQCWWAN_ERR_NONE) {
        MM_LOG("%sStarted IPv4 data session: CID=%"PRIu8", SID=0x%"PRIx32"\n",
                session_v4.wds.clientId, session_v4.session_id);

        /* Query for IPv4 runtime settings, validate and apply them. */
        if ((status = mm_wds_get_runtime_settings(&session_v4,
                &session_v4.last_runtime_settings, &address_present,
                &gateway_present)) != eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to get initial IPv4 runtime settings");
        }

        else if (!address_present || !gateway_present) {
            MM_LOG("%s%s\n", "Missing IPv4 address/gateway in settings?");
            status = -1;
        }

        else if ((status = mm_apply_ipv4_runtime_settings(mm_nl,
                &session_v4.last_runtime_settings, false)) ==
                eQCWWAN_ERR_NONE) {

            /* Start DNS/NTP daemons and enter the run (monitoring) loop. */
            if ((status = mm_sdbus_manage_service(bus, "StartUnit",
                    "unbound.service"))) {
                MM_LOG("%s%s\n", "Failed to start unbound after modem up");
                exit_requested = true;
            }

            else if ((status = mm_exec_wireguard_setconf()) ||
                    (status = mm_netlink_ensure_wg0_interface_state(
                    mm_nl, true)) || mm_netlink_ensure_wg0_routes_are_applied(
                    mm_nl)) {
                MM_LOG("%s%s\n", "Failed to bring up the Wireguard interface");

                /*
                 * Do not request an exit, as a failure to bring up Wireguard
                 * likely means that we are unable to issue DNS queries right
                 * now. Attempting to restart the modem should fix this...
                 */
            }

            else if ((status = mm_sdbus_manage_service(bus, "StartUnit",
                    "chrony.service"))) {
                MM_LOG("%s%s\n", "Failed to start chrony after modem up");
                exit_requested = true;
            }

            else {
                status = run_sessions_up(dms, mm_nl, &session_v4, session_v6);
            }
        }

        else {
            MM_LOG("%s%s\n", "Failed to apply IPv4 configuration to the host");
            exit_requested = true;
        }

        /*
         * If the call was prematurely ended, trying to stop it again may
         * raise an error here that would otherwise make it look like the
         * shutdown is not clean ("no effect"). Make sure that we do not
         * consider such a case to be an error.
         */
        if ((check = mm_wds_stop_data_session(&session_v4)) !=
                eQCWWAN_ERR_NONE && check != eQCWWAN_ERR_QMI_NO_EFFECT) {
            MM_LOG("%sFailed to stop the IPv4 data session (%d)\n", status);
            exit_requested = true;
            status = check;
        }
    }

    else {
        /* Do not request an exit: the signal is likely too weak. */
        MM_LOG("%sFailed to start the IPv4 data session (%d)\n", status);
    }

    if ((check = mm_wds_shutdown(&session_v4.wds, ctl))) {
        MM_LOG("%s%s\n", "Failed to shutdown the IPv4 WDS service object");
        exit_requested = true;
        status = check;
    }

    return status;
}

int run_up_ipv6(struct mm_dms_service *dms, struct mm_netlink *mm_nl,
        sd_bus *bus, CtlService *ctl) {
    bool address_present, gateway_present;
    struct mm_wds_session session_v6;
    int status, check;

    /* Setup a WDS service instance and connect to the network. */
    memset(&session_v6, 0, sizeof(session_v6));

    if ((status = mm_wds_initialize(&session_v6.wds, ctl, &session_v6))) {
        MM_LOG("%s%s\n", "Failed to initialize the IPv6 WDS service object");
        exit_requested = true;
        return status;
    }

    if ((status = mm_start_session(&session_v6, PROFILE_3GPP_VZWINTERNET,
            MM_WDS_IP_FAMILY_PREFERENCE_IPV6)) == eQCWWAN_ERR_NONE) {
        MM_LOG("%sStarted IPv6 data session: CID=%"PRIu8", SID=0x%"PRIx32"\n",
                session_v6.wds.clientId, session_v6.session_id);

        /* Query for IPv6 runtime settings and validate them. */
        if ((status = mm_wds_get_runtime_settings(&session_v6,
                &session_v6.last_runtime_settings, &address_present,
                &gateway_present)) != eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to get initial IPv6 runtime settings");
        }

        else if (!address_present || !gateway_present) {
            MM_LOG("%s%s\n", "Missing IPv6 address/gateway in settings?");
            status = -1;
        }

        /* Display the configuration and begin setting up the IPv4 session. */
        else if ((status = mm_apply_ipv6_runtime_settings(mm_nl,
                &session_v6.last_runtime_settings, false)) ==
                eQCWWAN_ERR_NONE) {
            status = run_up_ipv4(dms, mm_nl, &session_v6, bus, ctl);
        }

        else {
            MM_LOG("%s%s\n", "Failed to apply IPv6 configuration to the host");
            exit_requested = true;
        }

        /*
         * If the call was prematurely ended, trying to stop it again may
         * raise an error here that would otherwise make it look like the
         * shutdown is not clean ("no effect"). Make sure that we do not
         * consider such a case to be an error.
         */
        if ((check = mm_wds_stop_data_session(&session_v6)) !=
                eQCWWAN_ERR_NONE && check != eQCWWAN_ERR_QMI_NO_EFFECT) {
            MM_LOG("%sFailed to stop the IPv6 data session (%d)\n", status);
            exit_requested = true;
            status = check;
        }
    }

    else {
        /* Do not request an exit: the signal is likely too weak. */
        MM_LOG("%sFailed to start the IPv6 data session (%d)\n", status);
    }

    if ((check = mm_wds_shutdown(&session_v6.wds, ctl))) {
        MM_LOG("%s%s\n", "Failed to shutdown the IPv6 WDS service object");
        exit_requested = true;
        status = check;
    }

    return status;
}

int run_sessions_up(struct mm_dms_service *dms, struct mm_netlink *mm_nl,
        struct mm_wds_session *session_v4, struct mm_wds_session *session_v6) {
    while (!exit_requested && !session_v4->teardown_requested &&
            !session_v6->teardown_requested) {
        sleep(1);
    }

    MM_LOG("%s%s\n", "Stopping the modem-monitor due to external request");
    return 0;
}
