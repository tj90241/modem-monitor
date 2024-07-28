/*
 * src/run_helpers.c: Various helper functions used by the run loop
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_log.h"
#include "mm_run_helpers.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <qmerrno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

int mm_apply_ipv4_runtime_settings(struct mm_netlink *mm_nl,
        const struct mm_wds_runtime_settings *settings, bool refresh) {
    char ipv4_address_str[INET_ADDRSTRLEN];
    char ipv4_gateway_str[INET_ADDRSTRLEN];
    uint32_t address, gateway;
    int status;

    address = settings->address.in.s_addr;
    gateway = settings->gateway.in.s_addr;

    inet_ntop(AF_INET, &address, ipv4_address_str, sizeof(ipv4_address_str));
    inet_ntop(AF_INET, &gateway, ipv4_gateway_str, sizeof(ipv4_gateway_str));
    MM_LOG("%sApplying IPv4 Configuration: address=%s/%d, gateway=%s\n",
            ipv4_address_str, settings->prefix_length, ipv4_gateway_str);

    if (refresh) {
        if ((status = mm_netlink_reload_address_cache(mm_nl)) !=
                eQCWWAN_ERR_NONE) {
            return status;
        }
    }

    if ((status = mm_netlink_add_v4_address(mm_nl, address,
            settings->prefix_length)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    if ((status = mm_netlink_change_v4_default_gateway(mm_nl,
            address, gateway)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    return eQCWWAN_ERR_NONE;
}

int mm_apply_ipv6_runtime_settings(struct mm_netlink *mm_nl,
        const struct mm_wds_runtime_settings *settings, bool refresh) {
    char ipv6_address_str[INET6_ADDRSTRLEN];
    char ipv6_gateway_str[INET6_ADDRSTRLEN];
    const struct in6_addr *address, *gateway;
    int status;

    address = &settings->address.in6;
    gateway = &settings->gateway.in6;

    inet_ntop(AF_INET6, address, ipv6_address_str, sizeof(ipv6_address_str));
    inet_ntop(AF_INET6, gateway, ipv6_gateway_str, sizeof(ipv6_gateway_str));
    MM_LOG("%sApplying IPv6 Configuration: address=%s/%d, gateway=%s\n",
            ipv6_address_str, settings->prefix_length, ipv6_gateway_str);

    if (refresh) {
        if ((status = mm_netlink_reload_address_cache(mm_nl)) !=
                eQCWWAN_ERR_NONE) {
            return status;
        }
    }

    if ((status = mm_netlink_add_v6_address(mm_nl, address,
            settings->prefix_length)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    if ((status = mm_netlink_change_v6_default_gateway(mm_nl, address,
            gateway, settings->prefix_length)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    return eQCWWAN_ERR_NONE;
}

int mm_configure_autoconnect_and_roaming(CtlService *ctl) {
    QmiService wds;
    int status, check;

    if ((status = mm_wds_initialize(&wds, ctl, NULL)) != eQCWWAN_ERR_NONE) {
        MM_LOG("%s%s\n", "Failed to initialize the WDS service for setup");
        return status;
    }

    else {
        if ((status = mm_wds_set_autoconnect_settings(&wds,
                MM_WDS_AUTOCONNECT_SETTING_DISABLED,
                MM_WDS_AUTOCONNECT_ROAM_SETTING_HOME_ONLY)) !=
                eQCWWAN_ERR_NONE) {
            MM_LOG("%s%s\n", "Failed to set WDS autoconnect settings");
        }
    }

    if ((check = mm_wds_shutdown(&wds, ctl)) != eQCWWAN_ERR_NONE) {
        MM_LOG("%s%s\n", "Failed to shutdown the WDS service after setup");
        status = check;
    }

    return status;
}

int mm_exec_wireguard_setconf(void) {
    pid_t child;
    int status;

    if ((child = fork()) == 0) {
        execl("/usr/bin/wg", "/usr/bin/wg", "setconf", "wg0",
                "/etc/wireguard/wireguard.conf", NULL);

        perror("execl");
        exit(255);
    }

    else if (child < 0) {
        perror("fork");
        return 1;
    }

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    else if (!WIFEXITED(status)) {
        return -1;
    }

    return WEXITSTATUS(status);
}

int mm_start_session(struct mm_wds_session *session, unsigned profile_id,
        enum mm_wds_ip_family_preference preference) {
    int status, family;
    bool reason_present, verbose_reason_present;
    uint32_t failure_reason, verbose_failure_reason_type,
            verbose_failure_reason;

    if ((status = mm_wds_set_ip_family_preference(&session->wds,
            preference))) {
        MM_LOG("%s%s\n", "Failed to set IP family preference");
        return status;
    }

    family = preference == MM_WDS_IP_FAMILY_PREFERENCE_IPV4
        ? AF_INET
        : AF_INET6;

    if ((status = mm_wds_start_data_session(session, profile_id, family,
            &failure_reason, &verbose_failure_reason_type,
            &verbose_failure_reason, &reason_present,
            &verbose_reason_present))) {
        if (verbose_reason_present) {
            if (reason_present) {
                MM_LOG("%sFailed to start a data session: "
                    "VerboseFailureReasonType=%"PRIu32", "
                    "VerboseFailureReason=%"PRIu32", "
                    "FailureReason=%"PRIu32"\n",
                    verbose_failure_reason_type,
                    verbose_failure_reason,
                    failure_reason);
            }

            else {
                MM_LOG("%sFailed to start a data session: "
                    "VerboseFailureReasonType=%"PRIu32", "
                    "VerboseFailureReason=%"PRIu32"\n",
                    verbose_failure_reason_type,
                    verbose_failure_reason);
            }
        }

        else if (reason_present) {
            MM_LOG("%sFailed to start a data session: "
                "FailureReason=%"PRIu32"\n",
                failure_reason);
        }

        return status;
    }

    return eQCWWAN_ERR_NONE;
}
