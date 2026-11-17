#ifndef SBKR_DEMO_PROTOCOL_H
#define SBKR_DEMO_PROTOCOL_H
#include "sbkr_data.h"
#include "cJSON.h"
#define DEMO_PROTOCOL "sbkr-demo/1"
#define DEMO_MAX_POINTS 4096U
#define DEMO_MAX_BYTES (1024U * 1024U)
int demo_endpoint_valid(const char *endpoint);
cJSON *demo_message(const char *operation);
cJSON *demo_reply(sbkr_errorcode_t status);
int demo_protocol_valid(const cJSON *message);
cJSON *demo_encode_point(const sbkr_data *point);
sbkr_errorcode_t demo_decode_point(const cJSON *json, sbkr_data *point);
int demo_uint64(const cJSON *json, uint64_t *value);
#endif
