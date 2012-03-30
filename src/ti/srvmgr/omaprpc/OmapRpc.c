/*
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

#include <ti/grcm/RcmTypes.h>
#include <ti/grcm/RcmServer.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/ipc/rpmsg/MessageQCopy.h>
#include <ti/srvmgr/NameMap.h>
#include <ti/srvmgr/ServiceMgr.h>

#include "OmapRpc.h"

typedef struct OmapRpc_Object {
    Char                channelName[OMAPRPC_MAX_CHANNEL_NAMELEN];
    UInt16              dstProc;
    UInt32              port;
    MessageQCopy_Handle msgq;
    UInt32              localEndPt;
    Task_Handle         taskHandle;
    Bool                shutdown;
    Semaphore_Handle    exitSem;
} OmapRpc_Object;


static Void omapRpcTask(UArg arg0, UArg arg1)
{
    OmapRpc_Object *obj = (OmapRpc_Object *)arg0;
    UInt32 local;
    UInt32 remote;
    Int status;
    UInt16 len;
    UInt8  msg[512]; /* maximum rpmsg size is probably smaller, but we need to
                      * be cautious */
    OmapRpc_MsgHeader *hdr = (OmapRpc_MsgHeader *)&msg[0];

    if (obj == NULL) {
        System_printf("OMAPRPC: Failed to start task as arguments are NULL!\n");
        return;
    }

    if (obj->msgq == NULL) {
        System_printf("OMAPRPC: Failed to start task as MessageQ was NULL!\n");
        return;
    }

    /* get the local endpoint we are to use */
    local = obj->localEndPt;
    System_printf("OMAPRPC: connecting from local endpoint %u to port %u\n",
                        obj->localEndPt, obj->port);

    NameMap_register("omaprpc", obj->port);

    System_printf("OMAPRPC: started channel on port: %d\n", obj->port);

    while (!obj->shutdown) {

        /* clear out the message and our message vars */
        _memset(msg, 0, sizeof(msg));
        len = 0;
        remote = 0;

        /* receive the message */
        status = MessageQCopy_recv(obj->msgq, (Ptr)msg, &len, &remote,
                                        MessageQCopy_FOREVER);

        if (status == MessageQCopy_E_UNBLOCKED) {
            System_printf("OMAPRPC: unblocked while waiting for messages\n");
            continue;
        }

        System_printf("OMAPRPC: received msg type: %d len: %d from addr: %d\n",
                             hdr->msgType, len, remote);
        switch (hdr->msgType) {
            case OmapRpc_MsgType_CREATE_INSTANCE:
            {
                /* temp pointer to payload */
                OmapRpc_CreateInstance *create =
                                OmapRpc_PAYLOAD(msg, OmapRpc_CreateInstance);
                /* local copy of name */
                Char name[sizeof(create->name)];
                /* return structure */
                OmapRpc_InstanceHandle *handle =
                                OmapRpc_PAYLOAD(msg, OmapRpc_InstanceHandle);
                /* save a copy of the input structure data */
                _strcpy(name, create->name);
                /* clear out the old data */
                _memset(msg, 0, sizeof(msg));

                /* create a new instance of the service */
                handle->status = ServiceMgr_createService(name,
                                                &handle->endpointAddress);

                System_printf("OMAPRPC: created service instance named: %s "
                              "(status=%d) addr: %d\n", name, handle->status,
                                handle->endpointAddress);

                hdr->msgType = OmapRpc_MsgType_INSTANCE_CREATED;
                hdr->msgLen  = sizeof(OmapRpc_InstanceHandle);
                break;
            }
            case OmapRpc_MsgType_DESTROY_INSTANCE:
            {
                /* temp pointer to payload */
                OmapRpc_InstanceHandle *handle =
                                OmapRpc_PAYLOAD(msg, OmapRpc_InstanceHandle);

                /* don't clear out the old data... */
                System_printf("OMAPRPC: destroying instance addr: %d\n",
                                                    handle->endpointAddress);
                handle->status = ServiceMgr_deleteService(
                                                    handle->endpointAddress);
                hdr->msgType = OmapRpc_MsgType_INSTANCE_DESTROYED;
                hdr->msgLen = sizeof(OmapRpc_InstanceHandle);
                /* leave the endpoint address alone. */
                break;
            }
            case OmapRpc_MsgType_QUERY_CHAN_INFO:
            {
                OmapRpc_ChannelInfo *chanInfo =
                                      OmapRpc_PAYLOAD(msg, OmapRpc_ChannelInfo);

                _strcpy(chanInfo->name, obj->channelName);
                System_printf("OMAPRPC: channel info query - name %s\n",
                                                                chanInfo->name);
                hdr->msgType = OmapRpc_MsgType_CHAN_INFO;
                hdr->msgLen = sizeof(OmapRpc_ChannelInfo);
                break;
            }
            default:
            {
                /* temp pointer to payload */
                OmapRpc_Error *err = OmapRpc_PAYLOAD(msg, OmapRpc_Error);

                /* the debugging message before we clear */
                System_printf("OMAPRPC: unexpected msg type: %d\n",
                                                                hdr->msgType);

                /* clear out the old data */
                _memset(msg, 0, sizeof(msg));

                hdr->msgType = OmapRpc_MsgType_ERROR;
                err->endpointAddress = local;
                err->status = OmapRpc_ErrorType_NOT_SUPPORTED;
                break;
            }
       }

       /* compute the length of the return message. */
       len = sizeof(struct OmapRpc_MsgHeader) + hdr->msgLen;

       System_printf("OMAPRPC: Replying with msg type: %d to addr: %d "
                      " from: %d len: %u\n", hdr->msgType, remote, local, len);

       /* send the response. All messages get responses! */
       MessageQCopy_send(obj->dstProc, remote, local, msg, len);
    }

    System_printf("OMAPRPC: destroying channel on port: %d\n", obj->port);
    NameMap_unregister("omaprpc", obj->port);
    /* @TODO delete any outstanding ServiceMgr instances if no disconnect
     * was issued? */

    Semaphore_post(obj->exitSem);
}

