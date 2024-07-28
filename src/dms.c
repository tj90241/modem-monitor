/*
 * src/dms.c: Device Management Service (DMS) helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_dms.h"

#include <dms.h>
#include <QmiSyncObject.h>
#include <qmerrno.h>
#include <swidms.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static int dms_get_model_sync(struct mm_dms_service *, char **);
static void dms_indication_callback(uint8_t *, uint16_t, void *);

int dms_get_model_sync(struct mm_dms_service *dms, char **model_id) {
    unpack_dms_GetModelID_t resp;
    int status;

    memset(&resp, 0, sizeof(resp));
    *model_id = NULL;

    if ((status = QmiService_SendSyncRequest(&dms->dms_service,
            (pack_func) pack_dms_GetModelID, "pack_dms_GetModelID", NULL,
            (unpack_func) unpack_dms_GetModelID, "unpack_dms_GetModelID", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 1)) {
            if ((*model_id = malloc(strlen(resp.modelid) + 1)) == NULL) {
                return eQCWWAN_ERR_QMI_NO_MEMORY;
            }

            strcpy(*model_id, resp.modelid);
        }
    }

    return status;
}

void dms_indication_callback(uint8_t* qmi_packet,
        uint16_t qmi_packet_size, void* context) {
}

const char *mm_dms_get_operation_mode_string(enum mm_dms_operation_mode mode) {
    static const char *mm_dms_operation_modes[] = {
        "Online",
        "Low power (airplane) mode",
        "Factory test mode",
        "Offline",
        "Resetting",
        "Power off",
        "Persistent low power (airplane) mode",
        "Mode-only low power",
    };

    const size_t entries = sizeof(mm_dms_operation_modes) /
            sizeof(*mm_dms_operation_modes);

    if (mode >= entries) {
        return "Invalid";
    }

    return mm_dms_operation_modes[mode];
}

int mm_dms_get_power_sync(struct mm_dms_service *dms,
        enum mm_dms_operation_mode *mode, bool *hardware_controlled_mode) {
    unpack_dms_GetPower_t resp;
    int status;

    memset(&resp, 0, sizeof(resp));
    *hardware_controlled_mode = false;
    *mode = MM_DMS_OPERATION_MODE_INVALID;

    if ((status = QmiService_SendSyncRequest(&dms->dms_service,
            (pack_func) pack_dms_GetPower, "pack_dms_GetPower", NULL,
            (unpack_func) unpack_dms_GetPower, "unpack_dms_GetPower", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 1)) {
            *mode = resp.OperationMode;
        }

        if (swi_uint256_get_bit(resp.ParamPresenceMask, 16)) {
            *hardware_controlled_mode = !!resp.HardwareControlledMode;
        }
    }

    return status;
}

int mm_dms_initialize(struct mm_dms_service *dms, CtlService *ctl) {
    int status, check;

    memset(&dms->dms_service, 0, sizeof(dms->dms_service));
    memset(&dms->swi_dms_service, 0, sizeof(dms->swi_dms_service));
    status = eQCWWAN_ERR_NONE;

    /* There is no SWI DMS notification from firmware. */
    if ((status = CtlService_InitializeRegularServiceEx(ctl,
            &dms->swi_dms_service, eSWIDMS, NULL, NULL, 0)) !=
            eQCWWAN_ERR_NONE) {
        return status;
    }

    if ((check = CtlService_InitializeRegularServiceEx(ctl, &dms->dms_service,
            eDMS, dms_indication_callback, dms, 0)) != eQCWWAN_ERR_NONE) {
        status = check;
    }

    else {
        /* Cache any values which will not change at runtime. */
        if (dms->model_id == NULL) {
            if ((check = dms_get_model_sync(dms,
                    &dms->model_id)) != eQCWWAN_ERR_NONE) {
                status = check;
            }
        }

        if (status == eQCWWAN_ERR_NONE) {
            return status;
        }

        if ((check = CtlService_ShutDownRegularService(ctl,
                &dms->dms_service)) != eQCWWAN_ERR_NONE) {
            status = check;
        }
    }

    if ((check = CtlService_ShutDownRegularService(ctl,
            &dms->swi_dms_service)) != eQCWWAN_ERR_NONE) {
        status = check;
    }

    return status;
}

int mm_dms_set_power_sync(struct mm_dms_service *dms,
        enum mm_dms_operation_mode requested_mode,
        enum mm_dms_operation_mode *resulting_mode) {
    pack_dms_SetPower_t req;
    unpack_dms_SetPower_t resp;
    int status;

    enum mm_dms_operation_mode current_mode;
    bool hardware_controlled_mode;

    /*
     * Query the current mode and check if either the new mode would result
     * in a state change, or the mode is hardware controlled and thus setting
     * it would be futile.
     */
    *resulting_mode = MM_DMS_OPERATION_MODE_INVALID;

    if ((status = mm_dms_get_power_sync(dms, &current_mode,
            &hardware_controlled_mode)) != eQCWWAN_ERR_NONE) {
        return status;
    }

    if (current_mode == requested_mode || hardware_controlled_mode) {
        *resulting_mode = current_mode;
        return eQCWWAN_ERR_NONE;
    }

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.mode = requested_mode;

    if ((status = QmiService_SendSyncRequest(&dms->dms_service,
            (pack_func) pack_dms_SetPower, "pack_dms_SetPower", &req,
            (unpack_func) unpack_dms_SetPower, "unpack_dms_SetPower", &resp,
            DEFAULT_SYNC_REQUEST_TIMEOUT_S)) == eQCWWAN_ERR_NONE) {
        if (resp.Tlvresult != eQCWWAN_ERR_NONE) {
            return resp.Tlvresult;
        }

        /* Read the power state back out to see if it really changed. */
        if ((status = mm_dms_get_power_sync(dms, &current_mode,
                &hardware_controlled_mode)) != eQCWWAN_ERR_NONE) {
            return status;
        }

        *resulting_mode = current_mode;

        if (current_mode != requested_mode) {
            status = eQCWWAN_ERR_GENERAL;
        }
    }

    return status;
}

int mm_dms_shutdown(struct mm_dms_service *dms,
        CtlService *ctl, bool deallocate_cached_fields) {
    int status, check;

    if (deallocate_cached_fields) {
        free(dms->model_id);
        dms->model_id = NULL;
    }

    /* If either DMS service shutdown raises an error, return it. */
    status = CtlService_ShutDownRegularService(ctl,
            &dms->swi_dms_service);

    if ((check = CtlService_ShutDownRegularService(ctl,
            &dms->dms_service)) != eQCWWAN_ERR_NONE) {
        status = check;
    }

    return status;
}
