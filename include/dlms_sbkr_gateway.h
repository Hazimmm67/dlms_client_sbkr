#ifndef DLMS_SBKR_GATEWAY_H
#define DLMS_SBKR_GATEWAY_H
#include "dlms_client_sbkr.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dlms_sbkr_gateway dlms_sbkr_gateway_t;
/* Load the matching SBKR shared library at runtime. Headers alone cannot
   implement the data-plane protocol. This fails clearly if symbols are absent. */
dlms_sbkr_gateway_t *dlms_sbkr_gateway_open(const char *library,const char *endpoint,
    int timeout_ms,char *error,size_t error_size);
sbkr_errorcode_t dlms_sbkr_gateway_publish(void *gateway,unsigned int count,sbkr_data *points);
/* Optional logger, using the same library. Each handle belongs to one thread. */
int dlms_sbkr_gateway_enable_logger(dlms_sbkr_gateway_t *gateway,const char *endpoint);
void dlms_sbkr_gateway_log(void *gateway,const char *message);
void dlms_sbkr_gateway_close(dlms_sbkr_gateway_t **gateway);
#ifdef __cplusplus
}
#endif
#endif
