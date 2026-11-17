#include "connection.h"
#include "gxobjects.h"
#include "serverevents.h"

void svr_preGet(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_postGet(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_preRead(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_postRead(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_preWrite(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_postWrite(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_preAction(dlmsSettings* settings, gxValueEventCollection* args)
{
}

void svr_postAction(dlmsSettings* settings, gxValueEventCollection* args)
{
}

    unsigned char svr_isTarget(
        dlmsSettings* settings,
        uint32_t serverAddress,
        uint32_t clientAddress)
{
    return 0;
}

int svr_connected(dlmsServerSettings* settings)
{
    return 0;
}

DLMS_ACCESS_MODE svr_getAttributeAccess(
    dlmsSettings* settings,
    gxObject* obj,
    unsigned char index)
{
    return DLMS_ACCESS_MODE_READ_WRITE;
}

DLMS_METHOD_ACCESS_MODE svr_getMethodAccess(
    dlmsSettings* settings,
    gxObject* obj,
    unsigned char index)
{
    return DLMS_METHOD_ACCESS_MODE_ACCESS;
}

void svr_trace(const char* str, const char* data)
{
}