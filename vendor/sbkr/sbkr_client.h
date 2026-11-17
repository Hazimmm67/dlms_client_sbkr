/*  =========================================================================
    sbkr_client.h - client API
    -------------------------------------------------------------------------
    Copyright (c) 2026 wan_azlan@simpro.com.my
    Copyright other contributors as noted in the AUTHORS file.
    =========================================================================
*/

#ifndef __SBKR_CLIENT_H_INCLUDED__
#define __SBKR_CLIENT_H_INCLUDED__

#include <czmq.h>
#include "sbkr_data.h"

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _sbkr_client_t sbkr_client_t;

//  @interface
CZMQ_EXPORT sbkr_client_t *
    sbkr_client_new (char *broker, int verbose);
CZMQ_EXPORT void
    sbkr_client_destroy (sbkr_client_t **self_p);
CZMQ_EXPORT void
    sbkr_client_set_timeout (sbkr_client_t *self, int timeout);
CZMQ_EXPORT int
    sbkr_client_setsockopt (sbkr_client_t *self, int option, const void *optval,
    size_t optvallen);
CZMQ_EXPORT int
    sbkr_client_getsockopt (sbkr_client_t *self, int option, void *optval,
    size_t *optvallen);
CZMQ_EXPORT void
    sbkr_client_send (sbkr_client_t *self, char *service, zmsg_t **request_p);
CZMQ_EXPORT zmsg_t *
    sbkr_client_recv (sbkr_client_t *self, char **command_p, char **service_p);

// Connect the dedicated gateway DEALER used for DATA operations.
CZMQ_EXPORT int
    sbkr_client_connect_gateway (sbkr_client_t *self, const char *endpoint);

// Create a data-plane-only client connected directly to a gateway.
CZMQ_EXPORT sbkr_client_t *
    sbkr_data_client_new (const char *gateway, int verbose);

// Write current data. Array of values
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_write_data(sbkr_client_t *self, unsigned int count, sbkr_data *values);

// Write current data. Array of pointers to values
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_write_data_ptr(sbkr_client_t *self, unsigned int count, sbkr_data **values);

// Return current data. Values must be initialized to blank on initial read.
// Free string and blob buffers returned by the final read.
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_data(sbkr_client_t *self, unsigned int count, sbkr_data *values);

// Array of pointers
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_data_ptr(sbkr_client_t *self, unsigned int count, sbkr_data **values);

// Return synchronized data. Values must be initialized to blank on initial read.
// String type is not supported
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_sync_data(sbkr_client_t *self, sbkr_time_t utc, unsigned int count, sbkr_data *values);

// Array of pointers
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_sync_data_ptr(sbkr_client_t *self, sbkr_time_t utc, unsigned int count, sbkr_data **values);

//  ---------------------------------------------------------------------
//  Subscription / event delivery
//  ---------------------------------------------------------------------

// Fidelity verdict attached to each delivered event.
typedef enum {
    SBKR_EVENT_OK = 0,          // sequence == last+1, or first seen for point
    SBKR_EVENT_GAP,             // sequence jumped: intermediate update(s) lost
    SBKR_EVENT_DUPLICATE,       // sequence repeated
    SBKR_EVENT_OUT_OF_ORDER     // sequence went backwards
} sbkr_event_state_t;

// One subscribed UPDATE push, decoded and fidelity-checked.
typedef struct {
    sbkr_data          point;             // full snapshot (free STRING/BLOB)
    uint64_t           ticket;            // originating write ticket
    sbkr_event_state_t state;             // fidelity verdict
    uint64_t           expected_sequence; // watermark+1 at decode time
    uint32_t           flags;             // reserved, currently 0
} sbkr_event_t;

// Callback receiving one event. Return SBKR_STATUS_OK to continue, or any
// other code to stop the train early. The event is borrowed and valid only
// for the duration of the callback.
typedef sbkr_errorcode_t (sbkr_event_fn)(sbkr_client_t *self,
                                         const sbkr_event_t *event, void *arg);

// Cumulative subscription fidelity counters.
typedef struct {
    uint64_t events_received;
    uint64_t gaps_detected;
    uint64_t duplicates_detected;
    uint64_t out_of_order_detected;
    uint64_t queue_overflows;
    uint32_t points_subscribed;
} sbkr_subscription_stats_t;

// Subscribe this client to a set of point ids and start a background reader
// thread that drains UPDATE pushes into an internal queue. Only one
// subscription is allowed per client.
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_subscribe_data(sbkr_client_t *self, unsigned int count,
                        const sbkr_data *values);

// Pull up to max_count events from the background queue. Blocks up to timeout
// ms for the first event (0 uses the client timeout), then drains any further
// events already queued. Free STRING/BLOB values in the returned points.
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_event(sbkr_client_t *self, unsigned int max_count,
                    unsigned int *count, sbkr_event_t *events,
                    unsigned int timeout);

// Consume the background queue as a continuous train, invoking fn per event
// in arrival order. Returns the callback's code if it stops early,
// SBKR_STATUS_TIMEOUT if no event arrives before idle_timeout_ms, or
// SBKR_STATUS_OK once the train goes idle after delivering at least one event.
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_read_event_ex(sbkr_client_t *self, sbkr_event_fn fn,
                       unsigned int idle_timeout_ms, void *arg);

// Snapshot the cumulative subscription fidelity counters.
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_subscription_stats(sbkr_client_t *self,
                            sbkr_subscription_stats_t *stats);

// Stream event to user definable function.
// Only one streamer is allowed per socket
CZMQ_EXPORT sbkr_errorcode_t
    sbkr_stream_event(sbkr_client_t *self, unsigned int count, sbkr_data **values, sbkr_errorcode_t (*fn)(sbkr_client_t *self, sbkr_errorcode_t result, void *arg), unsigned int timeout, void *arg);
//  @end

#ifdef __cplusplus
}
#endif

#endif
