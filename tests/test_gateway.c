#include "dlms_sbkr_gateway.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Gateway check failed at %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);char error[256];
    dlms_sbkr_gateway_t *g=dlms_sbkr_gateway_open(argv[1],"test://unit",123,error,sizeof(error));
    CHECK(g);sbkr_data point={.Id=42,.Type=SBKR_TYPE_DOUBLE,.Quality=SBKR_QUALITY_GOOD,.Value.Double=1.25};
    CHECK(dlms_sbkr_gateway_publish(g,1,&point)==SBKR_STATUS_OK);
    point.Id=43;CHECK(dlms_sbkr_gateway_publish(g,1,&point)==SBKR_STATUS_MISMATCH_ID);
    CHECK(dlms_sbkr_gateway_enable_logger(g,"test://log")==0);
    dlms_sbkr_gateway_log(g,"test %s remains literal");
    dlms_sbkr_gateway_close(&g);CHECK(!g);
    g=dlms_sbkr_gateway_open(argv[1],"test://bad",123,error,sizeof(error));CHECK(!g);
    g=dlms_sbkr_gateway_open(argv[1],"test://unit",1,error,sizeof(error));CHECK(g);
    CHECK(dlms_sbkr_gateway_publish(g,1,&point)==SBKR_STATUS_TIMEOUT);
    dlms_sbkr_gateway_close(&g);return 0;
}
