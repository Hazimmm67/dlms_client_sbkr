/*  =========================================================================
    sbkr_data.h - SBKR canonical data types
    -------------------------------------------------------------------------
    Single source of truth for the data point type, quality, value, and error
    enumerations shared by the client, gateway, and store.
    =========================================================================
*/

#ifndef __SBKR_DATA_H_INCLUDED__
#define __SBKR_DATA_H_INCLUDED__

#include <czmq.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//  Helper macros for UTC and local time conversion
typedef unsigned long long          sbkr_time_t;

#define SecMsecToTime(_Sec, _Msec)  ((sbkr_time_t) (_Sec) * 1000 + (_Msec))
#define TimeToSec(_Time)            ((_Time) / 1000)
#define TimeToMSec(_Time)           ((_Time) % 1000)

//  All possible errors returned by the API functions.
typedef enum
{
    SBKR_STATUS_OK = 0,
    SBKR_STATUS_FAILED,
    SBKR_STATUS_DBOPEN_FAILED,
    SBKR_STATUS_DBDROP_FAILED,
    SBKR_STATUS_DBCREATE_FAILED,
    SBKR_STATUS_DBSELECT_FAILED,
    SBKR_STATUS_DBUPDATE_FAILED,
    SBKR_STATUS_DBTRANSACTION_FAILED,
    SBKR_STATUS_DBINSERT_FAILED,
    SBKR_STATUS_DBDELETE_FAILED,
    SBKR_STATUS_CONNECT_FAILED,    // 10
    SBKR_STATUS_RECEIVE_FAILED,
    SBKR_STATUS_SEND_FAILED,
    SBKR_STATUS_HEADER_FAILED,
    SBKR_STATUS_LOCK_FAILED,
    SBKR_STATUS_NULL_POINTER,
    SBKR_STATUS_OUT_OF_MEMORY,
    SBKR_STATUS_NOT_FOUND,
    SBKR_STATUS_BUFFER_MISMATCH,
    SBKR_STATUS_BUFFER_EXCEEDED,
    SBKR_STATUS_DUPLICATE,         // 20
    SBKR_STATUS_INVALID_TYPE,
    SBKR_STATUS_MISMATCH_TYPE,
    SBKR_STATUS_NOT_SUPPORTED,
    SBKR_STATUS_NOT_SUBSCRIBE,
    SBKR_STATUS_NOT_ARCHIVE,
    SBKR_STATUS_NOT_SYNC,
    SBKR_STATUS_SEQ_FAILED,
    SBKR_STATUS_SYNC_FAILED,
    SBKR_STATUS_PARTIAL_FAILED,
    SBKR_STATUS_INVALID_METHOD,    // 30
    SBKR_STATUS_TIMEOUT,
    SBKR_STATUS_SYNC_QUALITY,
    SBKR_STATUS_SYNC_MAXDEAD,
    SBKR_STATUS_MISMATCH_ID,
} sbkr_errorcode_t;

//  Data type identifiers. Ordering is the canonical SBP wire order.
typedef enum {
    SBKR_TYPE_INT32 = 0,
    SBKR_TYPE_INT64,
    SBKR_TYPE_FLOAT,
    SBKR_TYPE_DOUBLE,
    SBKR_TYPE_BOOL,
    SBKR_TYPE_STRING,
    SBKR_TYPE_TIMESTAMP,
    SBKR_TYPE_BLOB,
    SBKR_TYPE_INT8,
    SBKR_TYPE_INT16
} sbkr_data_type_t;

//  Possible data quality
typedef enum
{
    SBKR_QUALITY_CREATED = 0,
    SBKR_QUALITY_DELETED,
    SBKR_QUALITY_GOOD,
    SBKR_QUALITY_BAD,
    SBKR_QUALITY_TOPICAL,
    SBKR_QUALITY_ERROR,
    SBKR_QUALITY_NO_DATA,
    SBKR_QUALITY_INVALID,
    SBKR_QUALITY_COMM_FAILURE,
    SBKR_QUALITY_CONFIG_ERROR,
    SBKR_QUALITY_NOT_FOUND,
    SBKR_QUALITY_NOT_SUPPORTED,
    SBKR_QUALITY_TIMEOUT,
    SBKR_QUALITY_NO_ACCESS,
    SBKR_QUALITY_UNKNOWN
} sbkr_quality_t;

//  Group all supported data types under one union.
union _sbkr_value_u
{
    int8_t      Int8;
    int16_t     Int16;
    int32_t     Int32;
    int64_t     Int64;
    float       Float;
    double      Double;
    bool        Bool;
    char       *String;           // Caller-owned; must be strdup'd where required
    struct {
        void *data;
        size_t size;
    } blob_val;
};

typedef union _sbkr_value_u sbkr_value;

//  Single data point definition.
typedef struct
{
    unsigned int        Id;
    sbkr_data_type_t    Type;
    sbkr_quality_t      Quality;
    uint64_t            Sequence;
    sbkr_time_t         Time;
    sbkr_value          Value;
} sbkr_data;

#if defined(__cplusplus)
}
#endif

#endif
