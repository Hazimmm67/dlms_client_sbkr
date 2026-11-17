#include "internal.h"
#include "dlms_sbkr_gateway.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
static volatile sig_atomic_t stop;
static void interrupted(int sig){(void)sig;stop=1;}
static void log_stderr(void *user,const char *message){(void)user;fprintf(stderr,"%s\n",message);}
static int simulate(void *arg,const dlms_sbkr_point_t *p,double *value){(void)arg;(void)p;*value=1.0;return 0;}
static void print_samples(dlms_client_sbkr_t *c,int simulation) {
    cJSON *root=cJSON_CreateObject(),*arr=cJSON_AddArrayToObject(root,"points");
    cJSON_AddBoolToObject(root,"simulated",simulation);
    const sbkr_data *values=dlms_client_sbkr_samples(c);
    for(size_t i=0;i<dlms_client_sbkr_count(c);i++) {
        const dlms_sbkr_point_t *p=dlms_client_sbkr_point(c,i);cJSON *row=cJSON_CreateObject();
        cJSON_AddItemToArray(arr,row);cJSON_AddStringToObject(row,"description",p->description);
        cJSON_AddNumberToObject(row,"sbkr_id",values[i].Id);cJSON_AddNumberToObject(row,"quality",values[i].Quality);
        cJSON_AddNumberToObject(row,"time_ms",(double)values[i].Time);cJSON_AddNumberToObject(row,"value",values[i].Value.Double);
    }
    char *json=cJSON_PrintUnformatted(root);if(json){puts(json);free(json);}cJSON_Delete(root);fflush(stdout);
}
int main(int argc,char **argv) {
    const char *path=NULL,*library=NULL,*endpoint=NULL;int once=0,check=0,simulation=0,exit_status=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--once"))once=1;
        else if(!strcmp(argv[i],"--check"))check=1;
        else if(!strcmp(argv[i],"--simulate"))simulation=1;
        else if(!strcmp(argv[i],"--sbkr-library") && i+1<argc)library=argv[++i];
        else if(!strcmp(argv[i],"--gateway") && i+1<argc)endpoint=argv[++i];
        else if(argv[i][0]!='-' && !path)path=argv[i];
        else{fprintf(stderr,"Unknown/incomplete argument: %s\n",argv[i]);return 2;}
    }
    if(!path){fprintf(stderr,"Usage: dlms_client_sbkr config.json [--check | --simulate] [--once]\n  [--sbkr-library /path/libsbkr.so --gateway tcp://host:port]\n");return 2;}
    char error[256];dlms_sbkr_callbacks_t cb={0};cb.log=log_stderr;
    if(simulation)cb.read=simulate;
    dlms_client_sbkr_t *c=dlms_client_sbkr_create(path,&cb,error,sizeof(error));
    if(!c){fprintf(stderr,"%s\n",error);return 2;}
    dlms_sbkr_gateway_t *gateway=NULL;
    if(check) {
        printf("Valid configuration: %zu points, poll interval %u ms; no connections opened.\n",c->count,c->interval);
        for(size_t i=0;i<c->count;i++)printf("%s -> %u (CASDU %u, IOA %u)%s\n",c->points[i].description,c->points[i].sbkr_id,c->points[i].casdu,c->points[i].ioa,c->points[i].has_id?"":" [no SBKR ID]");
        goto cleanup;
    }
    if(!endpoint)endpoint=c->gateway;
    if(!library)library=c->library;
    if(simulation && *endpoint){fprintf(stderr,"Simulation is console-only. Remove the configured gateway to prevent publishing synthetic data.\n");exit_status=2;goto cleanup;}
    if(*endpoint) {
        for(size_t i=0;i<c->count;i++)if(!c->points[i].has_id){fprintf(stderr,"Explicit sbkr_id required for every published point.\n");exit_status=2;goto cleanup;}
        gateway=dlms_sbkr_gateway_open(library,endpoint,(int)c->timeout,error,sizeof(error));
        if(!gateway){fprintf(stderr,"%s\n",error);exit_status=2;goto cleanup;}
        c->cb.publish=dlms_sbkr_gateway_publish;c->cb.publish_user=gateway;
        if(c->logger_endpoint[0]){
            if(dlms_sbkr_gateway_enable_logger(gateway,c->logger_endpoint)){fprintf(stderr,"SBKR logger unavailable\n");exit_status=2;goto cleanup;}
            c->cb.log=dlms_sbkr_gateway_log;c->cb.log_user=gateway;
        }
    } else if(*library){fprintf(stderr,"SBKR library supplied without a gateway endpoint\n");exit_status=2;goto cleanup;}
    signal(SIGINT,interrupted);signal(SIGTERM,interrupted);
    do {
        dlms_sbkr_poll_result_t r;int rc=dlms_client_sbkr_poll(c,&r);
        print_samples(c,simulation);
        if(c->cb.publish)fprintf(stderr,"%zu good, %zu failed, SBKR status %d\n",r.good,r.failed,r.publish_status);
        else fprintf(stderr,"%zu good, %zu failed; console output, SBKR disabled\n",r.good,r.failed);
        if(once){exit_status=rc?1:0;break;}
        struct timespec remaining={c->interval/1000,(long)(c->interval%1000)*1000000L};
        while(!stop && nanosleep(&remaining,&remaining) && errno==EINTR){}
    }while(!stop);
cleanup:
    dlms_client_sbkr_destroy(&c);dlms_sbkr_gateway_close(&gateway);return exit_status;
}
