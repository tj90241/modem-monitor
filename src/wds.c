/*
 * src/wds.h: Wireless Data Service (WDS) helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_log.h"
#include "mm_wds.h"

#include <msgid.h>
#include <QmiSyncObject.h>
#include <wds.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const char *get_connection_status_string(uint8_t);
static const char *get_reconfiguration_string(uint8_t);
static void wds_indication_callback(uint8_t *, uint16_t, void *);

const char *get_connection_status_string(uint8_t connection_status) {
    const char *statuses[] = {
        "DISCONNECTED",
        "CONNECTED",
        "SUSPENDED",
        "AUTHENTICATING",
    };

    if (connection_status == 0 ||
            connection_status > sizeof(statuses) / sizeof(*statuses)) {
        return "INVALID";
    }

    return statuses[connection_status - 1];
}

const char *get_reconfiguration_string(
        uint8_t reconfiguration_required) {
    return !!reconfiguration_required ? "YES" : "NO";
}

int mm_wds_get_autoconnect_settings(QmiService *wds,
        enum mm_wds_autoconnect_setting *autoconnect_setting,
        enum mm_wds_autoconnect_roam_setting *autoconnect_roam_setting) {
    unpack_wds_GetAutoconnectSetting_t resp;
    int status;

    *autoconnect_setting = MM_WDS_AUTOCONNECT_SETTING_INVALID;
    *autoconnect_roam_setting = MM_WDS_AUTOCONNECT_ROAM_SETTING_INVALID;
    memset(&resp, 0, sizeof(resp));

    if ((status = QmiService_SendSyncRequestNoInput(wds,
            (pack_func_no_input) pack_wds_GetAutoconnect,
            "pack_wds_GetAutoconnect",
            (unpack_func) unpack_wds_GetAutoconnectExt,
            "unpack_wds_GetAutoconnectExt", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 1)) {
            *autoconnect_setting = resp.autoconnect_setting;
        }

        if (resp.pAutoconnect_roam_setting) {
            *autoconnect_roam_setting = *resp.pAutoconnect_roam_setting;
        }
    }

    return status;
}

int mm_wds_get_runtime_settings(struct mm_wds_session *session,
        struct mm_wds_runtime_settings *settings, bool *address_present,
        bool *gateway_present) {
    pack_wds_SLQSGetRuntimeSettings_t req;
    unpack_wds_SLQSGetRuntimeSettings_t resp;
    uint32_t request_settings;
    int status;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    memset(settings, 0, sizeof(*settings));

    request_settings = 0x300;
    req.pReqSettings = &request_settings;
    *address_present = false;
    *gateway_present = false;

    if ((status = QmiService_SendSyncRequest(&session->wds,
            (pack_func) pack_wds_SLQSGetRuntimeSettings,
            "pack_wds_SLQSGetRuntimeSettings", &req,
            (unpack_func) unpack_wds_SLQSGetRuntimeSettings,
            "unpack_wds_SLQSGetRuntimeSettings", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 30)) {
            if (session->family == AF_INET) {
                *address_present = true;
                settings->address.in.s_addr = ntohl(resp.IPv4);
            }
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 32) &&
                swi_uint256_get_bit(resp.ParamPresenceMask, 33)) {
            if (session->family == AF_INET) {
                *gateway_present = true;
                settings->gateway.in.s_addr = ntohl(resp.GWAddressV4);
                settings->prefix_length = 32;

                while (!(resp.SubnetMaskV4 & 0x1)) {
                    resp.SubnetMaskV4 = resp.SubnetMaskV4 >> 1;
                    settings->prefix_length--;
                }
            }
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 37)) {
            if (session->family == AF_INET6) {
                unsigned i;

                for (i = 0; i < 8; i++) {
                    uint16_t u16 = ntohs(resp.IPV6AddrInfo.IPAddressV6[i]);
                    settings->address.in6.__in6_u.__u6_addr16[i] = u16;
                }

                *address_present = true;
                settings->prefix_length = resp.IPV6AddrInfo.IPV6PrefixLen;
            }
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 38)) {
            if (session->family == AF_INET6) {
                int prefix_length = resp.IPV6GWAddrInfo.gwV6PrefixLen;
                unsigned i;

                for (i = 0; i < 8; i++) {
                    uint16_t u16 = ntohs(resp.IPV6GWAddrInfo.gwAddressV6[i]);
                    settings->gateway.in6.__in6_u.__u6_addr16[i] = u16;
                }

                *gateway_present = true;

                /* Not sure why prefix_length appears twice...? */
                if (settings->prefix_length &&
                        settings->prefix_length != prefix_length) {
                    MM_LOG("%sIPv6 prefix length for address and "
                            "gateway differ? (/%d /%d)\n",
                            settings->prefix_length,
                            prefix_length);
                }

                else {
                    settings->prefix_length = prefix_length;
                }
            }
        }
    }

    return status;
}

