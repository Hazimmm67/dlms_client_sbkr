#ifndef DLMS_SBKR_INTERNAL_H
#define DLMS_SBKR_INTERNAL_H
#include "dlms_client_sbkr.h"
#include "connection.h"
#include "gxobjects.h"
struct dlms_client_sbkr {
    dlms_sbkr_point_t *points;
    sbkr_data *samples;
    gxRegister *objects;
    size_t count;
    dlms_sbkr_callbacks_t cb;
    char serial[256], host[256], password[128], error[256];
    char gateway[256], library[512], logger_endpoint[256];
    unsigned int baud, interval, timeout, client_address, server_address, port;
    int tcp, mode_e, low_auth, initialized, connected;
    connection conn;
};
int ds_config_load(dlms_client_sbkr_t *c, const char *path);
int ds_transport_connect(dlms_client_sbkr_t *c);
void ds_transport_close(dlms_client_sbkr_t *c);
int ds_transport_read(dlms_client_sbkr_t *c, size_t index, double *value);
int ds_error(dlms_client_sbkr_t *c, const char *format, ...);
#endif
