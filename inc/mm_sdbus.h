/*
 * inc/mm_sdbus.h: sd-bus helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_SDBUS_H
#define MM_SDBUS_H

#include <sd-bus.h>

int mm_sdbus_manage_service(sd_bus *, const char *, const char *);

#endif

