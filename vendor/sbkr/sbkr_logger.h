/*  =========================================================================
    sbkr_logger.h - SBKR asynchronous application logger
    -------------------------------------------------------------------------
    Copyright (c) 2026 wan_azlan@simpro.com.my
    Copyright other contributors as noted in the AUTHORS file.
    =========================================================================
*/

#ifndef __SBKR_LOGGER_H_INCLUDED__
#define __SBKR_LOGGER_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SBKR_LOG_ERROR = 0,
    SBKR_LOG_WARN  = 1,
    SBKR_LOG_TRACE = 2,
    SBKR_LOG_INFO  = 3,
    SBKR_LOG_DEBUG = 4
} sbkr_log_level_t;

typedef struct sbkr_logger sbkr_logger_t;

typedef enum {
    SBKR_LOG_DELIVERY_RETAINED = 0,
    SBKR_LOG_DELIVERY_REJECTED = 1
} sbkr_log_delivery_status_t;

typedef struct {
    uint64_t ticket;
    sbkr_log_delivery_status_t status;
} sbkr_log_confirmation_t;

enum {
    SBKR_LOG_RECEIVE_ERROR = -1,
    SBKR_LOG_RECEIVE_TIMEOUT = 1
};

// Each logger handle owns a ZeroMQ socket and must be used by one thread only.
// Open one handle per thread/actor; after fork, open a new handle in the child.
sbkr_logger_t *
sbkr_logger_open(const char *source, const char *endpoint);

// Submit without waiting. Pass ticket_out to request an in-memory-retention ACK.
// Pass NULL for best-effort delivery. Returns 0 if queued locally, -1 otherwise.
int
sbkr_logger_log_async(sbkr_logger_t *logger, sbkr_log_level_t level,
                      uint64_t *ticket_out, const char *format, ...);

// Receive one asynchronous confirmation. Returns 0 with result, 1 on timeout,
// or -1 on transport/protocol failure.
int
sbkr_logger_receive(sbkr_logger_t *logger, int timeout_ms,
                    sbkr_log_confirmation_t *result);

void
sbkr_logger_close(sbkr_logger_t **logger_p);

#endif
