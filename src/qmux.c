/*
 * src/qmux.c: QMUX helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_qmux.h"

#include <stdbool.h>
#include <string.h>

int mm_ctl_initialize(CtlService *ctl, QmuxTransport *qmux) {
    memset(ctl, 0, sizeof(*ctl));
    return CtlService_InitializeEx(ctl, qmux, false, 0);
}

void mm_ctl_shutdown(CtlService *ctl) {
    CtlService_ShutDown(ctl);
}

int mm_qmux_transport_initialize(QmuxTransport *qmux) {
    char qmi_device_path[15];
    strncpy(qmi_device_path, "/dev/wwan0qmi0", sizeof(qmi_device_path));

    memset(qmux, 0, sizeof(*qmux));
    return QmuxTransport_InitializeEx2(qmux, qmi_device_path,
                                       QMUX_INTERFACE_DIRECT,
                                       NULL, false, false);
}

void mm_qmux_transport_shutdown(QmuxTransport *qmux) {
    QmuxTransport_ShutDown(qmux);
}
