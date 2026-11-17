#include "internal.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#define GET(o,k) cJSON_GetObjectItemCaseSensitive(o,k)
static int string_field(dlms_client_sbkr_t *c, cJSON *o, const char *key,
                        char *dst, size_t n, int required) {
    cJSON *v=GET(o,key);
    if (!v && !required) return 0;
    if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring)>=n)
        return ds_error(c,"Invalid or missing string: %s",key);
    strcpy(dst,v->valuestring); return 0;
}
static int number_field(dlms_client_sbkr_t *c, cJSON *o, const char *key,
                        unsigned int *dst, double lo, double hi, int required) {
    cJSON *v=GET(o,key);
    if (!v && !required) return 0;
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
        v->valuedouble<lo || v->valuedouble>hi || floor(v->valuedouble)!=v->valuedouble)
        return ds_error(c,"Invalid or missing integer: %s",key);
    *dst=(unsigned int)v->valuedouble;return 0;
}
static int boolean_field(dlms_client_sbkr_t *c,cJSON *o,const char *key,int *dst) {
    cJSON *v=GET(o,key);if(!v)return 0;
    if(!cJSON_IsBool(v))return ds_error(c,"Expected boolean: %s",key);
    *dst=cJSON_IsTrue(v);return 0;
}
/* Both document shorthand A-B:C.D and full A.B.C.D.E.F. */
static int obis_parse(const char *text,unsigned char out[6]) {
    const int short_form=strchr(text,':')!=NULL;
    const char *separators=short_form?"-:.":".....";
    unsigned int count=short_form?4:6;
    out[4]=0;out[5]=255;
    for(unsigned int i=0;i<count;i++) {
        char *end;unsigned long n;
        if(!isdigit((unsigned char)*text))return -1;
        errno=0;n=strtoul(text,&end,10);
        if(errno || n>255)return -1;
        out[i]=(unsigned char)n;
        if(i+1<count){if(*end!=separators[i])return -1;text=end+1;}
        else if(*end)return -1;
    }
    return 0;
}
static int no_duplicate_keys(const cJSON *o) {
    if(cJSON_IsObject(o))for(const cJSON *a=o->child;a;a=a->next)
        for(const cJSON *b=a->next;b;b=b->next)
            if(!strcmp(a->string,b->string))return 0;
    for(const cJSON *a=o->child;a;a=a->next)if(!no_duplicate_keys(a))return 0;
    return 1;
}
int ds_config_load(dlms_client_sbkr_t *c,const char *path) {
    FILE *f=fopen(path,"rb");char *text=NULL;long len;cJSON *root=NULL,*conn,*serial,*dlms,*arr,*v;int rc=-1;
    c->baud=9600;c->interval=60000;c->timeout=3000;c->client_address=16;c->server_address=1;
    strcpy(c->serial,"/dev/ttyUSB0");
    if(!f)return ds_error(c,"Cannot open configuration: %s",path);
    if(fseek(f,0,SEEK_END) || (len=ftell(f))<1 || len>1024*1024 || fseek(f,0,SEEK_SET)) {
        fclose(f);return ds_error(c,"Configuration must contain 1 byte to 1 MiB");
    }
    text=malloc((size_t)len+1);
    if(!text){fclose(f);return ds_error(c,"Out of memory");}
    if(fread(text,1,(size_t)len,f)!=(size_t)len){fclose(f);free(text);return ds_error(c,"Cannot read configuration");}
    fclose(f);text[len]=0;root=cJSON_ParseWithLengthOpts(text,(size_t)len+1,NULL,1);free(text);
    if(!cJSON_IsObject(root) || !no_duplicate_keys(root)){ds_error(c,"Invalid JSON or duplicate keys");goto done;}
    conn=GET(root,"connectivity");serial=GET(conn,"serial_settings");dlms=GET(root,"dlms");
    if(!cJSON_IsObject(conn)){ds_error(c,"Missing connectivity object");goto done;}
    if(dlms && !cJSON_IsObject(dlms)){ds_error(c,"dlms must be an object");goto done;}
    v=GET(conn,"connection_type");
    if(!cJSON_IsString(v) || (strcmp(v->valuestring,"Serial") && strcmp(v->valuestring,"TCP"))) {
        ds_error(c,"connection_type must be Serial or TCP");goto done;
    }
    c->tcp=!strcmp(v->valuestring,"TCP");
    if(!c->tcp) {
        unsigned int bits=8,stop=1;
        if(!cJSON_IsObject(serial) || number_field(c,serial,"baud_rate",&c->baud,300,115200,1) ||
           number_field(c,serial,"data_bits",&bits,8,8,1) || number_field(c,serial,"stop_bits",&stop,1,1,1))goto done;
        v=GET(serial,"parity");if(!cJSON_IsString(v)||strcmp(v->valuestring,"None")){ds_error(c,"Only serial 8N1 is supported");goto done;}
        switch(c->baud){case 300:case 600:case 1200:case 2400:case 4800:case 9600:case 19200:case 38400:case 57600:case 115200:break;default:ds_error(c,"Unsupported baud rate");goto done;}
    }
    v=GET(conn,"polling_frequency");
    if(v) {
        unsigned long n;char *unit;unsigned int mul;
        if(!cJSON_IsString(v)||!isdigit((unsigned char)v->valuestring[0])){ds_error(c,"Invalid polling_frequency");goto done;}
        errno=0;n=strtoul(v->valuestring,&unit,10);
        mul=!strcmp(unit,"ms")?1:!strcmp(unit,"s")?1000:!strcmp(unit,"min")?60000:0;
        if(errno || !mul || n<1 || n>86400000U/mul){ds_error(c,"Polling interval must be 1 ms to 24 h");goto done;}
        c->interval=(unsigned int)n*mul;
    }
    if(number_field(c,conn,"station_id",&c->server_address,1,UINT_MAX,0))goto done;
    if(dlms) {
        if(string_field(c,dlms,"serial_port",c->serial,sizeof(c->serial),0) ||
           string_field(c,dlms,"host",c->host,sizeof(c->host),c->tcp) ||
           number_field(c,dlms,"port",&c->port,1,65535,c->tcp) ||
           number_field(c,dlms,"client_address",&c->client_address,1,65535,0) ||
           number_field(c,dlms,"server_address",&c->server_address,1,UINT_MAX,0) ||
           number_field(c,dlms,"timeout_ms",&c->timeout,1,60000,0) ||
           boolean_field(c,dlms,"iec_mode_e",&c->mode_e))goto done;
        v=GET(dlms,"authentication");
        if(v){if(!cJSON_IsString(v)||(strcmp(v->valuestring,"none")&&strcmp(v->valuestring,"low"))){ds_error(c,"Authentication must be none or low");goto done;}c->low_auth=!strcmp(v->valuestring,"low");}
        if(c->low_auth && string_field(c,dlms,"password",c->password,sizeof(c->password),1))goto done;
    }
    if(c->tcp && (!c->host[0] || !c->port || c->mode_e)){ds_error(c,"TCP requires dlms.host/port and iec_mode_e=false");goto done;}
    cJSON *sbkr=GET(root,"sbkr");
    if(sbkr) {
        if(!cJSON_IsObject(sbkr)){ds_error(c,"sbkr must be an object");goto done;}
        const char *keys[]={"gateway","library","logger_endpoint"};
        char *dest[]={c->gateway,c->library,c->logger_endpoint};
        size_t sizes[]={sizeof(c->gateway),sizeof(c->library),sizeof(c->logger_endpoint)};
        for(int k=0;k<3;k++) {
            v=GET(sbkr,keys[k]);if(!v)continue;
            if(!cJSON_IsString(v)||strlen(v->valuestring)>=sizes[k]){ds_error(c,"Invalid sbkr.%s",keys[k]);goto done;}
            strcpy(dest[k],v->valuestring);
        }
    }
    arr=GET(root,"io_points");
    if(!cJSON_IsArray(arr) || cJSON_GetArraySize(arr)<1 || cJSON_GetArraySize(arr)>4096){ds_error(c,"io_points must contain 1 to 4096 points");goto done;}
    c->count=(size_t)cJSON_GetArraySize(arr);
    c->points=calloc(c->count,sizeof(*c->points));c->samples=calloc(c->count,sizeof(*c->samples));c->objects=calloc(c->count,sizeof(*c->objects));
    if(!c->points||!c->samples||!c->objects){ds_error(c,"Out of memory");goto done;}
    for(size_t i=0;i<c->count;i++) {
        cJSON *row=cJSON_GetArrayItem(arr,(int)i);dlms_sbkr_point_t *p=&c->points[i];char obis[64],type[8];
        if(!cJSON_IsObject(row)){ds_error(c,"Point must be an object");goto done;}
        if(string_field(c,row,"description",p->description,sizeof(p->description),1) ||
           string_field(c,row,"obis_code",obis,sizeof(obis),1)||
           string_field(c,row,"data_type",type,sizeof(type),1)||
           number_field(c,row,"casdu",&p->casdu,0,65535,1)||
           number_field(c,row,"ioa",&p->ioa,0,16777215,0))goto done;
        if(strcmp(type,"AI") || obis_parse(obis,p->obis)){ds_error(c,"Point %zu: only AI and valid OBIS supported",i);goto done;}
        p->has_id=GET(row,"sbkr_id")!=NULL;
        if(number_field(c,row,"sbkr_id",&p->sbkr_id,1,UINT_MAX,0))goto done;
        if(c->cb.publish && !p->has_id){ds_error(c,"Point %zu requires explicit sbkr_id for publication",i);goto done;}
        cJSON *lo=GET(row,"low"),*hi=GET(row,"high");
        if(lo||hi){if(!cJSON_IsNumber(lo)||!cJSON_IsNumber(hi)||!isfinite(lo->valuedouble)||!isfinite(hi->valuedouble)||lo->valuedouble>hi->valuedouble){ds_error(c,"Point %zu: invalid range",i);goto done;}p->has_range=1;p->low=lo->valuedouble;p->high=hi->valuedouble;}
        p->simulator_value=1.0;
        v=GET(row,"simulator_value");
        if(v){if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)){ds_error(c,"Invalid simulator_value");goto done;}p->simulator_value=v->valuedouble;}
        v=GET(row,"fallback_scaler");if(v){if(!cJSON_IsNumber(v)||v->valuedouble<-12||v->valuedouble>12||floor(v->valuedouble)!=v->valuedouble){ds_error(c,"Invalid fallback_scaler");goto done;}p->has_fallback_scaler=1;p->fallback_scaler=v->valueint;}
        for(size_t j=0;j<i;j++)if(!memcmp(p->obis,c->points[j].obis,6)||(p->has_id && c->points[j].has_id && p->sbkr_id==c->points[j].sbkr_id)){ds_error(c,"Duplicate OBIS or sbkr_id at point %zu",i);goto done;}
        c->samples[i].Id=p->sbkr_id;c->samples[i].Type=SBKR_TYPE_DOUBLE;c->samples[i].Quality=SBKR_QUALITY_NO_DATA;
    }
    rc=0;
done:
    if(rc && !c->error[0])ds_error(c,"Invalid configuration");
    cJSON_Delete(root);return rc;
}
