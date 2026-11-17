#include "dlms_client_sbkr.h"
#include "dlms_sbkr_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Failed at line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct state {int fail,range,nan,publish_fail;unsigned int calls;};
static int read_value(void *arg,const dlms_sbkr_point_t *p,double *value) {
    struct state *s=arg;s->calls++;
    if(s->fail && p->ioa==23)return -1;
    *value=s->nan?NAN:s->range?1e20:p->ioa==23?1234567890.125:0.5;return 0;
}
static sbkr_errorcode_t publish(void *arg,unsigned int count,sbkr_data *values) {
    struct state *s=arg;CHECK(count==25);CHECK(values[0].Type==SBKR_TYPE_DOUBLE);
    CHECK(values[0].Id==((15U<<16)|23));
    CHECK(values[24].Id==((15U<<16)|66));
    return s->publish_fail?SBKR_STATUS_TIMEOUT:SBKR_STATUS_OK;
}
int main(int argc,char **argv) {
    CHECK(argc==2);char error[256];struct state state={0};
    dlms_sbkr_callbacks_t cb={.read=read_value,.read_user=&state,.publish=publish,.publish_user=&state};
    dlms_client_sbkr_t *c=dlms_client_sbkr_create(argv[1],&cb,error,sizeof(error));
    CHECK(c);CHECK(dlms_client_sbkr_count(c)==25);CHECK(dlms_client_sbkr_interval_ms(c)==60000);
    CHECK(dlms_client_sbkr_point(c,0)->obis[0]==1);CHECK(dlms_client_sbkr_point(c,0)->obis[5]==255);
    CHECK(dlms_client_sbkr_point(c,25)==NULL);
    dlms_sbkr_poll_result_t r;CHECK(dlms_client_sbkr_poll(c,&r)==0);
    CHECK(r.good==25 && r.failed==0 && state.calls==25);
    const sbkr_data *samples=dlms_client_sbkr_samples(c);
    CHECK(samples[0].Value.Double==1234567890.125);CHECK(samples[0].Time>0);CHECK(samples[0].Sequence==1);
    state.fail=1;CHECK(dlms_client_sbkr_poll(c,&r)==1);CHECK(r.good==24 && r.failed==1);
    CHECK(samples[0].Quality==SBKR_QUALITY_COMM_FAILURE);CHECK(samples[0].Value.Double==1234567890.125);CHECK(samples[0].Sequence==2);
    state.fail=0;state.nan=1;CHECK(dlms_client_sbkr_poll(c,&r)==1);CHECK(r.failed==25);CHECK(samples[0].Quality==SBKR_QUALITY_INVALID);
    state.nan=0;state.range=1;CHECK(dlms_client_sbkr_poll(c,&r)==1);CHECK(r.failed==25);CHECK(samples[0].Quality==SBKR_QUALITY_INVALID);
    state.range=0;CHECK(dlms_client_sbkr_poll(c,&r)==0);CHECK(r.good==25);
    state.publish_fail=1;CHECK(dlms_client_sbkr_poll(c,&r)==-1);CHECK(r.publish_status==SBKR_STATUS_TIMEOUT);
    CHECK(strstr(dlms_client_sbkr_error(c),"publication failed"));
    dlms_client_sbkr_t *other=dlms_client_sbkr_create(argv[1],&cb,error,sizeof(error));CHECK(other);
    CHECK(dlms_client_sbkr_samples(other)[0].Quality==SBKR_QUALITY_NO_DATA);
    dlms_client_sbkr_destroy(&other);CHECK(!other);
    dlms_client_sbkr_destroy(&c);CHECK(!c);dlms_client_sbkr_destroy(&c);
    CHECK(!dlms_client_sbkr_create("/nonexistent/config.json",NULL,error,sizeof(error)));
    CHECK(strstr(error,"Cannot open"));
    dlms_sbkr_gateway_t *g=dlms_sbkr_gateway_open("/nonexistent/libsbkr.so","tcp://127.0.0.1:5555",100,error,sizeof(error));
    CHECK(!g);CHECK(strstr(error,"Cannot load"));
    puts("polling, quality, precision, callbacks, lifecycle, missing dependency: passed");return 0;
}
