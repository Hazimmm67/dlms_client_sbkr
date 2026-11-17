#include "internal.h"
#include "communication.h"
#include "client.h"
#include "cosem.h"
#include "variant.h"
#include "errorcodes.h"
#include <math.h>
#include <unistd.h>
/* The old bridge's com_close clears settings. Always recreate them on reconnect. */
void ds_transport_close(dlms_client_sbkr_t *c) {
    if(!c->initialized)return;
    if(c->conn.socket!=-1){close(c->conn.socket);c->conn.socket=-1;}
    con_close(&c->conn);
    cl_clear(&c->conn.settings);
    for(size_t i=0;i<c->count;i++)var_clear(&c->objects[i].value);
    c->initialized=0;c->connected=0;
}
int ds_transport_connect(dlms_client_sbkr_t *c) {
    int ret;ds_transport_close(c);
    con_init(&c->conn,GX_TRACE_LEVEL_OFF);c->initialized=1;
    c->conn.waitTime=c->timeout;c->conn.baudRate=c->baud;
    cl_init(&c->conn.settings,1,c->client_address,c->server_address,
            c->low_auth?DLMS_AUTHENTICATION_LOW:DLMS_AUTHENTICATION_NONE,
            c->low_auth?c->password:NULL,
            c->mode_e?DLMS_INTERFACE_TYPE_HDLC_WITH_MODE_E:DLMS_INTERFACE_TYPE_HDLC);
    for(size_t i=0;i<c->count;i++)cosem_init2((gxObject*)&c->objects[i],DLMS_OBJECT_TYPE_REGISTER,c->points[i].obis);
    ret=c->tcp?com_makeConnect(&c->conn,c->host,c->port,c->timeout):com_open(&c->conn,c->serial);
    if(!ret)ret=com_initializeConnection(&c->conn);
    if(ret){ds_transport_close(c);return ds_error(c,"DLMS connection/association failed: %d",ret);}
    c->connected=1;return 0;
}
static int numeric(DLMS_DATA_TYPE type) {
    switch(type){case DLMS_DATA_TYPE_INT8:case DLMS_DATA_TYPE_INT16:case DLMS_DATA_TYPE_INT32:
    case DLMS_DATA_TYPE_INT64:case DLMS_DATA_TYPE_UINT8:case DLMS_DATA_TYPE_UINT16:
    case DLMS_DATA_TYPE_UINT32:case DLMS_DATA_TYPE_UINT64:case DLMS_DATA_TYPE_FLOAT32:
    case DLMS_DATA_TYPE_FLOAT64:return 1;default:return 0;}
}
int ds_transport_read(dlms_client_sbkr_t *c,size_t i,double *value) {
    gxRegister *r=&c->objects[i];int ret=0;
    if(!r->unitRead) {
        ret=com_read(&c->conn,(gxObject*)r,3);
        if(ret && c->points[i].has_fallback_scaler &&
           (ret==DLMS_ERROR_CODE_READ_WRITE_DENIED || ret==DLMS_ERROR_CODE_UNDEFINED_OBJECT)) {
            r->scaler=c->points[i].fallback_scaler;r->unitRead=1;ret=0;
        }
    }
    if(!ret)ret=com_read(&c->conn,(gxObject*)r,2);
    if(!ret && !numeric(r->value.vt))ret=DLMS_ERROR_CODE_UNMATCH_TYPE;
    if(ret) {
        if(ret==DLMS_ERROR_CODE_READ_WRITE_DENIED)c->samples[i].Quality=SBKR_QUALITY_NO_ACCESS;
        else if(ret==DLMS_ERROR_CODE_UNDEFINED_OBJECT)c->samples[i].Quality=SBKR_QUALITY_NOT_FOUND;
        else if(ret==DLMS_ERROR_CODE_UNMATCH_TYPE)c->samples[i].Quality=SBKR_QUALITY_INVALID;
        ds_error(c,"Point %zu DLMS read failed: %d",i,ret);
        if((ret & DLMS_ERROR_TYPE_COMMUNICATION_ERROR) || ret==DLMS_ERROR_CODE_SEND_FAILED ||
           ret==DLMS_ERROR_CODE_RECEIVE_FAILED || ret==DLMS_ERROR_CODE_NOT_REPLY ||
           ret==DLMS_ERROR_CODE_INVALID_RESPONSE) ds_transport_close(c);
        return ret;
    }
    *value=var_toDouble(&r->value)*pow(10.0,r->scaler);return 0;
}
