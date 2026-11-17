/* Local development meter: exposes the JSON point database as COSEM Registers.
   This is a DLMS HDLC-over-TCP simulator, not an SBKR gateway. */
#include "internal.h"
#include "server.h"
#include "cosem.h"
#include "variant.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static volatile sig_atomic_t stopped;
static unsigned int server_address,client_address;
static void stop_server(int sig){(void)sig;stopped=1;}
uint32_t time_current(void){return (uint32_t)time(NULL);}
uint32_t time_elapsed(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (uint32_t)((uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000);}
unsigned char svr_isTarget(dlmsSettings *s,uint32_t server,uint32_t client){(void)s;return server==server_address && client==client_address;}
DLMS_ACCESS_MODE svr_getAttributeAccess(dlmsSettings *s,gxObject *o,unsigned char i){(void)s;(void)o;(void)i;return DLMS_ACCESS_MODE_READ;}
DLMS_METHOD_ACCESS_MODE svr_getMethodAccess(dlmsSettings *s,gxObject *o,unsigned char i){(void)s;(void)o;(void)i;return DLMS_METHOD_ACCESS_MODE_NONE;}
int svr_connected(dlmsServerSettings *s){(void)s;return 0;}
int svr_disconnected(dlmsServerSettings *s){(void)s;return 0;}
int svr_invalidConnection(dlmsServerSettings *s){(void)s;return 0;}
DLMS_SOURCE_DIAGNOSTIC svr_validateAuthentication(dlmsServerSettings *s,DLMS_AUTHENTICATION a,gxByteBuffer *p){(void)s;(void)p;return a==DLMS_AUTHENTICATION_NONE?DLMS_SOURCE_DIAGNOSTIC_NONE:DLMS_SOURCE_DIAGNOSTIC_AUTHENTICATION_FAILURE;}
int svr_findObject(dlmsSettings *s,DLMS_OBJECT_TYPE t,int sn,unsigned char *ln,gxValueEventArg *e){(void)s;(void)t;(void)sn;(void)ln;(void)e;return 0;}
#define NOOP(name) void name(dlmsSettings *s,gxValueEventCollection *a){(void)s;(void)a;}
NOOP(svr_preGet) NOOP(svr_postGet) NOOP(svr_preRead) NOOP(svr_postRead)
NOOP(svr_preWrite) NOOP(svr_postWrite) NOOP(svr_preAction) NOOP(svr_postAction) NOOP(svr_getDataType)
void svr_trace(const char *a,const char *b){(void)a;(void)b;}
int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"Usage: dlms_meter_simulator config.json (TCP/none authentication only)\n");return 2;}
    char error[256];dlms_client_sbkr_t *config=dlms_client_sbkr_create(argv[1],NULL,error,sizeof(error));
    if(!config){fprintf(stderr,"%s\n",error);return 2;}
    if(!config->tcp || config->low_auth || strcmp(config->host,"127.0.0.1")) {
        fprintf(stderr,"Simulator requires connectivity.connection_type=TCP, dlms.host=127.0.0.1 and authentication=none\n");dlms_client_sbkr_destroy(&config);return 2;
    }
    server_address=config->server_address;client_address=config->client_address;
    dlmsServerSettings settings;unsigned char frame[2048],pdu[2048];gxByteBuffer reply;
    gxAssociationLogicalName association;
    gxObject **objects=calloc(config->count+1,sizeof(*objects));
    gxRegister *regs=calloc(config->count,sizeof(*regs));
    if(!objects||!regs){free(objects);free(regs);dlms_client_sbkr_destroy(&config);return 2;}
    svr_init(&settings,1,DLMS_INTERFACE_TYPE_HDLC,1024,1024,frame,sizeof(frame),pdu,sizeof(pdu));
    bb_init(&reply);
    gxIecHdlcSetup hdlc;
    cosem_init2((gxObject*)&hdlc,DLMS_OBJECT_TYPE_IEC_HDLC_SETUP,(unsigned char[]){0,0,22,0,0,255});
    hdlc.maximumInfoLengthTransmit=hdlc.maximumInfoLengthReceive=128;
    hdlc.windowSizeReceive=hdlc.windowSizeTransmit=1;settings.hdlc=&hdlc;
    cosem_init2((gxObject*)&association,DLMS_OBJECT_TYPE_ASSOCIATION_LOGICAL_NAME,(unsigned char[]){0,0,40,0,1,255});
    association.clientSAP=client_address;association.authenticationMechanismName.mechanismId=DLMS_AUTHENTICATION_NONE;
    association.xDLMSContextInfo.maxSendPduSize=association.xDLMSContextInfo.maxReceivePduSize=1024;
    association.xDLMSContextInfo.conformance=DLMS_CONFORMANCE_GET;
    objects[0]=(gxObject*)&association;
    for(size_t i=0;i<config->count;i++) {
        cosem_init2((gxObject*)&regs[i],DLMS_OBJECT_TYPE_REGISTER,config->points[i].obis);
        regs[i].scaler=0;regs[i].unit=0;var_setDouble(&regs[i].value,config->points[i].simulator_value);objects[i+1]=(gxObject*)&regs[i];
    }
    oa_attach(&settings.base.objects,objects,(uint16_t)(config->count+1));settings.base.objects.size=(uint16_t)(config->count+1);
    oa_attach(&association.objectList,objects,(uint16_t)(config->count+1));association.objectList.size=(uint16_t)(config->count+1);
    int rc=svr_initialize(&settings),listener=-1,peer=-1;
    if(rc){fprintf(stderr,"Simulator initialization failed: %d\n",rc);goto cleanup;}
    listener=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(config->port)};address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    if(listener<0 || bind(listener,(struct sockaddr*)&address,sizeof(address)) || listen(listener,1)){perror("Simulator socket");rc=1;goto cleanup;}
    struct sigaction action={0};action.sa_handler=stop_server;sigaction(SIGINT,&action,NULL);sigaction(SIGTERM,&action,NULL);signal(SIGPIPE,SIG_IGN);
    printf("SIMULATED meter: 127.0.0.1:%u, %zu JSON registers, values from JSON (default 1.0)\n",config->port,config->count);fflush(stdout);
    while(!stopped) {
        peer=accept(listener,NULL,NULL);
        if(peer<0){if(errno==EINTR)continue;rc=1;break;}
        struct timeval timeout={1,0};setsockopt(peer,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        svr_reset(&settings);
        while(!stopped) {
            unsigned char data[2048];ssize_t n=recv(peer,data,sizeof(data),0);
            if(n<0 && (errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
            if(n<=0)break;
            if(svr_handleRequest2(&settings,data,(uint16_t)n,&reply))break;
            size_t sent=0;
            while(sent<reply.size) {
                ssize_t wrote=send(peer,reply.data+sent,reply.size-sent,MSG_NOSIGNAL);
                if(wrote<0 && errno==EINTR)continue;
                if(wrote<=0)break;
                sent+=(size_t)wrote;
            }
            if(sent<reply.size)break;
            bb_clear(&reply);
        }
        close(peer);peer=-1;
    }
cleanup:
    if(peer>=0)close(peer);
    if(listener>=0)close(listener);
    bb_clear(&reply);
    /* Arrays and objects are caller-owned; detach before library cleanup. */
    settings.base.objects.size=0;association.objectList.size=0;
    svr_clear(&settings);
    for(size_t i=0;i<config->count;i++)var_clear(&regs[i].value);
    free(regs);free(objects);dlms_client_sbkr_destroy(&config);return rc?1:0;
}
