/* Copyright 2026. HTTPS telemetry transport for the FRDM-RW612. */
#ifndef DASHBOARD_TRANSPORT_H
#define DASHBOARD_TRANSPORT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

int DASHBOARD_PostJson(const char *host, const char *path, unsigned short port,
                      const char *secret, const char *json);
void DASHBOARD_TimeSyncSet(uint32_t epoch_seconds);
bool DASHBOARD_IsTimeReady(void);

#endif