int mm_wds_get_session_state(struct mm_wds_session *session,
        uint32_t *connection_status) {
    unpack_wds_GetSessionState_t resp;
    int status;

    memset(&resp, 0, sizeof(resp));
    *connection_status = 0;

    if ((status = QmiService_SendSyncRequestNoInput(&session->wds,
            (pack_func_no_input) pack_wds_GetSessionState,
            "pack_wds_GetSessionState",
            (unpack_func) unpack_wds_GetSessionState,
            "unpack_wds_GetSessionState,", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (!swi_uint256_get_bit(resp.ParamPresenceMask, 1)) {
            return -1;
        }

        *connection_status = resp.connectionStatus;
    }

    return status;
}

int mm_wds_initialize(QmiService *wds, CtlService *ctl,
        struct mm_wds_session *context) {
    memset(wds, 0, sizeof(*wds));

    return CtlService_InitializeRegularServiceEx(ctl,
            wds, eWDS, wds_indication_callback, context, 0);
}

int mm_wds_set_autoconnect_settings(QmiService *wds,
        enum mm_wds_autoconnect_setting autoconnect_setting,
        enum mm_wds_autoconnect_roam_setting autoconnect_roam_setting) {
    enum mm_wds_autoconnect_setting current_autoconnect_setting;
    enum mm_wds_autoconnect_roam_setting current_autoconnect_roam_setting;
    int status;

    pack_wds_SetAutoconnect_t req;
    unpack_wds_GetAutoconnectSetting_t resp;

    /*
     * Query the current autoconnect settings and check if the new
     * autoconnect would result in a state change.
     */
    current_autoconnect_setting = MM_WDS_AUTOCONNECT_SETTING_DISABLED;
    current_autoconnect_roam_setting = MM_WDS_AUTOCONNECT_ROAM_SETTING_INVALID;

    if ((status = mm_wds_get_autoconnect_settings(wds,
            &current_autoconnect_setting,
            &current_autoconnect_roam_setting)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    if (current_autoconnect_setting == autoconnect_setting &&
            current_autoconnect_roam_setting == autoconnect_roam_setting) {
        return eQCWWAN_ERR_NONE;
    }

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.acsetting = autoconnect_setting;
    req.acroamsetting = autoconnect_roam_setting;

    if ((status = QmiService_SendSyncRequest(wds,
            (pack_func) pack_wds_SetAutoconnect,
            "pack_wds_SetAutoconnect", &req,
            (unpack_func) unpack_wds_SetAutoconnect,
            "unpack_wds_SetAutoconnect", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }
    }

    return status;
}

int mm_wds_set_ip_family_preference(QmiService *wds,
        enum mm_wds_ip_family_preference preference) {
    pack_wds_SLQSSetIPFamilyPreference_t req;
    unpack_wds_SLQSSetIPFamilyPreference_t resp;
    int status;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.IPFamilyPreference = preference;

    if ((status = QmiService_SendSyncRequest(wds,
            (pack_func) pack_wds_SLQSSetIPFamilyPreference,
            "pack_wds_SLQSSetIPFamilyPreference", &req,
            (unpack_func) unpack_wds_SLQSSetIPFamilyPreference,
            "unpack_wds_SLQSSetIPFamilyPreference", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }
    }

    return status;
}

int mm_wds_shutdown(QmiService *wds, CtlService *ctl) {
    return CtlService_ShutDownRegularService(ctl, wds);
}

int mm_wds_start_data_session(struct mm_wds_session *session,
        uint32_t profile, int family, uint32_t *failure_reason,
        uint32_t *verbose_failure_reason_type,
        uint32_t *verbose_failure_reason, bool *reason_present,
        bool *verbose_reason_present) {
    pack_wds_SLQSStartDataSession_t req;
    unpack_wds_SLQSStartDataSession_t resp;
    int status;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    session->session_id = 0;
    session->profile = profile;
    session->family = family;

    *failure_reason = 0;
    *verbose_failure_reason_type = 0;
    *verbose_failure_reason = 0;
    *reason_present = false;
    *verbose_reason_present = false;

    req.pprofileid3gpp = &session->profile;
    resp.psid = &session->session_id;
    resp.pFailureReason = failure_reason;
    resp.pVerboseFailReasonType = verbose_failure_reason_type;
    resp.pVerboseFailureReason = verbose_failure_reason;

    if ((status = QmiService_SendSyncRequest(&session->wds,
            (pack_func) pack_wds_SLQSStartDataSession,
            "pack_wds_SLQSStartDataSession", &req,
            (unpack_func) unpack_wds_SLQSStartDataSession,
            "unpack_wds_SLQSStartDataSession", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (!swi_uint256_get_bit(resp.ParamPresenceMask, 1)) {
            return -1;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 16)) {
            *reason_present = true;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 17)) {
            *verbose_reason_present = true;
        }
    }

    return status;
}

int mm_wds_stop_data_session(struct mm_wds_session *session) {
    pack_wds_SLQSStopDataSession_t req;
    unpack_wds_SLQSStopDataSession_t resp;
    int status;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.psid = &session->session_id;

    if ((status = QmiService_SendSyncRequest(&session->wds,
            (pack_func) pack_wds_SLQSStopDataSession,
            "pack_wds_SLQSStopDataSession", &req,
            (unpack_func) unpack_wds_SLQSStopDataSession,
            "unpack_wds_SLQSStopDataSession", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }
    }

    return status;
}

void wds_indication_callback(uint8_t* qmi_packet,
        uint16_t qmi_packet_size, void* context) {
    struct mm_wds_session *session = (struct mm_wds_session *) context;
    unpack_qmi_t resp_context;
    const char *message_str;
    int status;

    /* Additional locals for processing indication callbacks */
    unpack_wds_SLQSSetPacketSrvStatusCallback_t packet_srv_status;

    bool reason_present, verbose_reason_present;
    uint8_t connection_status, host_reconfiguration_required;
    uint32_t session_end_reason, verbose_session_end_reason_type,
            verbose_session_end_reason;

    (void) message_str;
    message_str = helper_get_resp_ctx(eWDS, qmi_packet, qmi_packet_size,
            &resp_context);

    switch (resp_context.msgid) {
    case eQMI_WDS_PKT_SRVC_STATUS_IND:
        memset(&packet_srv_status, 0, sizeof(packet_srv_status));
        reason_present = verbose_reason_present = false;

        if ((status = unpack_wds_SLQSSetPacketSrvStatusCallback(qmi_packet,
                qmi_packet_size, &packet_srv_status) != eQCWWAN_ERR_NONE)) {
            MM_LOG("%s%s\n", "Failed to process packet service indication");
            break;
        }

        if (packet_srv_status.Tlvresult) {
            MM_LOG("%s%s\n", "Packet service indication signaled an error");
            break;
        }

        if (!swi_uint256_get_bit(packet_srv_status.ParamPresenceMask, 1)) {
            MM_LOG("%s%s\n", "Missing context in packet service indication");
            break;
        }

        if (swi_uint256_get_bit(packet_srv_status.ParamPresenceMask, 16)) {
            reason_present = true;
            session_end_reason = packet_srv_status.sessionEndReason;
        }

        else {
            reason_present = false;
            session_end_reason = 0;
        }

        if (swi_uint256_get_bit(packet_srv_status.ParamPresenceMask, 17)) {
            verbose_reason_present = true;

            verbose_session_end_reason_type =
                    packet_srv_status.verboseSessnEndReasonType;
            verbose_session_end_reason =
                    packet_srv_status.verboseSessnEndReason;
        }

        else {
            verbose_reason_present = false;
            verbose_session_end_reason_type = 0;
            verbose_session_end_reason = 0;
        }

        connection_status = packet_srv_status.conn_status;
        host_reconfiguration_required = packet_srv_status.reconfigReqd;

        if (verbose_reason_present) {
            if (reason_present) {
                MM_LOG("%sPacket service signaled session teardown: "
                    "Session=%"PRIx32", "
                    "ConnectionStatus=%s, "
                    "HostReconfigurationRequired=%s, "
                    "VerboseSessionEndReasonType=%"PRIu16", "
                    "VerboseSessionEndReason=%"PRIu16", "
                    "SessionEndReason=%"PRIu16"\n",
                    session->session_id,
                    get_connection_status_string(connection_status),
                    get_reconfiguration_string(host_reconfiguration_required),
                    verbose_session_end_reason_type,
                    verbose_session_end_reason,
                    session_end_reason);
            }

            else {
                MM_LOG("%sPacket service signaled session teardown: "
                    "Session=%"PRIx32", "
                    "ConnectionStatus=%s, "
                    "HostReconfigurationRequired=%s, "
                    "VerboseFailureReasonType=%"PRIu32", "
                    "VerboseFailureReason=%"PRIu32"\n",
                    session->session_id,
                    get_connection_status_string(connection_status),
                    get_reconfiguration_string(host_reconfiguration_required),
                    verbose_session_end_reason_type,
                    verbose_session_end_reason);
            }
        }

        else if (reason_present) {
            MM_LOG("%sPacket service signaled session teardown: "
                "Session=%"PRIx32", "
                "ConnectionStatus=%s, "
                "HostReconfigurationRequired=%s, "
                "SessionEndReason=%"PRIu16"\n",
                session->session_id,
                get_connection_status_string(connection_status),
                get_reconfiguration_string(host_reconfiguration_required),
                session_end_reason);
        }

        else {
            MM_LOG("%sPacket service indication received: "
                "Session=%p, "
                "ConnectionStatus=%s, "
                "HostReconfigurationRequired=%s\n",
                session,
                get_connection_status_string(connection_status),
                get_reconfiguration_string(host_reconfiguration_required));
        }

        /* If we ended the session, then do not signal session teardown. */
        if (session && session->session_id && connection_status == 1 && !(
                (reason_present && session_end_reason == 2) ||
                (verbose_reason_present &&
                    verbose_session_end_reason_type == 3 &&
                    verbose_session_end_reason == 2000))) {
            MM_LOG("%s%s\n", "Requesting main thread to teardown the session");
            session->teardown_requested = true;
        }

        break;

    default:
        MM_LOG("%sUnhandled WDS indication: MessageID=%"PRIu16"\n",
                resp_context.msgid);
        break;
    }
}
