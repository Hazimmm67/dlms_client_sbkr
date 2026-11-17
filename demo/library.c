/* Temporary local demo API. Its JSON wire format is not the production SBKR protocol. */
#include "sbkr_client.h"
#include "sbkr_logger.h"
#include "protocol.h"
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct _sbkr_client_t {
    zsock_t *socket;
    char *endpoint;
    int timeout;
};
struct sbkr_logger {
    sbkr_client_t *transport;
    char *source;
    uint64_t next_ticket, pending_ticket;
    int pending, wants_confirmation;
};
static int reconnect(sbkr_client_t *c)
{
    zsock_destroy(&c->socket);
    c->socket = zsock_new(ZMQ_REQ);
    if (!c->socket) return -1;
    zsock_set_linger(c->socket, 0);
    zsock_set_sndtimeo(c->socket, c->timeout);
    zsock_set_rcvtimeo(c->socket, c->timeout);
    zsock_set_maxmsgsize(c->socket, DEMO_MAX_BYTES);
    if (zsock_connect(c->socket, "%s", c->endpoint)) {
        zsock_destroy(&c->socket); return -1;
    }
    return 0;
}
sbkr_client_t *sbkr_data_client_new(const char *endpoint, int verbose)
{
    (void)verbose;
    if (!demo_endpoint_valid(endpoint)) { errno = EINVAL; return NULL; }
    sbkr_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->endpoint = strdup(endpoint); c->timeout = 1000;
    if (!c->endpoint || reconnect(c)) { sbkr_client_destroy(&c); return NULL; }
    return c;
}
void sbkr_client_destroy(sbkr_client_t **cp)
{
    if (!cp || !*cp) return;
    sbkr_client_t *c = *cp;
    zsock_destroy(&c->socket); free(c->endpoint); free(c); *cp = NULL;
}
void sbkr_client_set_timeout(sbkr_client_t *c, int timeout)
{
    if (!c || timeout < 1) return;
    c->timeout = timeout;
    if (c->socket) { zsock_set_sndtimeo(c->socket, timeout); zsock_set_rcvtimeo(c->socket, timeout); }
}
int sbkr_client_connect_gateway(sbkr_client_t *c, const char *endpoint)
{
    if (!c || !demo_endpoint_valid(endpoint)) return -1;
    char *copy = strdup(endpoint);
    if (!copy) return -1;
    free(c->endpoint); c->endpoint = copy; return reconnect(c);
}
static sbkr_errorcode_t response_status(const cJSON *root)
{
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!demo_protocol_valid(root) || !cJSON_IsNumber(status) ||
        status->valuedouble < SBKR_STATUS_OK || status->valuedouble > SBKR_STATUS_MISMATCH_ID ||
        floor(status->valuedouble) != status->valuedouble) return SBKR_STATUS_HEADER_FAILED;
    return (sbkr_errorcode_t)status->valueint;
}
static sbkr_errorcode_t receive_reply(sbkr_client_t *c, cJSON **reply)
{
    char *wire = zstr_recv(c->socket);
    if (!wire) {
        sbkr_errorcode_t status = errno == EAGAIN ? SBKR_STATUS_TIMEOUT : SBKR_STATUS_RECEIVE_FAILED;
        reconnect(c); return status;
    }
    *reply = cJSON_ParseWithOpts(wire, NULL, 1);
    free(wire);
    return response_status(*reply);
}
static sbkr_errorcode_t request(sbkr_client_t *c, cJSON *message, cJSON **reply)
{
    if (!c) return SBKR_STATUS_NULL_POINTER;
    if (!message) return SBKR_STATUS_OUT_OF_MEMORY;
    if (!c->socket && reconnect(c)) return SBKR_STATUS_CONNECT_FAILED;
    char *wire = cJSON_PrintUnformatted(message);
    if (!wire) return SBKR_STATUS_OUT_OF_MEMORY;
    if (strlen(wire) > DEMO_MAX_BYTES) { free(wire); return SBKR_STATUS_BUFFER_EXCEEDED; }
    int sent = zstr_send(c->socket, wire); free(wire);
    if (sent) { reconnect(c); return SBKR_STATUS_SEND_FAILED; }
    return receive_reply(c, reply);
}
sbkr_errorcode_t sbkr_write_data(sbkr_client_t *c, unsigned int count, sbkr_data *points)
{
    if (!c || !points) return SBKR_STATUS_NULL_POINTER;
    if (!count || count > DEMO_MAX_POINTS) return SBKR_STATUS_BUFFER_EXCEEDED;
    cJSON *message = demo_message("write"), *reply = NULL;
    cJSON *array = cJSON_AddArrayToObject(message, "points");
    sbkr_errorcode_t status = SBKR_STATUS_OK;
    if (!array) { cJSON_Delete(message); return SBKR_STATUS_OUT_OF_MEMORY; }
    for (unsigned int i=0; i<count; ++i) {
        if (points[i].Type != SBKR_TYPE_DOUBLE) { status = SBKR_STATUS_NOT_SUPPORTED; goto done; }
        for (unsigned int j=0; j<i; ++j) if (points[j].Id == points[i].Id) { status = SBKR_STATUS_DUPLICATE; goto done; }
        cJSON *row = demo_encode_point(&points[i]);
        if (!row) { status = SBKR_STATUS_BUFFER_MISMATCH; goto done; }
        cJSON_AddItemToArray(array, row);
    }
    status = request(c, message, &reply);
done:
    cJSON_Delete(message); cJSON_Delete(reply); return status;
}
sbkr_errorcode_t sbkr_read_data(sbkr_client_t *c, unsigned int count, sbkr_data *points)
{
    if (!c || !points) return SBKR_STATUS_NULL_POINTER;
    if (!count || count > DEMO_MAX_POINTS) return SBKR_STATUS_BUFFER_EXCEEDED;
    cJSON *message = demo_message("read"), *reply = NULL;
    cJSON *ids = cJSON_AddArrayToObject(message, "ids");
    sbkr_data *decoded = calloc(count, sizeof(*decoded));
    sbkr_errorcode_t status = SBKR_STATUS_OUT_OF_MEMORY;
    if (!ids || !decoded) goto done;
    for (unsigned int i=0; i<count; ++i) cJSON_AddItemToArray(ids, cJSON_CreateNumber(points[i].Id));
    status = request(c, message, &reply);
    if (status != SBKR_STATUS_OK) goto done;
    cJSON *rows = cJSON_GetObjectItemCaseSensitive(reply,"points");
    if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) != (int)count) { status = SBKR_STATUS_BUFFER_MISMATCH; goto done; }
    for (unsigned int i=0; i<count; ++i) {
        status = demo_decode_point(cJSON_GetArrayItem(rows,(int)i), &decoded[i]);
        if (status) goto done;
        if (decoded[i].Id != points[i].Id) { status = SBKR_STATUS_MISMATCH_ID; goto done; }
    }
    memcpy(points, decoded, count*sizeof(*points));
