/*
 * inc/mm_wds.h: Wireless Data Service (WDS) helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_WDS_H
#define MM_WDS_H

#include <CtlService.h>
#include <QmiService.h>
#include <wds.h>

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

enum mm_wds_autoconnect_setting {
    MM_WDS_AUTOCONNECT_SETTING_DISABLED = 0,
    MM_WDS_AUTOCONNECT_SETTING_ENABLED = 1,
    MM_WDS_AUTOCONNECT_SETTING_PAUSED = 2,
    MM_WDS_AUTOCONNECT_SETTING_MAX = 2,
    MM_WDS_AUTOCONNECT_SETTING_INVALID = 255,
};

enum mm_wds_autoconnect_roam_setting {
    MM_WDS_AUTOCONNECT_ROAM_SETTING_ALWAYS = 0,
    MM_WDS_AUTOCONNECT_ROAM_SETTING_HOME_ONLY = 1,
    MM_WDS_AUTOCONNECT_ROAM_SETTING_MAX = 1,
    MM_WDS_AUTOCONNECT_ROAM_SETTING_INVALID = 255,
};

enum mm_wds_ip_family_preference {
    MM_WDS_IP_FAMILY_PREFERENCE_IPV4 = PACK_WDS_IPV4,
    MM_WDS_IP_FAMILY_PREFERENCE_IPV6 = PACK_WDS_IPV6,
};

struct mm_wds_runtime_settings {
    union {
        struct in_addr in;
        struct in6_addr in6;
    } address;

    union {
        struct in_addr in;
        struct in6_addr in6;
    } gateway;

    int prefix_length;
};

struct mm_wds_session {
    QmiService wds;
    struct mm_wds_runtime_settings last_runtime_settings;

    uint32_t session_id;
    uint32_t profile;
    int family;

    bool teardown_requested;
};

int mm_wds_get_autoconnect_settings(QmiService *,
        enum mm_wds_autoconnect_setting *,
        enum mm_wds_autoconnect_roam_setting *);

int mm_wds_get_runtime_settings(struct mm_wds_session *,
        struct mm_wds_runtime_settings *, bool *, bool *);

int mm_wds_get_session_state(struct mm_wds_session *, uint32_t *);

int mm_wds_set_autoconnect_settings(QmiService *,
        enum mm_wds_autoconnect_setting,
        enum mm_wds_autoconnect_roam_setting);

int mm_wds_set_ip_family_preference(QmiService *,
        enum mm_wds_ip_family_preference);

int mm_wds_start_data_session(struct mm_wds_session *, uint32_t, int,
        uint32_t *, uint32_t *, uint32_t *, bool *, bool *);

int mm_wds_stop_data_session(struct mm_wds_session *);

int mm_wds_initialize(QmiService *, CtlService *, struct mm_wds_session *);
int mm_wds_shutdown(QmiService *, CtlService *);

#endif
