/*
 * inc/mm_qmux.h: QMUX helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_QMUX_H
#define MM_QMUX_H

#include <CtlService.h>
#include <QmuxTransport.h>

int mm_ctl_initialize(CtlService *, QmuxTransport *);
void mm_ctl_shutdown(CtlService *);

int mm_qmux_transport_initialize(QmuxTransport *);
void mm_qmux_transport_shutdown(QmuxTransport *);

#endif