done:
    free(decoded); cJSON_Delete(message); cJSON_Delete(reply); return status;
}
static sbkr_errorcode_t pointer_data(sbkr_client_t *c, unsigned int count, sbkr_data **points, int write)
{
    if (!c || !points) return SBKR_STATUS_NULL_POINTER;
    if (!count || count > DEMO_MAX_POINTS) return SBKR_STATUS_BUFFER_EXCEEDED;
    sbkr_data *flat = calloc(count, sizeof(*flat));
    if (!flat) return SBKR_STATUS_OUT_OF_MEMORY;
    for (unsigned int i=0; i<count; ++i) {
        if (!points[i]) { free(flat); return SBKR_STATUS_NULL_POINTER; }
        flat[i] = *points[i];
    }
    sbkr_errorcode_t status = write ? sbkr_write_data(c,count,flat) : sbkr_read_data(c,count,flat);
    if (!status && !write) for (unsigned int i=0; i<count; ++i) *points[i]=flat[i];
    free(flat); return status;
}
sbkr_errorcode_t sbkr_write_data_ptr(sbkr_client_t *c,unsigned int n,sbkr_data **v){return pointer_data(c,n,v,1);}
sbkr_errorcode_t sbkr_read_data_ptr(sbkr_client_t *c,unsigned int n,sbkr_data **v){return pointer_data(c,n,v,0);}
sbkr_logger_t *sbkr_logger_open(const char *source,const char *endpoint)
{
    if (!source || !*source) return NULL;
    sbkr_logger_t *l=calloc(1,sizeof(*l));
    if (!l) return NULL;
    l->source=strdup(source); l->transport=sbkr_data_client_new(endpoint,0);
    if (!l->source || !l->transport) { sbkr_logger_close(&l); return NULL; }
    l->next_ticket=1; return l;
}
int sbkr_logger_log_async(sbkr_logger_t *l,sbkr_log_level_t level,uint64_t *ticket,const char *format,...)
{
    if (!l || !format || level<SBKR_LOG_ERROR || level>SBKR_LOG_DEBUG) return -1;
    if (l->pending) {
        if (l->wants_confirmation) return -1;
        char *previous=zstr_recv_nowait(l->transport->socket);
        if (!previous) return -1;
        free(previous);l->pending=0;
    }
    char text[4096];va_list args;va_start(args,format);
    int size=vsnprintf(text,sizeof(text),format,args);va_end(args);
    if (size<0 || (size_t)size>=sizeof(text) || !l->next_ticket) return -1;
    cJSON *message=demo_message("log");
    if (!message || !cJSON_AddStringToObject(message,"source",l->source) ||
        !cJSON_AddStringToObject(message,"message",text) || !cJSON_AddNumberToObject(message,"level",level)) {
        cJSON_Delete(message);return -1;
    }
    char *wire=cJSON_PrintUnformatted(message);cJSON_Delete(message);
    if (!wire) return -1;
    zsock_set_sndtimeo(l->transport->socket,0);
    int sent=zstr_send(l->transport->socket,wire);free(wire);
    if (sent) { reconnect(l->transport);return -1; }
    l->pending=1;l->wants_confirmation=ticket!=NULL;l->pending_ticket=l->next_ticket++;
    if (ticket) *ticket=l->pending_ticket;
    return 0;
}
int sbkr_logger_receive(sbkr_logger_t *l,int timeout,sbkr_log_confirmation_t *result)
{
    if (!l || !result || timeout<0) return SBKR_LOG_RECEIVE_ERROR;
    if (!l->pending) return SBKR_LOG_RECEIVE_TIMEOUT;
    zsock_set_rcvtimeo(l->transport->socket,timeout);
    char *wire=zstr_recv(l->transport->socket);
    if (!wire) return errno==EAGAIN?SBKR_LOG_RECEIVE_TIMEOUT:SBKR_LOG_RECEIVE_ERROR;
    cJSON *reply=cJSON_ParseWithOpts(wire,NULL,1);free(wire);
    sbkr_errorcode_t status=response_status(reply);cJSON_Delete(reply);
    result->ticket=l->pending_ticket;
    result->status=status==SBKR_STATUS_OK?SBKR_LOG_DELIVERY_RETAINED:SBKR_LOG_DELIVERY_REJECTED;
    l->pending=0;
    return status==SBKR_STATUS_HEADER_FAILED?SBKR_LOG_RECEIVE_ERROR:0;
}
void sbkr_logger_close(sbkr_logger_t **lp)
{
    if (!lp || !*lp) return;
    sbkr_logger_t *l=*lp;sbkr_client_destroy(&l->transport);free(l->source);free(l);*lp=NULL;
}
/* Unsupported header surfaces fail explicitly; no production wire behavior is guessed. */
sbkr_client_t *sbkr_client_new(char *b,int v){(void)b;(void)v;errno=ENOTSUP;return NULL;}
int sbkr_client_setsockopt(sbkr_client_t *c,int o,const void *v,size_t n){(void)c;(void)o;(void)v;(void)n;errno=ENOTSUP;return -1;}
int sbkr_client_getsockopt(sbkr_client_t *c,int o,void *v,size_t *n){(void)c;(void)o;(void)v;(void)n;errno=ENOTSUP;return -1;}
void sbkr_client_send(sbkr_client_t *c,char *s,zmsg_t **r){(void)c;(void)s;(void)r;errno=ENOTSUP;}
zmsg_t *sbkr_client_recv(sbkr_client_t *c,char **cmd,char **s){(void)c;if(cmd)*cmd=NULL;if(s)*s=NULL;errno=ENOTSUP;return NULL;}
sbkr_errorcode_t sbkr_read_sync_data(sbkr_client_t *c,sbkr_time_t t,unsigned int n,sbkr_data *v){(void)c;(void)t;(void)n;(void)v;return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_read_sync_data_ptr(sbkr_client_t *c,sbkr_time_t t,unsigned int n,sbkr_data **v){(void)c;(void)t;(void)n;(void)v;return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_subscribe_data(sbkr_client_t *c,unsigned int n,const sbkr_data *v){(void)c;(void)n;(void)v;return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_read_event(sbkr_client_t *c,unsigned int max,unsigned int *n,sbkr_event_t *v,unsigned int t){(void)c;(void)max;(void)v;(void)t;if(n)*n=0;return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_read_event_ex(sbkr_client_t *c,sbkr_event_fn *f,unsigned int t,void *a){(void)c;(void)f;(void)t;(void)a;return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_subscription_stats(sbkr_client_t *c,sbkr_subscription_stats_t *s){(void)c;if(s)memset(s,0,sizeof(*s));return SBKR_STATUS_NOT_SUPPORTED;}
sbkr_errorcode_t sbkr_stream_event(sbkr_client_t *c,unsigned int n,sbkr_data **v,sbkr_errorcode_t (*f)(sbkr_client_t*,sbkr_errorcode_t,void*),unsigned int t,void *a){(void)c;(void)n;(void)v;(void)f;(void)t;(void)a;return SBKR_STATUS_NOT_SUPPORTED;}
