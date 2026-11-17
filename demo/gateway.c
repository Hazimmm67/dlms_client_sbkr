#include "protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static cJSON *field(const cJSON *o,const char *name){return cJSON_GetObjectItemCaseSensitive(o,name);}
static cJSON *empty_store(void)
{
    cJSON *s=cJSON_CreateObject();
    if (!s || !cJSON_AddBoolToObject(s,"demo",1) || !cJSON_AddStringToObject(s,"protocol",DEMO_PROTOCOL) ||
        !cJSON_AddArrayToObject(s,"points") || !cJSON_AddArrayToObject(s,"logs") ||
        !cJSON_AddNumberToObject(s,"write_batches",0)) {cJSON_Delete(s);return NULL;}
    return s;
}
static int validate_store(cJSON *store)
{
    cJSON *points=field(store,"points"),*logs=field(store,"logs"),*batches=field(store,"write_batches");
    if (!demo_protocol_valid(store) || !cJSON_IsTrue(field(store,"demo")) || !cJSON_IsArray(points) ||
        cJSON_GetArraySize(points)>(int)DEMO_MAX_POINTS || !cJSON_IsArray(logs) || cJSON_GetArraySize(logs)>100 ||
        !cJSON_IsNumber(batches) || !isfinite(batches->valuedouble) || batches->valuedouble<0) return -1;
    for(cJSON *a=points->child;a;a=a->next) {
        sbkr_data p;if(demo_decode_point(a,&p))return -1;
        for(cJSON *b=a->next;b;b=b->next)if(field(b,"id") && field(b,"id")->valuedouble==p.Id)return -1;
    }
    return 0;
}
static cJSON *load_store(const char *path)
{
    FILE *f=fopen(path,"rb");
    if(!f)return errno==ENOENT?empty_store():NULL;
    long length;
    if(fseek(f,0,SEEK_END) || (length=ftell(f))<1 || length>4*DEMO_MAX_BYTES || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    char *text=malloc((size_t)length+1);
    if(!text){fclose(f);return NULL;}
    size_t n=fread(text,1,(size_t)length,f);fclose(f);text[n]=0;
    cJSON *s=n==(size_t)length?cJSON_ParseWithLengthOpts(text,n+1,NULL,1):NULL;free(text);
    if(!s || validate_store(s)){cJSON_Delete(s);return NULL;}
    return s;
}
static int save_store(cJSON *store,const char *path)
{
    char *text=cJSON_PrintUnformatted(store);
    char *temporary=malloc(strlen(path)+16);
    if(!text || !temporary){free(text);free(temporary);return -1;}
    sprintf(temporary,"%s.XXXXXX",path);
    int fd=mkstemp(temporary),ok=0;
    if(fd<0)goto done;
    FILE *f=fdopen(fd,"w");
    if(!f){close(fd);goto done;}
    ok=fputs(text,f)>=0 && fputc('\n',f)!=EOF && fflush(f)==0 && fsync(fd)==0;
    if(fclose(f))ok=0;
    if(ok && rename(temporary,path))ok=0;
done:
    if(!ok)unlink(temporary);
    free(text);free(temporary);return ok?0:-1;
}
static cJSON *find_point(cJSON *points,unsigned int id)
{
    for(cJSON *p=points->child;p;p=p->next)if(field(p,"id")->valuedouble==id)return p;
    return NULL;
}
static int valid_id(const cJSON *id)
{
    return cJSON_IsNumber(id) && isfinite(id->valuedouble) && id->valuedouble>=1 &&
        id->valuedouble<=UINT_MAX && floor(id->valuedouble)==id->valuedouble;
}
static sbkr_errorcode_t write_points(cJSON *next,const cJSON *request)
{
    cJSON *rows=field(request,"points"),*points=field(next,"points");
    if(!cJSON_IsArray(rows) || cJSON_GetArraySize(rows)<1 || cJSON_GetArraySize(rows)>(int)DEMO_MAX_POINTS)return SBKR_STATUS_BUFFER_EXCEEDED;
    for(cJSON *row=rows->child;row;row=row->next) {
        sbkr_data p;sbkr_errorcode_t status=demo_decode_point(row,&p);
        if(status)return status;
        for(cJSON *other=row->next;other;other=other->next)if(valid_id(field(other,"id")) && field(other,"id")->valuedouble==p.Id)return SBKR_STATUS_DUPLICATE;
        cJSON *copy=demo_encode_point(&p),*old=find_point(points,p.Id);
        if(!copy)return SBKR_STATUS_OUT_OF_MEMORY;
        if(old)cJSON_ReplaceItemViaPointer(points,old,copy);
        else {
            if(cJSON_GetArraySize(points)>=(int)DEMO_MAX_POINTS){cJSON_Delete(copy);return SBKR_STATUS_BUFFER_EXCEEDED;}
            cJSON_AddItemToArray(points,copy);
        }
    }
    cJSON_SetNumberValue(field(next,"write_batches"),field(next,"write_batches")->valuedouble+1);
    return SBKR_STATUS_OK;
}
static sbkr_errorcode_t add_log(cJSON *next,const cJSON *request)
{
    cJSON *source=field(request,"source"),*text=field(request,"message"),*level=field(request,"level");
    if(!cJSON_IsString(source) || !source->valuestring[0] || strlen(source->valuestring)>128 ||
        !cJSON_IsString(text) || strlen(text->valuestring)>4095 || !cJSON_IsNumber(level) ||
        level->valuedouble<0 || level->valuedouble>4 || floor(level->valuedouble)!=level->valuedouble)return SBKR_STATUS_BUFFER_MISMATCH;
    cJSON *log=cJSON_CreateObject(),*logs=field(next,"logs");
    if(!log || !cJSON_AddStringToObject(log,"source",source->valuestring) ||
        !cJSON_AddStringToObject(log,"message",text->valuestring) || !cJSON_AddNumberToObject(log,"level",level->valueint)) {
        cJSON_Delete(log);return SBKR_STATUS_OUT_OF_MEMORY;
    }
    if(cJSON_GetArraySize(logs)>=100)cJSON_DeleteItemFromArray(logs,0);
    cJSON_AddItemToArray(logs,log);return SBKR_STATUS_OK;
}
static cJSON *handle(cJSON **store,const cJSON *request,const char *path)
{
    cJSON *op=field(request,"op");
    if(!demo_protocol_valid(request) || !cJSON_IsString(op))return demo_reply(SBKR_STATUS_HEADER_FAILED);
    if(!strcmp(op->valuestring,"read")) {
        cJSON *ids=field(request,"ids"),*reply=demo_reply(SBKR_STATUS_OK),*out=cJSON_AddArrayToObject(reply,"points");
        sbkr_errorcode_t status=SBKR_STATUS_OK;
        if(!reply || !out){cJSON_Delete(reply);return demo_reply(SBKR_STATUS_OUT_OF_MEMORY);}
        if(!cJSON_IsArray(ids) || cJSON_GetArraySize(ids)<1 || cJSON_GetArraySize(ids)>(int)DEMO_MAX_POINTS)status=SBKR_STATUS_BUFFER_EXCEEDED;
        else for(cJSON *id=ids->child;id;id=id->next) {
            if(!valid_id(id)){status=SBKR_STATUS_MISMATCH_ID;break;}
            cJSON *point=find_point(field(*store,"points"),(unsigned int)id->valuedouble);
            if(!point){status=SBKR_STATUS_NOT_FOUND;break;}
            cJSON *copy=cJSON_Duplicate(point,1);
            if(!copy){status=SBKR_STATUS_OUT_OF_MEMORY;break;}
            cJSON_AddItemToArray(out,copy);
        }
        if(status){cJSON_Delete(reply);return demo_reply(status);}
        return reply;
    }
    if(strcmp(op->valuestring,"write") && strcmp(op->valuestring,"log"))return demo_reply(SBKR_STATUS_NOT_SUPPORTED);
    cJSON *next=cJSON_Duplicate(*store,1);
    if(!next)return demo_reply(SBKR_STATUS_OUT_OF_MEMORY);
    sbkr_errorcode_t status=!strcmp(op->valuestring,"write")?write_points(next,request):add_log(next,request);
    if(!status) {
        struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);
        char timestamp[32];snprintf(timestamp,sizeof(timestamp),"%llu",(unsigned long long)ts.tv_sec*1000+(unsigned long long)ts.tv_nsec/1000000);
        cJSON_DeleteItemFromObjectCaseSensitive(next,"updated_ms");
        if(!cJSON_AddStringToObject(next,"updated_ms",timestamp))status=SBKR_STATUS_OUT_OF_MEMORY;
        else if(save_store(next,path))status=SBKR_STATUS_DBUPDATE_FAILED;
    }
    if(!status){cJSON_Delete(*store);*store=next;}else cJSON_Delete(next);
    return demo_reply(status);
}
int main(int argc,char **argv)
{
    if(argc!=3 || !demo_endpoint_valid(argv[1])) {
        fprintf(stderr,"Usage: sbkr_demo_gateway tcp://127.0.0.1:PORT /path/to/store.json\n");return 2;
    }
    /* One gateway owns a snapshot file. Refuse another writer to that file. */
    char *lock_path=malloc(strlen(argv[2])+6);
    if(!lock_path)return 2;
    sprintf(lock_path,"%s.lock",argv[2]);
    int lock=open(lock_path,O_RDWR|O_CREAT,0600);free(lock_path);
    struct flock owner={.l_type=F_WRLCK,.l_whence=SEEK_SET};
    if(lock<0 || fcntl(lock,F_SETLK,&owner)<0){fprintf(stderr,"Demo store is locked or unavailable\n");if(lock>=0)close(lock);return 2;}
    cJSON *store=load_store(argv[2]);
    if(!store){fprintf(stderr,"Cannot load demo store; existing contents were not overwritten\n");close(lock);return 2;}
    zsock_t *socket=zsock_new(ZMQ_REP);
    if(!socket){cJSON_Delete(store);close(lock);return 2;}
    zsock_set_linger(socket,0);zsock_set_rcvtimeo(socket,250);zsock_set_sndtimeo(socket,1000);
    zsock_set_maxmsgsize(socket,DEMO_MAX_BYTES);
    if(zsock_bind(socket,"%s",argv[1])<0){fprintf(stderr,"Cannot bind demo gateway\n");zsock_destroy(&socket);cJSON_Delete(store);close(lock);return 2;}
    printf("DEMO SBKR gateway listening on %s; snapshot %s; protocol %s\n",argv[1],argv[2],DEMO_PROTOCOL);fflush(stdout);
    while(!zsys_interrupted) {
        zmsg_t *message=zmsg_recv(socket);
        if(!message)continue;
        cJSON *request=NULL;
        if(zmsg_size(message)==1) {
            zframe_t *frame=zmsg_first(message);
            size_t size=zframe_size(frame);
            char *text=zmsg_popstr(message);
            if(text && strlen(text)==size && size<=DEMO_MAX_BYTES)request=cJSON_ParseWithLengthOpts(text,size+1,NULL,1);
            free(text);
        }
        zmsg_destroy(&message);
        cJSON *reply=handle(&store,request,argv[2]);cJSON_Delete(request);
        char *wire=reply?cJSON_PrintUnformatted(reply):NULL;cJSON_Delete(reply);
        if(!wire)wire=strdup("{\"protocol\":\"sbkr-demo/1\",\"status\":16}");
        if(!wire || zstr_send(socket,wire)){free(wire);break;}
        free(wire);
    }
    zsock_destroy(&socket);cJSON_Delete(store);close(lock);return 0;
}
