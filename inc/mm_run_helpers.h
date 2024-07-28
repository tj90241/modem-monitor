/*
 * inc/mm_run_helpers.h: Various helpers used by the run loop
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_RUN_HELPERS_H
#define MM_RUN_HELPERS_H

#include "mm_netlink.h"
#include "mm_wds.h"

#include <stdint.h>

int mm_apply_ipv4_runtime_settings(struct mm_netlink *,
        const struct mm_wds_runtime_settings *, bool refresh);

int mm_apply_ipv6_runtime_settings(struct mm_netlink *,
        const struct mm_wds_runtime_settings *, bool refresh);

int mm_configure_autoconnect_and_roaming(CtlService *);

int mm_exec_wireguard_setconf(void);

int mm_start_session(struct mm_wds_session *, unsigned,
        enum mm_wds_ip_family_preference);

#endif
