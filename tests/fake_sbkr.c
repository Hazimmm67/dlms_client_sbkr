/* Test-only ABI fixture. Implements no networking or production SBKR protocol. */
#include "sbkr_client.h"
#include "sbkr_logger.h"
#include <stdlib.h>
#include <string.h>
struct _sbkr_client_t { int timeout; };
struct sbkr_logger { int unused; };
sbkr_client_t *sbkr_data_client_new(const char *endpoint,int verbose) {
    (void)verbose;return strcmp(endpoint,"test://unit")?NULL:calloc(1,sizeof(sbkr_client_t));
}
void sbkr_client_destroy(sbkr_client_t **p){free(*p);*p=NULL;}
void sbkr_client_set_timeout(sbkr_client_t *c,int ms){c->timeout=ms;}
sbkr_errorcode_t sbkr_write_data(sbkr_client_t *c,unsigned int n,sbkr_data *v) {
    if(c->timeout!=123)return SBKR_STATUS_TIMEOUT;
    return n==1 && v[0].Id==42 && v[0].Type==SBKR_TYPE_DOUBLE && v[0].Value.Double==1.25?SBKR_STATUS_OK:SBKR_STATUS_MISMATCH_ID;
}
sbkr_logger_t *sbkr_logger_open(const char *source,const char *endpoint) {
    return !strcmp(source,"dlms_client_sbkr")&&!strcmp(endpoint,"test://log")?calloc(1,sizeof(sbkr_logger_t)):NULL;
}
void sbkr_logger_close(sbkr_logger_t **p){free(*p);*p=NULL;}
int sbkr_logger_log_async(sbkr_logger_t *l,sbkr_log_level_t level,uint64_t *ticket,const char *format,...) {
    (void)l;(void)level;(void)ticket;(void)format;return 0;
}
