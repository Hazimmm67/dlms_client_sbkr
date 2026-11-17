#include "protocol.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int demo_endpoint_valid(const char *endpoint)
{
    const char *prefix = "tcp://127.0.0.1:";
    if (!endpoint || strncmp(endpoint, prefix, strlen(prefix))) return 0;
    const char *port = endpoint + strlen(prefix);
    if (!isdigit((unsigned char)*port)) return 0;
    char *end;
    errno = 0;
    unsigned long number = strtoul(port, &end, 10);
    return !errno && !*end && number > 0 && number <= 65535;
}
int demo_protocol_valid(const cJSON *message)
{
    const cJSON *p = cJSON_GetObjectItemCaseSensitive(message, "protocol");
    return cJSON_IsObject(message) && cJSON_IsString(p) && !strcmp(p->valuestring, DEMO_PROTOCOL);
}
cJSON *demo_message(const char *operation)
{
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "protocol", DEMO_PROTOCOL) ||
        !cJSON_AddStringToObject(root, "op", operation)) {
        cJSON_Delete(root); return NULL;
    }
    return root;
}
cJSON *demo_reply(sbkr_errorcode_t status)
{
    cJSON *root = demo_message("reply");
    if (!root || !cJSON_AddNumberToObject(root, "status", status)) {
        cJSON_Delete(root); return NULL;
    }
    return root;
}
int demo_uint64(const cJSON *json, uint64_t *value)
{
    if (!cJSON_IsString(json) || !isdigit((unsigned char)json->valuestring[0])) return -1;
    char *end;
    errno = 0;
    unsigned long long n = strtoull(json->valuestring, &end, 10);
    if (errno || *end) return -1;
    *value = (uint64_t)n;
    return 0;
}
cJSON *demo_encode_point(const sbkr_data *p)
{
    if (!p || p->Type != SBKR_TYPE_DOUBLE || !isfinite(p->Value.Double) ||
        !p->Id || p->Quality < SBKR_QUALITY_CREATED || p->Quality > SBKR_QUALITY_UNKNOWN) return NULL;
    char sequence[32], timestamp[32];
    snprintf(sequence, sizeof(sequence), "%llu", (unsigned long long)p->Sequence);
    snprintf(timestamp, sizeof(timestamp), "%llu", (unsigned long long)p->Time);
    cJSON *row = cJSON_CreateObject();
    if (!row || !cJSON_AddNumberToObject(row, "id", p->Id) ||
        !cJSON_AddStringToObject(row, "type", "double") ||
        !cJSON_AddNumberToObject(row, "quality", p->Quality) ||
        !cJSON_AddStringToObject(row, "sequence", sequence) ||
        !cJSON_AddStringToObject(row, "time_ms", timestamp) ||
        !cJSON_AddNumberToObject(row, "value", p->Value.Double)) {
        cJSON_Delete(row); return NULL;
    }
    return row;
}
sbkr_errorcode_t demo_decode_point(const cJSON *row, sbkr_data *point)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(row, "id");
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(row, "type");
    const cJSON *quality = cJSON_GetObjectItemCaseSensitive(row, "quality");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(row, "value");
    uint64_t sequence, timestamp;
    if (!cJSON_IsObject(row) || !cJSON_IsNumber(id) || !isfinite(id->valuedouble) ||
        id->valuedouble < 1 || id->valuedouble > UINT_MAX || floor(id->valuedouble) != id->valuedouble ||
        !cJSON_IsNumber(quality) || !isfinite(quality->valuedouble) ||
        quality->valuedouble < SBKR_QUALITY_CREATED || quality->valuedouble > SBKR_QUALITY_UNKNOWN ||
        floor(quality->valuedouble) != quality->valuedouble || !cJSON_IsNumber(value) ||
        !isfinite(value->valuedouble) || demo_uint64(cJSON_GetObjectItemCaseSensitive(row,"sequence"), &sequence) ||
        demo_uint64(cJSON_GetObjectItemCaseSensitive(row,"time_ms"), &timestamp)) return SBKR_STATUS_BUFFER_MISMATCH;
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "double")) return SBKR_STATUS_NOT_SUPPORTED;
    memset(point, 0, sizeof(*point));
    point->Id = (unsigned int)id->valuedouble;
    point->Type = SBKR_TYPE_DOUBLE;
    point->Quality = (sbkr_quality_t)quality->valueint;
    point->Sequence = sequence; point->Time = timestamp; point->Value.Double = value->valuedouble;
    return SBKR_STATUS_OK;
}
