#include "internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
int ds_error(dlms_client_sbkr_t *c,const char *format,...) {
    va_list ap;va_start(ap,format);vsnprintf(c->error,sizeof(c->error),format,ap);va_end(ap);
    if(c->cb.log)c->cb.log(c->cb.log_user,c->error);
    return -1;
}
dlms_client_sbkr_t *dlms_client_sbkr_create(const char *path,const dlms_sbkr_callbacks_t *cb,char *error,size_t n) {
    dlms_client_sbkr_t *c=calloc(1,sizeof(*c));
    if(error&&n)error[0]=0;
    if(!c){if(error&&n)snprintf(error,n,"Out of memory");return NULL;}
    if(cb)c->cb=*cb;
    if(!path || ds_config_load(c,path)) {
        if(error&&n)snprintf(error,n,"%s",path?c->error:"Missing configuration path");
        dlms_client_sbkr_destroy(&c);return NULL;
    }
    return c;
}
int dlms_client_sbkr_connect(dlms_client_sbkr_t *c) {
    if(!c)return -1;
    if(c->connected)return 0;
    if(c->cb.read){c->connected=1;return 0;}
    return ds_transport_connect(c);
}
int dlms_client_sbkr_poll(dlms_client_sbkr_t *c,dlms_sbkr_poll_result_t *result) {
    dlms_sbkr_poll_result_t r={0};
    if(!c)return -1;
    if(!c->connected && dlms_client_sbkr_connect(c)) {
        /* Mark every point with communication failure and still publish health. */
        for(size_t i=0;i<c->count;i++)c->samples[i].Quality=SBKR_QUALITY_COMM_FAILURE;
    }
    for(size_t i=0;i<c->count;i++) {
        sbkr_data *s=&c->samples[i];double value=0;struct timespec ts;
        s->Quality=SBKR_QUALITY_COMM_FAILURE;
        int status=c->connected?(c->cb.read?c->cb.read(c->cb.read_user,&c->points[i],&value):ds_transport_read(c,i,&value)):-1;
        s->Time=clock_gettime(CLOCK_REALTIME,&ts)==0?(sbkr_time_t)ts.tv_sec*1000+(sbkr_time_t)ts.tv_nsec/1000000:0;
        s->Sequence++; /* Local observation sequence, not an SBKR ACK/ticket. */
        if(status || !isfinite(value)) {s->Quality=status?s->Quality:SBKR_QUALITY_INVALID;r.failed++;}
        else {
            s->Value.Double=value;
            if(c->points[i].has_range && (value<c->points[i].low || value>c->points[i].high)){s->Quality=SBKR_QUALITY_INVALID;r.failed++;}
            else{s->Quality=SBKR_QUALITY_GOOD;r.good++;}
        }
    }
    if(c->cb.publish)r.publish_status=c->cb.publish(c->cb.publish_user,(unsigned int)c->count,c->samples);
    if(result)*result=r;
    if(r.publish_status!=SBKR_STATUS_OK)return ds_error(c,"SBKR publication failed: %d",r.publish_status);
    return r.failed?1:0;
}
size_t dlms_client_sbkr_count(const dlms_client_sbkr_t *c){return c?c->count:0;}
const dlms_sbkr_point_t *dlms_client_sbkr_point(const dlms_client_sbkr_t *c,size_t i){return c&&i<c->count?&c->points[i]:NULL;}
const sbkr_data *dlms_client_sbkr_samples(const dlms_client_sbkr_t *c){return c?c->samples:NULL;}
unsigned int dlms_client_sbkr_interval_ms(const dlms_client_sbkr_t *c){return c?c->interval:0;}
const char *dlms_client_sbkr_error(const dlms_client_sbkr_t *c){return c?c->error:"NULL client";}
void dlms_client_sbkr_destroy(dlms_client_sbkr_t **cp) {
    if(!cp || !*cp)return;
    dlms_client_sbkr_t *c=*cp;ds_transport_close(c);
    free(c->points);free(c->samples);free(c->objects);free(c);*cp=NULL;
}