OmapRpc_Handle OmapRpc_createChannel(String channelName, UInt16 dstProc,
                                    UInt32 port, RcmServer_Params *rcmSrvParams)

{
    Task_Params taskParams;
    OmapRpc_Object *obj = Memory_alloc(NULL, sizeof(OmapRpc_Object), 0, NULL);
    if (obj == NULL) {
        System_printf("OMAPRPC: Failed to allocate memory for object!\n");
        return NULL;
    }

    _memset(obj, 0, sizeof(OmapRpc_Object));
    obj->shutdown = FALSE;
    obj->dstProc = dstProc;
    obj->port = port;
    strncpy(obj->channelName, channelName, OMAPRPC_MAX_CHANNEL_NAMELEN-1);
    obj->channelName[OMAPRPC_MAX_CHANNEL_NAMELEN-1]='\0';

    ServiceMgr_init();
    if (ServiceMgr_register(channelName, rcmSrvParams) == TRUE) {
        System_printf("OMAPRPC: registered channel: %s\n", obj->channelName);
        MessageQCopy_init(obj->dstProc);
        obj->msgq = MessageQCopy_create(obj->port, &obj->localEndPt);
        if (obj->msgq == NULL) {
            goto unload;
        }
        Task_Params_init(&taskParams);
        taskParams.instance->name = channelName;
        taskParams.priority = 1;   /* Lowest priority thread */
        taskParams.arg0 = (UArg)obj;
        obj->exitSem = Semaphore_create(0, NULL, NULL);
        if (obj->exitSem == NULL) {
            goto unload;
        }
        obj->taskHandle = Task_create(omapRpcTask, &taskParams, NULL);
        if (obj->taskHandle == NULL) {
            goto unload;
        }
    }
    else {
        System_printf("OMAPRPC: FAILED to register channel: %s\n",
                                                            obj->channelName);
    }
    return obj;

unload:
    OmapRpc_deleteChannel(obj);
    return NULL;
}

Int OmapRpc_deleteChannel(OmapRpc_Handle handle)
{
    OmapRpc_Object *obj = (OmapRpc_Object *)handle;

    if (obj == NULL) {
        return OmapRpc_E_FAIL;
    }

    System_printf("OMAPRPC: deleting channel %s\n", obj->channelName);
    obj->shutdown = TRUE;
    if (obj->msgq) {
        MessageQCopy_unblock(obj->msgq);
        if (obj->exitSem) {
            Semaphore_pend(obj->exitSem, BIOS_WAIT_FOREVER);
            MessageQCopy_delete(&obj->msgq);
            Semaphore_delete(&obj->exitSem);
        }
        if (obj->taskHandle) {
            Task_delete(&obj->taskHandle);
        }
    }

    Memory_free(NULL, obj, sizeof(*obj));
    return OmapRpc_S_SUCCESS;
}
