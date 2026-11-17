#include "sbkr_client.h"
#include "sbkr_logger.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Demo API failure line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int raw_rejection(const char *endpoint)
{
    zsock_t *s=zsock_new(ZMQ_REQ);CHECK(s);
    zsock_set_linger(s,0);zsock_set_rcvtimeo(s,1000);
    CHECK(zsock_connect(s,"%s",endpoint)==0);
    CHECK(zstr_send(s,"{\"protocol\":\"production\",\"op\":\"write\"}")==0);
    char *reply=zstr_recv(s);CHECK(reply);CHECK(strstr(reply,"\"status\":13"));free(reply);
    const char *bad="{\"protocol\":\"sbkr-demo/1\",\"op\":\"write\",\"points\":["
        "{\"id\":42,\"type\":\"double\",\"quality\":2,\"value\":999,\"sequence\":\"1\",\"time_ms\":\"1\"},"
        "{\"id\":43,\"type\":\"string\",\"quality\":2,\"value\":1,\"sequence\":\"1\",\"time_ms\":\"1\"}]}";
    CHECK(zstr_send(s,bad)==0);reply=zstr_recv(s);CHECK(reply);CHECK(strstr(reply,"\"status\":23"));free(reply);
    zsock_destroy(&s);return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc==3);sbkr_client_t *c=sbkr_data_client_new(argv[1],0);CHECK(c);
    sbkr_client_set_timeout(c,150);
    sbkr_data p={.Id=42,.Type=SBKR_TYPE_DOUBLE,.Quality=SBKR_QUALITY_GOOD,
        .Sequence=UINT64_MAX,.Time=UINT64_MAX,.Value.Double=1234567890.125};
    if(!strcmp(argv[2],"--timeout")) {
        CHECK(sbkr_write_data(c,1,&p)==SBKR_STATUS_TIMEOUT);
        CHECK(sbkr_write_data(c,1,&p)==SBKR_STATUS_TIMEOUT);
        sbkr_client_destroy(&c);return 0;
    }
    if(!strcmp(argv[2],"--recover")) {
        CHECK(sbkr_write_data(c,1,&p)==SBKR_STATUS_TIMEOUT);
        puts("timeout observed");fflush(stdout);CHECK(getchar()=='\n');
        sbkr_client_set_timeout(c,1000);
        CHECK(sbkr_write_data(c,1,&p)==SBKR_STATUS_OK);
        sbkr_client_destroy(&c);return 0;
    }
    if(!strcmp(argv[2],"--write-fail")) {
        CHECK(sbkr_write_data(c,1,&p)==SBKR_STATUS_DBUPDATE_FAILED);
        sbkr_data read={.Id=42};CHECK(sbkr_read_data(c,1,&read)==SBKR_STATUS_NOT_FOUND);
        sbkr_client_destroy(&c);return 0;
    }
    if(!strcmp(argv[2],"--verify")) {
        sbkr_data read={.Id=42};CHECK(sbkr_read_data(c,1,&read)==0);
        CHECK(read.Value.Double==p.Value.Double && read.Sequence==UINT64_MAX && read.Time==UINT64_MAX);
        sbkr_client_destroy(&c);return 0;
    }
    CHECK(!strcmp(argv[2],"--suite"));
    CHECK(sbkr_write_data(c,1,&p)==0);
    sbkr_data read={.Id=42};CHECK(sbkr_read_data(c,1,&read)==0);
    CHECK(read.Value.Double==p.Value.Double && read.Sequence==p.Sequence && read.Time==p.Time && read.Quality==p.Quality);
    CHECK(raw_rejection(argv[1])==0);
    CHECK(sbkr_read_data(c,1,&read)==0 && read.Value.Double==p.Value.Double);
    sbkr_data bad=p;bad.Type=SBKR_TYPE_STRING;CHECK(sbkr_write_data(c,1,&bad)==SBKR_STATUS_NOT_SUPPORTED);
    bad=p;bad.Value.Double=NAN;CHECK(sbkr_write_data(c,1,&bad)==SBKR_STATUS_BUFFER_MISMATCH);
    bad=p;bad.Id=0;CHECK(sbkr_write_data(c,1,&bad)==SBKR_STATUS_BUFFER_MISMATCH);
    sbkr_data duplicates[]={p,p};CHECK(sbkr_write_data(c,2,duplicates)==SBKR_STATUS_DUPLICATE);
    sbkr_data missing={.Id=999};CHECK(sbkr_read_data(c,1,&missing)==SBKR_STATUS_NOT_FOUND);
    sbkr_data two[]={ {.Id=42,.Value.Double=-99}, {.Id=999} };
    CHECK(sbkr_read_data(c,2,two)==SBKR_STATUS_NOT_FOUND && two[0].Value.Double==-99);
    sbkr_data second=p;second.Id=43;second.Value.Double=.95;sbkr_data *ptrs[]={&p,&second};
    CHECK(sbkr_write_data_ptr(c,2,ptrs)==0);
    second.Value.Double=0;CHECK(sbkr_read_data_ptr(c,2,ptrs)==0 && second.Value.Double==.95);
    CHECK(sbkr_write_data(c,0,&p)==SBKR_STATUS_BUFFER_EXCEEDED);
    CHECK(sbkr_read_data(NULL,1,&p)==SBKR_STATUS_NULL_POINTER);
    CHECK(sbkr_subscribe_data(c,1,&p)==SBKR_STATUS_NOT_SUPPORTED);
    CHECK(!sbkr_data_client_new("tcp://example.com:5555",0));
    sbkr_logger_t *logger=sbkr_logger_open("api-test",argv[1]);CHECK(logger);
    uint64_t ticket=0;CHECK(sbkr_logger_log_async(logger,SBKR_LOG_INFO,&ticket,"Literal %s","100% good")==0 && ticket);
    CHECK(sbkr_logger_log_async(logger,SBKR_LOG_INFO,NULL,"backpressure")==-1);
    sbkr_log_confirmation_t ack;
    CHECK(sbkr_logger_receive(logger,1000,&ack)==0 && ack.ticket==ticket && ack.status==SBKR_LOG_DELIVERY_RETAINED);
    CHECK(sbkr_logger_receive(logger,0,&ack)==SBKR_LOG_RECEIVE_TIMEOUT);
    sbkr_logger_close(&logger);CHECK(!logger);
    sbkr_client_destroy(&c);CHECK(!c);return 0;
}
