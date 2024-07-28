/*
 * src/sdbus.c: sd-bus helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_log.h"
#include "mm_sdbus.h"

#include <sd-bus.h>

int mm_sdbus_manage_service(sd_bus *bus, const char *method,
        const char *service) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *message = NULL;
    int status;

    if ((status = sd_bus_call_method(bus,
            "org.freedesktop.systemd1",         /* service */
            "/org/freedesktop/systemd1",        /* path */
            "org.freedesktop.systemd1.Manager", /* interface */
            method, &error, &message, "ss",     /* signature */
            service, "replace")) < 0) {
        MM_LOG("%smm_sdbus_manage_service: %s\n", error.message);

        sd_bus_error_free(&error);
        sd_bus_message_unref(message);
        return -1;
    }

    return 0;
}
