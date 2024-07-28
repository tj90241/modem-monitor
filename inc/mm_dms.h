/*
 * inc/mm_dms.h: Device Management Service (DMS) helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_DMS_H
#define MM_DMS_H

#include <CtlService.h>
#include <QmiService.h>

#include <stdbool.h>

enum mm_dms_operation_mode {
    MM_DMS_OPERATION_MODE_ONLINE = 0,
    MM_DMS_OPERATION_MODE_LOW_POWER = 1,
    MM_DMS_OPERATION_MODE_FACTORY_TEST = 2,
    MM_DMS_OPERATION_MODE_OFFLINE = 3,
    MM_DMS_OPERATION_MODE_RESETTING = 4,
    MM_DMS_OPERATION_MODE_POWER_OFF = 5,
    MM_DMS_OPERATION_MODE_PERSISTENT_LOW_POWER = 6,
    MM_DMS_OPERATION_MODE_ONLY_LOW_POWER = 7,
    MM_DMS_OPERATION_MODE_MAX = 8,
    MM_DMS_OPERATION_MODE_INVALID = 255,
};

struct mm_dms_service {
    QmiService dms_service;
    QmiService swi_dms_service;
    char *model_id;
};

__attribute__(( pure ))
const char *mm_dms_get_operation_mode_string(enum mm_dms_operation_mode);

int mm_dms_get_power_sync(struct mm_dms_service *,
        enum mm_dms_operation_mode *, bool *);

int mm_dms_set_power_sync(struct mm_dms_service *,
        enum mm_dms_operation_mode, enum mm_dms_operation_mode *);

int mm_dms_initialize(struct mm_dms_service *, CtlService *);
int mm_dms_shutdown(struct mm_dms_service *, CtlService *, bool);

#endif
