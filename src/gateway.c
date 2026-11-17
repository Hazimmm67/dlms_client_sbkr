#include "dlms_sbkr_gateway.h"
#include "sbkr_client.h"
#include "sbkr_logger.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct dlms_sbkr_gateway {
    void *library;sbkr_client_t *client;sbkr_logger_t *logger;
    sbkr_client_t *(*create)(const char *,int);
    void (*destroy)(sbkr_client_t **);
    void (*timeout)(sbkr_client_t *,int);
    sbkr_errorcode_t (*write)(sbkr_client_t *,unsigned int,sbkr_data *);
    sbkr_logger_t *(*log_open)(const char *,const char *);
    void (*log_close)(sbkr_logger_t **);
    int (*log_async)(sbkr_logger_t *,sbkr_log_level_t,uint64_t *,const char *,...);
};
/* POSIX specifies dlsym conversion to function pointers. memcpy avoids a
   non-ISO direct cast warning on supported POSIX platforms. */
#define LOAD(field,name) do {void *s=dlsym(g->library,name);memcpy(&g->field,&s,sizeof(g->field));if(!s){snprintf(error,n,"SBKR library missing required symbol: %s",name);goto fail;}}while(0)
dlms_sbkr_gateway_t *dlms_sbkr_gateway_open(const char *library,const char *endpoint,int timeout,char *error,size_t n) {
    char discard[256];dlms_sbkr_gateway_t *g=calloc(1,sizeof(*g));
    if(!error || !n){error=discard;n=sizeof(discard);}error[0]=0;
    if(!g){snprintf(error,n,"Out of memory");return NULL;}
    if(!library||!*library||!endpoint||!*endpoint||timeout<1){snprintf(error,n,"SBKR requires a library path, endpoint, and positive timeout");goto fail;}
    g->library=dlopen(library,RTLD_NOW|RTLD_LOCAL);
    if(!g->library){snprintf(error,n,"Cannot load matching SBKR library: %s",dlerror());goto fail;}
    LOAD(create,"sbkr_data_client_new");LOAD(destroy,"sbkr_client_destroy");
    LOAD(timeout,"sbkr_client_set_timeout");LOAD(write,"sbkr_write_data");
    g->client=g->create(endpoint,0);
    if(!g->client){snprintf(error,n,"SBKR client creation failed");goto fail;}
    g->timeout(g->client,timeout);return g;
fail:dlms_sbkr_gateway_close(&g);return NULL;
}
sbkr_errorcode_t dlms_sbkr_gateway_publish(void *arg,unsigned int count,sbkr_data *points) {
    dlms_sbkr_gateway_t *g=arg;
    return g&&g->client?g->write(g->client,count,points):SBKR_STATUS_CONNECT_FAILED;
}
int dlms_sbkr_gateway_enable_logger(dlms_sbkr_gateway_t *g,const char *endpoint) {
    if(!g||!endpoint||!*endpoint||g->logger)return -1;
    void *a=dlsym(g->library,"sbkr_logger_open"),*b=dlsym(g->library,"sbkr_logger_close"),*c=dlsym(g->library,"sbkr_logger_log_async");
    if(!a||!b||!c)return -1;
    memcpy(&g->log_open,&a,sizeof(g->log_open));memcpy(&g->log_close,&b,sizeof(g->log_close));memcpy(&g->log_async,&c,sizeof(g->log_async));
    g->logger=g->log_open("dlms_client_sbkr",endpoint);return g->logger?0:-1;
}
void dlms_sbkr_gateway_log(void *arg,const char *message) {
    dlms_sbkr_gateway_t *g=arg;
    if(g&&g->logger)g->log_async(g->logger,SBKR_LOG_ERROR,NULL,"%s",message);
}
void dlms_sbkr_gateway_close(dlms_sbkr_gateway_t **gp) {
    if(!gp||!*gp)return;
    dlms_sbkr_gateway_t *g=*gp;
    if(g->logger)g->log_close(&g->logger);
    if(g->client)g->destroy(&g->client);
    if(g->library)dlclose(g->library);
    free(g);*gp=NULL;
}
