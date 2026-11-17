#ifndef DLMS_CLIENT_SBKR_H
#define DLMS_CLIENT_SBKR_H
#include <stddef.h>
#include "sbkr_data.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One handle per meter. Serialize calls on each handle and its callbacks. */
typedef struct dlms_client_sbkr dlms_client_sbkr_t;
typedef struct {
    char description[160];
    unsigned char obis[6];
    unsigned int sbkr_id, casdu, ioa;
    int has_id, has_range, has_fallback_scaler, fallback_scaler;
    double low, high;
    double simulator_value; /* Used only by the development meter server. */
} dlms_sbkr_point_t;
typedef struct {
    /* Omit read to use the built-in Gurux transport. Supply it for embedding
       or simulation. read returns zero and a scaled engineering value on success. */
    int (*read)(void *user, const dlms_sbkr_point_t *point, double *value);
    void *read_user;
    /* Optional. Receives all points, including failed reads with bad quality.
       Points are borrowed only until callback returns; never retain the pointer. */
    sbkr_errorcode_t (*publish)(void *user, unsigned int count, sbkr_data *points);
    void *publish_user;
    void (*log)(void *user, const char *message);
    void *log_user;
} dlms_sbkr_callbacks_t;
typedef struct {
    size_t good, failed;
    sbkr_errorcode_t publish_status;
} dlms_sbkr_poll_result_t;
/* Parses and validates configuration without opening connections. On failure,
   returns NULL and writes an explanation into error if provided. */
dlms_client_sbkr_t *dlms_client_sbkr_create(const char *json_path,
    const dlms_sbkr_callbacks_t *callbacks, char *error, size_t error_size);
int dlms_client_sbkr_connect(dlms_client_sbkr_t *client);
/* 0 = all reads and publication succeeded; 1 = at least one read failed;
   -1 = invalid state or publication failed. Samples remain available in all
   completed polls. Failed samples retain last value with current bad quality. */
int dlms_client_sbkr_poll(dlms_client_sbkr_t *client, dlms_sbkr_poll_result_t *result);
size_t dlms_client_sbkr_count(const dlms_client_sbkr_t *client);
const dlms_sbkr_point_t *dlms_client_sbkr_point(const dlms_client_sbkr_t *client, size_t index);
const sbkr_data *dlms_client_sbkr_samples(const dlms_client_sbkr_t *client);
unsigned int dlms_client_sbkr_interval_ms(const dlms_client_sbkr_t *client);
const char *dlms_client_sbkr_error(const dlms_client_sbkr_t *client);
void dlms_client_sbkr_destroy(dlms_client_sbkr_t **client);
#ifdef __cplusplus
}
#endif
#endif
