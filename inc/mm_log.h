/*
 * inc/mm_log.h: Logging helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_LOG_H
#define MM_LOG_H

#include <sys/time.h>
#include <stdio.h>
#include <time.h>

#define MM_LOG(fmt, ...) \
  do { \
    struct timeval tv; \
    struct tm *tm; \
    char buf[64]; \
    \
    gettimeofday(&tv, NULL); \
    buf[0] = 0; \
    \
    if ((tm = localtime(&tv.tv_sec)) != NULL) { \
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z: ", tm); \
    } \
    fprintf(stderr, fmt, buf, __VA_ARGS__); \
  } while (0)

#endif
