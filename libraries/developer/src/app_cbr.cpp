// Copyright (c) 2001-2009, Scalable Network Technologies, Inc.  All Rights Reserved.
//                          6100 Center Drive
//                          Suite 1250
//                          Los Angeles, CA 90045
//                          sales@scalable-networks.com
//
// This source code is licensed, not sold, and is subject to a written
// license agreement.  Among other things, no portion of this source
// code may be copied, transmitted, disclosed, displayed, distributed,
// translated, used as the basis for a derivative work, or used, in
// whole or in part, for any program or purpose other than its intended
// use in compliance with the license agreement as part of the QualNet
// software.  This source code and certain of the algorithms contained
// within it are confidential trade secrets of Scalable Network
// Technologies, Inc. and may not be used as the basis for any other
// software, hardware, product or service.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "partition.h"
#include "app_util.h"
#include "app_cbr.h"

#ifdef ADDON_DB
#include "db.h"
#include "dbapi.h"
#endif // ADDON_DB


// /**
// FUNCTION   :: AppLayerCbrInitTrace
// LAYER      :: APPLCATION
// PURPOSE    :: Cbr initialization  for tracing
// PARAMETERS ::
// + node : Node* : Pointer to node
// + nodeInput    : const NodeInput* : Pointer to NodeInput
// RETURN ::  void : NULL
// **/


void AppLayerCbrInitTrace(Node* node, const NodeInput* nodeInput)
{
    char buf[MAX_STRING_LENGTH];
    BOOL retVal;
    BOOL traceAll = TRACE_IsTraceAll(node);
    BOOL trace = FALSE;
    static BOOL writeMap = TRUE;

    IO_ReadString(
        node->nodeId,
        ANY_ADDRESS,
        nodeInput,
        "TRACE-CBR",
        &retVal,
        buf);

    if (retVal)
    {
        if (strcmp(buf, "YES") == 0)
        {
            trace = TRUE;
        }
        else if (strcmp(buf, "NO") == 0)
        {
            trace = FALSE;
        }
        else
        {
            ERROR_ReportError(
                "TRACE-CBR should be either \"YES\" or \"NO\".\n");
        }
    }
    else
    {
        if (traceAll || node->traceData->layer[TRACE_APPLICATION_LAYER])
        {
            trace = TRUE;
        }
    }

    if (trace)
    {
            TRACE_EnableTraceXML(node, TRACE_CBR,
                "CBR",  AppLayerCbrPrintTraceXML, writeMap);
    }
    else
    {
            TRACE_DisableTraceXML(node, TRACE_CBR,
                "CBR", writeMap);
    }
    writeMap = FALSE;
}


 //**
// FUNCTION   :: AppLayerCbrPrintTraceXML
// LAYER      :: NETWORK
// PURPOSE    :: Print packet trace information in XML format
// PARAMETERS ::
// + node     : Node*    : Pointer to node
// + msg      : Message* : Pointer to packet to print headers from
// RETURN     ::  void   : NULL
// **/

void AppLayerCbrPrintTraceXML(Node* node, Message* msg)
{
    char buf[MAX_STRING_LENGTH];
    char clockStr[MAX_STRING_LENGTH];

    CbrData* data = (CbrData*) MESSAGE_ReturnPacket(msg);

    if (data == NULL) {
        return;
       }

    TIME_PrintClockInSecond(data->txTime, clockStr);

    sprintf(buf, "<cbr>%u %c %u %d %s</cbr>",
            data->sourcePort,
            data->type,
            data->seqNo,
                 (msg->packetSize + msg->virtualPayloadSize),
            clockStr);
    TRACE_WriteToBufferXML(node, buf);
}

/*
 * NOTE: CBR does not attempt to reorder packets.  Any out of order
 *       packets will be dropped.
 */

/*
 * NAME:        AppLayerCbrClient.
 * PURPOSE:     Models the behaviour of CbrClient Client on receiving the
 *              message encapsulated in msg.
 * PARAMETERS:  node - pointer to the node which received the message.
 *              msg - message received by the layer
 * RETURN:      none.
 */
void
AppLayerCbrClient(Node *node, Message *msg)
{
    char buf[MAX_STRING_LENGTH];
    char error[MAX_STRING_LENGTH];
    AppDataCbrClient *clientPtr;

#ifdef DEBUG
    TIME_PrintClockInSecond(getSimTime(node), buf);
    printf("CBR Client: Node %ld got a message at %sS\n",
           node->nodeId, buf);
#endif /* DEBUG */

    switch (msg->eventType)
    {
        case MSG_APP_TimerExpired:
        {
            AppTimer *timer;

            timer = (AppTimer *) MESSAGE_ReturnInfo(msg);

#ifdef DEBUG
            printf("CBR Client: Node %ld at %s got timer %d\n",
                   node->nodeId, buf, timer->type);
#endif /* DEBUG */

            clientPtr = AppCbrClientGetCbrClient(node, timer->sourcePort);

            if (clientPtr == NULL)
            {
                sprintf(error, "CBR Client: Node %d cannot find cbr"
                    " client\n", node->nodeId);

                ERROR_ReportError(error);
            }

            switch (timer->type)
            {
                case APP_TIMER_SEND_PKT:
                {
                    CbrData data;
                    char cbrData[CBR_HEADER_SIZE];

                    memset(&data, 0, sizeof(data));

                    ERROR_Assert(sizeof(data) <= CBR_HEADER_SIZE,
                         "CbrData size cant be greater than CBR_HEADER_SIZE");

#ifdef DEBUG
                    printf("CBR Client: Node %u has %u items left to"
                        " send\n", node->nodeId, clientPtr->itemsToSend);
#endif /* DEBUG */

                    if (((clientPtr->itemsToSend > 1) &&
                         (getSimTime(node) + clientPtr->interval
                            < clientPtr->endTime)) ||
                        ((clientPtr->itemsToSend == 0) &&
                         (getSimTime(node) + clientPtr->interval
                            < clientPtr->endTime)) ||
                        ((clientPtr->itemsToSend > 1) &&
                         (clientPtr->endTime == 0)) ||
                        ((clientPtr->itemsToSend == 0) &&
                         (clientPtr->endTime == 0)))
                    {
                        data.type = 'd';
                    }
                    else
                    {
                        data.type = 'c';
                        clientPtr->sessionIsClosed = TRUE;
                        clientPtr->sessionFinish = getSimTime(node);
                    }

                    data.sourcePort = clientPtr->sourcePort;
                    data.txTime = getSimTime(node);
                    data.seqNo = clientPtr->seqNo++;
                    data.isMdpEnabled = clientPtr->isMdpEnabled;
#ifdef ADVANCED_WIRELESS_LIB
                    data.pktSize = clientPtr->itemSize;
                    data.interval = clientPtr->interval;
#elif UMTS_LIB
                    data.pktSize = clientPtr->itemSize;
                    data.interval = clientPtr->interval;
#endif // ADVANCED_WIRELESS_LIB || UMTS_LIB

#ifdef DEBUG
                    {
                        char clockStr[MAX_STRING_LENGTH];
                        char addrStr[MAX_STRING_LENGTH];

                        TIME_PrintClockInSecond(getSimTime(node), clockStr, node);
                        IO_ConvertIpAddressToString(
                            &clientPtr->remoteAddr, addrStr);

                        printf("CBR Client: node %ld sending data packet"
                               " at time %sS to CBR server %s\n",
                               node->nodeId, clockStr, addrStr);
                        printf("    size of payload is %d\n",
                               clientPtr->itemSize);
                    }
#endif /* DEBUG */
                    // Note: An overloaded Function
                    memset(cbrData, 0, CBR_HEADER_SIZE);
                    memcpy(cbrData, (char *) &data, sizeof(data));
#ifdef ADDON_DB
                    StatsDBAppEventParam appParam;
                    appParam.m_SessionInitiator = node->nodeId;
                    appParam.m_ReceiverId = clientPtr->receiverId;
                    appParam.SetAppType("CBR");
                    appParam.SetFragNum(0);
                    appParam.SetAppName(clientPtr->applicationName);
                    appParam.SetReceiverAddr(&clientPtr->remoteAddr);
                    appParam.SetPriority(clientPtr->tos);
                    appParam.SetSessionId(clientPtr->sessionId);
                    appParam.SetMsgSize(clientPtr->itemSize);
                    appParam.m_TotalMsgSize = clientPtr->itemSize;
                    appParam.m_fragEnabled = FALSE;
#endif // ADDON_DB

                    APP_UdpSendNewHeaderVirtualDataWithPriority(
                        node,
                        APP_CBR_SERVER,
                        clientPtr->localAddr,
                        (short) clientPtr->sourcePort,
                        clientPtr->remoteAddr,
                        cbrData,
                        CBR_HEADER_SIZE,
                        clientPtr->itemSize - CBR_HEADER_SIZE,
                        clientPtr->tos,
                        0,
                        TRACE_CBR,
                        clientPtr->isMdpEnabled,
                        clientPtr->mdpUniqueId
#ifdef ADDON_DB
                        ,
                        &appParam
#endif // ADDON_DB
                        );

                    clientPtr->numBytesSent += clientPtr->itemSize;
                    clientPtr->numPktsSent++;
                    clientPtr->sessionLastSent = getSimTime(node);

                    if (clientPtr->itemsToSend > 0)
                    {
                        clientPtr->itemsToSend--;
                    }

                    if (clientPtr->sessionIsClosed == FALSE)
                    {
                        AppCbrClientScheduleNextPkt(node, clientPtr);
                    }

                    break;
                }

                default:
                {
#ifndef EXATA
                    assert(FALSE);
#endif
                }
            }

            break;
        }
        default:
           TIME_PrintClockInSecond(getSimTime(node), buf, node);
           sprintf(error, "CBR Client: at time %sS, node %d "
                   "received message of unknown type"
                   " %d\n", buf, node->nodeId, msg->eventType);
#ifndef EXATA
           ERROR_ReportError(error);
#endif
    }

    MESSAGE_Free(node, msg);
}

#ifdef ADDON_NGCNMS
// reset source address of application so that we
// can route packets correctly.
void
AppCbrClientReInit(Node* node, Address sourceAddr)
{
    AppInfo *appList = node->appData.appPtr;
    AppDataCbrClient *cbrClient;
    int i;

    for (; appList != NULL; appList = appList->appNext)
    {
        if (appList->appType == APP_CBR_CLIENT)
        {
            cbrClient = (AppDataCbrClient *) appList->appDetail;

            for (i=0; i< node->numberInterfaces; i++)
            {
                // currently only support ipv4. May be better way to compare this.
                if (cbrClient->localAddr.interfaceAddr.ipv4 == node->iface[i].oldIpAddress)
                {
                    memcpy(&(cbrClient->localAddr), &sourceAddr, sizeof(Address));
                }
            }
        }
    }

}
#endif

/*
 * NAME:        AppCbrClientInit.
 * PURPOSE:     Initialize a CbrClient session.
 * PARAMETERS:  node - pointer to the node,
 *              serverAddr - address of the server,
 *              itemsToSend - number of items to send,
 *              itemSize - size of each packet,
 *              interval - interval of packet transmission rate.
 *              startTime - time until the session starts,
 *              endTime - time until the session ends,
 *              tos - the contents for the type of service field.
 *              isRsvpTeEnabled - whether RSVP-TE is enabled
 *              isMdpEnabled - true if MDP is enabled by user.
 *              isProfileNameSet - true if profile name is provided by user.
 *              profileName - mdp profile name.
 *              uniqueId - mdp session unique id.
 *              nodeInput - nodeInput for config file.
 * RETURN:      none.
 */
void
AppCbrClientInit(
    Node *node,
    Address clientAddr,
    Address serverAddr,
    Int32 itemsToSend,
    Int32 itemSize,
    clocktype interval,
    clocktype startTime,
    clocktype endTime,
    unsigned tos,
    BOOL isRsvpTeEnabled,
#ifdef ADDON_DB
    char* appName,
#endif // ADDON_DB
    BOOL isMdpEnabled,
    BOOL isProfileNameSet,
    char* profileName,
    Int32 uniqueId,
    const NodeInput* nodeInput)
{
    char error[MAX_STRING_LENGTH];
    AppTimer *timer;
    AppDataCbrClient *clientPtr;
    Message *timerMsg;
    int minSize;
    startTime -= getSimStartTime(node);
    endTime -= getSimStartTime(node);

    ERROR_Assert(sizeof(CbrData) <= CBR_HEADER_SIZE,
                 "CbrData size cant be greater than CBR_HEADER_SIZE");
    minSize = CBR_HEADER_SIZE;

    /* Check to make sure the number of cbr items is a correct value. */
    if (itemsToSend < 0)
    {
        sprintf(error, "CBR Client: Node %d item to sends needs"
            " to be >= 0\n", node->nodeId);

        ERROR_ReportError(error);
    }

    /* Make sure that packet is big enough to carry cbr data information. */
    if (itemSize < minSize)
    {
        sprintf(error, "CBR Client: Node %d item size needs to be >= %d.\n",
                node->nodeId, minSize);
        ERROR_ReportError(error);
    }


    /* Make sure that packet is within max limit. */
    if (itemSize > APP_MAX_DATA_SIZE)
    {
        sprintf(error, "CBR Client: Node %d item size needs to be <= %d.\n",
                node->nodeId, APP_MAX_DATA_SIZE);
        ERROR_ReportError(error);
    }

    /* Make sure interval is valid. */
    if (interval <= 0)
    {
        sprintf(error, "CBR Client: Node %d interval needs to be > 0.\n",
                node->nodeId);
        ERROR_ReportError(error);
    }

    /* Make sure start time is valid. */
    if (startTime < 0)
    {
        sprintf(error, "CBR Client: Node %d start time needs to be >= 0.\n",
                node->nodeId);
        ERROR_ReportError(error);
    }

    /* Check to make sure the end time is a correct value. */
    if (!((endTime > startTime) || (endTime == 0)))
    {
        sprintf(error, "CBR Client: Node %d end time needs to be > "
            "start time or equal to 0.\n", node->nodeId);

        ERROR_ReportError(error);
    }

    // Validate the given tos for this application.
    if (/*tos < 0 || */tos > 255)
    {
        sprintf(error, "CBR Client: Node %d should have tos value "
            "within the range 0 to 255.\n",
            node->nodeId);
        ERROR_ReportError(error);
    }

#ifdef ADDON_NGCNMS
    clocktype origStart = startTime;

    // for restarts (node and interface).
    if (NODE_IsDisabled(node))
    {
        clocktype currTime = getSimTime(node);
        if (endTime <= currTime)
        {
            // application already finished.
            return;
        }
        else if (startTime >= currTime)
        {
            // application hasnt started yet
            startTime -= currTime;
        }
        else
        {
            // application has already started,
            // so pick up where we left off.

            // try to determine the number of items already sent.
            if (itemsToSend != 0)
            {
                clocktype diffTime = currTime - startTime;
                int itemsSent = diffTime / interval;

                if (itemsToSend < itemsSent)
                {
                    return;
                }

                itemsToSend -= itemsSent;
            }

            // start right away.
            startTime = 0;

        }
    }
#endif

    clientPtr = AppCbrClientNewCbrClient(node,
                                         clientAddr,
                                         serverAddr,
                                         itemsToSend,
                                         itemSize,
                                         interval,
#ifndef ADDON_NGCNMS
                                         startTime,
#else
                                         origStart,
#endif
                                         endTime,
#ifdef ADDON_DB
                                         (TosType) tos,
                                         appName);
#else // ADDON_DB
                                         (TosType) tos);
#endif // ADDON_DB

    if (clientPtr == NULL)
    {
        sprintf(error,
                "CBR Client: Node %d cannot allocate memory for "
                    "new client\n", node->nodeId);

        ERROR_ReportError(error);
    }

    //client pointer initialization with respect to mdp
    clientPtr->isMdpEnabled = isMdpEnabled;
    clientPtr->mdpUniqueId = uniqueId;

    if (isMdpEnabled)
    {
        //MDP Layer Init
        APP_MdpLayerInit(node,
                         clientAddr,
                         serverAddr,
                         clientPtr->sourcePort,
                         APP_CBR_CLIENT,
                         isProfileNameSet,
                         profileName,
                         uniqueId,
                         nodeInput,
                         -1,
                         TRUE);
    }

    if (node->transportData.rsvpProtocol && isRsvpTeEnabled)
    {
        // Note: RSVP is enabled for Ipv4 networks only.
        Message *rsvpRegisterMsg;
        AppToRsvpSend *info;

        rsvpRegisterMsg = MESSAGE_Alloc(node,
                                  TRANSPORT_LAYER,
                                  TransportProtocol_RSVP,
                                  MSG_TRANSPORT_RSVP_InitApp);

        MESSAGE_InfoAlloc(node,
                           rsvpRegisterMsg,
                           sizeof(AppToRsvpSend));

        info = (AppToRsvpSend *) MESSAGE_ReturnInfo(rsvpRegisterMsg);

        info->sourceAddr = GetIPv4Address(clientAddr);
        info->destAddr = GetIPv4Address(serverAddr);

        info->upcallFunctionPtr = NULL;

        MESSAGE_Send(node, rsvpRegisterMsg, startTime);
    }


    timerMsg = MESSAGE_Alloc(node,
                              APP_LAYER,
                              APP_CBR_CLIENT,
                              MSG_APP_TimerExpired);

    MESSAGE_InfoAlloc(node, timerMsg, sizeof(AppTimer));

    timer = (AppTimer *)MESSAGE_ReturnInfo(timerMsg);

    timer->sourcePort = clientPtr->sourcePort;
    timer->type = APP_TIMER_SEND_PKT;

#ifdef DEBUG
    {
        char clockStr[MAX_STRING_LENGTH];
        TIME_PrintClockInSecond(startTime, clockStr, node);
        printf("CBR Client: Node %ld starting client at %sS\n",
               node->nodeId, clockStr);
    }
#endif /* DEBUG */

    MESSAGE_Send(node, timerMsg, startTime);

#ifdef ADDON_NGCNMS

    clientPtr->lastTimer = timerMsg;

#endif
}

/*
 * NAME:        AppCbrClientPrintStats.
 * PURPOSE:     Prints statistics of a CbrClient session.
 * PARAMETERS:  node - pointer to the this node.
 *              clientPtr - pointer to the cbr client data structure.
 * RETURN:      none.
 */
void AppCbrClientPrintStats(Node *node, AppDataCbrClient *clientPtr) {
    clocktype throughput;

    char addrStr[MAX_STRING_LENGTH];
    char startStr[MAX_STRING_LENGTH];
    char closeStr[MAX_STRING_LENGTH];
    char sessionStatusStr[MAX_STRING_LENGTH];
    char throughputStr[MAX_STRING_LENGTH];

    char buf[MAX_STRING_LENGTH];
    char buf1[MAX_STRING_LENGTH];

    TIME_PrintClockInSecond(clientPtr->sessionStart, startStr, node);
    TIME_PrintClockInSecond(clientPtr->sessionLastSent, closeStr, node);

    if (clientPtr->sessionIsClosed == FALSE) {
        clientPtr->sessionFinish = getSimTime(node);
        sprintf(sessionStatusStr, "Not closed");
    }
    else {
        sprintf(sessionStatusStr, "Closed");
    }

    if (clientPtr->sessionFinish <= clientPtr->sessionStart) {
        throughput = 0;
    }
    else {
        throughput = (clocktype)
                     ((clientPtr->numBytesSent * 8.0 * SECOND)
                      / (clientPtr->sessionFinish
                      - clientPtr->sessionStart));
    }

    ctoa(throughput, throughputStr);

    if(clientPtr->remoteAddr.networkType == NETWORK_ATM)
    {
        const LogicalSubnet* dstLogicalSubnet =
            AtmGetLogicalSubnetFromNodeId(
            node,
            clientPtr->remoteAddr.interfaceAddr.atm.ESI_pt1,
            0);
        IO_ConvertIpAddressToString(
            dstLogicalSubnet->ipAddress,
            addrStr);
    }
    else
    {
        IO_ConvertIpAddressToString(&clientPtr->remoteAddr, addrStr);

    }

    sprintf(buf, "Server Address = %s", addrStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    sprintf(buf, "First Packet Sent at (s) = %s", startStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    sprintf(buf, "Last Packet Sent at (s) = %s", closeStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    sprintf(buf, "Session Status = %s", sessionStatusStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    ctoa((Int64) clientPtr->numBytesSent, buf1);
    sprintf(buf, "Total Bytes Sent = %s", buf1);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    sprintf(buf, "Total Packets Sent = %u", clientPtr->numPktsSent);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

    sprintf(buf, "Throughput (bits/s) = %s", throughputStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Client",
        ANY_DEST,
        clientPtr->sourcePort,
        buf);

}

/*
 * NAME:        AppCbrClientFinalize.
 * PURPOSE:     Collect statistics of a CbrClient session.
 * PARAMETERS:  node - pointer to the node.
 *              appInfo - pointer to the application info data structure.
 * RETURN:      none.
 */
void
AppCbrClientFinalize(Node *node, AppInfo* appInfo)
{
    AppDataCbrClient *clientPtr = (AppDataCbrClient*)appInfo->appDetail;

    if (node->appData.appStats == TRUE)
    {
        AppCbrClientPrintStats(node, clientPtr);
    }
}

/*
 * NAME:        AppCbrClientGetCbrClient.
 * PURPOSE:     search for a cbr client data structure.
 * PARAMETERS:  node - pointer to the node.
 *              sourcePort - source port of the cbr client.
 * RETURN:      the pointer to the cbr client data structure,
 *              NULL if nothing found.
 */
AppDataCbrClient *
AppCbrClientGetCbrClient(Node *node, short sourcePort)
{
    AppInfo *appList = node->appData.appPtr;
    AppDataCbrClient *cbrClient;

    for (; appList != NULL; appList = appList->appNext)
    {
        if (appList->appType == APP_CBR_CLIENT)
        {
            cbrClient = (AppDataCbrClient *) appList->appDetail;

            if (cbrClient->sourcePort == sourcePort)
            {
                return cbrClient;
            }
        }
    }

    return NULL;
}

/*
 * NAME:        AppCbrClientNewCbrClient.
 * PURPOSE:     create a new cbr client data structure, place it
 *              at the beginning of the application list.
 * PARAMETERS:  node - pointer to the node.
 *              remoteAddr - remote address.
 *              itemsToSend - number of cbr items to send in simulation.
 *              itemSize - size of each packet.
 *              interval - time between two packets.
 *              startTime - when the node will start sending.
 * RETURN:      the pointer to the created cbr client data structure,
 *              NULL if no data structure allocated.
 */
AppDataCbrClient *
AppCbrClientNewCbrClient(Node *node,
                         Address localAddr,
                         Address remoteAddr,
                         Int32 itemsToSend,
                         Int32 itemSize,
                         clocktype interval,
                         clocktype startTime,
                         clocktype endTime,
#ifdef ADDON_DB
                         TosType tos,
                         char* appName)
#else // ADDON_DB
                         TosType tos)
#endif // ADDON_DB
{
    AppDataCbrClient *cbrClient;

    cbrClient = (AppDataCbrClient *)
                MEM_malloc(sizeof(AppDataCbrClient));
    memset(cbrClient, 0, sizeof(AppDataCbrClient));

    /*
     * fill in cbr info.
     */
    memcpy(&(cbrClient->localAddr), &localAddr, sizeof(Address));
    memcpy(&(cbrClient->remoteAddr), &remoteAddr, sizeof(Address));
    cbrClient->interval = interval;
#ifndef ADDON_NGCNMS
    cbrClient->sessionStart = getSimTime(node) + startTime;
#else
    if (!NODE_IsDisabled(node))
    {
        cbrClient->sessionStart = getSimTime(node) + startTime;
    }
    else
    {
        // start time was already figured out in caller function.
        cbrClient->sessionStart = startTime;
    }
#endif
    cbrClient->sessionIsClosed = FALSE;
    cbrClient->sessionLastSent = getSimTime(node);
    cbrClient->sessionFinish = getSimTime(node);
    cbrClient->endTime = endTime;
    cbrClient->numBytesSent = 0;
    cbrClient->numPktsSent = 0;
    cbrClient->itemsToSend = itemsToSend;
    cbrClient->itemSize = itemSize;
    cbrClient->sourcePort = node->appData.nextPortNum++;
    cbrClient->seqNo = 0;
    cbrClient->tos = tos;
    cbrClient->uniqueId = node->appData.uniqueId++;
    //client pointer initialization with respect to mdp
    cbrClient->isMdpEnabled = FALSE;
    cbrClient->mdpUniqueId = -1;  //invalid value
#ifdef ADDON_DB
    cbrClient->sessionId = cbrClient->uniqueId;

    if (Address_IsAnyAddress(&(cbrClient->remoteAddr)) ||
        Address_IsMulticastAddress(&cbrClient->remoteAddr))
    {
        cbrClient->receiverId = 0;
    }
    else
    {
        cbrClient->receiverId =
            MAPPING_GetNodeIdFromInterfaceAddress(node, remoteAddr);
    }

    if (appName != NULL)
    {
        strcpy(cbrClient->applicationName, appName);
    }
    else
    {
        sprintf(cbrClient->applicationName,
                "CBR-%d-%d",
                node->nodeId,
                cbrClient->sessionId);
    }
#endif // ADDON_DB

    // Add CBR variables to hierarchy
    std::string path;
    D_Hierarchy *h = &node->partitionData->dynamicHierarchy;

    if (h->CreateApplicationPath(
            node,                   // node
            "cbrClient",            // protocol name
            cbrClient->sourcePort,  // port
            "interval",             // object name
            path))                  // path (output)
    {
        h->AddObject(
            path,
            new D_ClocktypeObj(&cbrClient->interval));
    }

// The HUMAN_IN_THE_LOOP_DEMO is part of a gui user-defined command
// demo.
// The type of service value for this CBR application is added to
// the dynamic hierarchy so that the user-defined-command can change
// it during simulation.
#ifdef HUMAN_IN_THE_LOOP_DEMO
    if (h->CreateApplicationPath(
            node,
            "cbrClient",
            cbrClient->sourcePort,
            "tos",                  // object name
            path))                  // path (output)
    {
        h->AddObject(
            path,
            new D_UInt32Obj(&cbrClient->tos));
    }
#endif

    if (h->CreateApplicationPath(
            node,
            "cbrClient",
            cbrClient->sourcePort,
            "numBytesSent",
            path))
    {
        h->AddObject(
            path,
            new D_Int64Obj(&cbrClient->numBytesSent));
    }

#ifdef DEBUG
    {
        char clockStr[MAX_STRING_LENGTH];
        char localAddrStr[MAX_STRING_LENGTH];
        char remoteAddrStr[MAX_STRING_LENGTH];

        IO_ConvertIpAddressToString(&cbrClient->localAddr, localAddrStr);
        IO_ConvertIpAddressToString(&cbrClient->remoteAddr, remoteAddrStr);

        printf("CBR Client: Node %u created new cbr client structure\n",
               node->nodeId);
        printf("    localAddr = %s\n", localAddrStr);
        printf("    remoteAddr = %s\n", remoteAddrStr);
        TIME_PrintClockInSecond(cbrClient->interval, clockStr);
        printf("    interval = %s\n", clockStr);
        TIME_PrintClockInSecond(cbrClient->sessionStart, clockStr, node);
        printf("    sessionStart = %sS\n", clockStr);
        printf("    numBytesSent = %u\n", cbrClient->numBytesSent);
        printf("    numPktsSent = %u\n", cbrClient->numPktsSent);
        printf("    itemsToSend = %u\n", cbrClient->itemsToSend);
        printf("    itemSize = %u\n", cbrClient->itemSize);
        printf("    sourcePort = %ld\n", cbrClient->sourcePort);
        printf("    seqNo = %ld\n", cbrClient->seqNo);
    }
#endif /* DEBUG */

    APP_RegisterNewApp(node, APP_CBR_CLIENT, cbrClient);

    return cbrClient;
}

/*
 * NAME:        AppCbrClientScheduleNextPkt.
 * PURPOSE:     schedule the next packet the client will send.  If next packet
 *              won't arrive until the finish deadline, schedule a close.
 * PARAMETERS:  node - pointer to the node,
 *              clientPtr - pointer to the cbr client data structure.
 * RETRUN:      none.
 */
void //inline//
AppCbrClientScheduleNextPkt(Node *node, AppDataCbrClient *clientPtr)
{
    AppTimer *timer;
    Message *timerMsg;

    timerMsg = MESSAGE_Alloc(node,
                              APP_LAYER,
                              APP_CBR_CLIENT,
                              MSG_APP_TimerExpired);

    MESSAGE_InfoAlloc(node, timerMsg, sizeof(AppTimer));

    timer = (AppTimer *)MESSAGE_ReturnInfo(timerMsg);
    timer->sourcePort = clientPtr->sourcePort;
    timer->type = APP_TIMER_SEND_PKT;

#ifdef DEBUG
    {
        char clockStr[24];
        printf("CBR Client: Node %u scheduling next data packet\n",
               node->nodeId);
        printf("    timer type is %d\n", timer->type);
        TIME_PrintClockInSecond(clientPtr->interval, clockStr);
        printf("    interval is %sS\n", clockStr);
    }
#endif /* DEBUG */

#ifdef ADDON_NGCNMS

    clientPtr->lastTimer = timerMsg;

#endif

    MESSAGE_Send(node, timerMsg, clientPtr->interval);
}

/*
 * NAME:        AppLayerCbrServer.
 * PURPOSE:     Models the behaviour of CbrServer Server on receiving the
 *              message encapsulated in msg.
 * PARAMETERS:  node - pointer to the node which received the message.
 *              msg - message received by the layer
 * RETURN:      none.
 */
void
AppLayerCbrServer(Node *node, Message *msg)
{
    char error[MAX_STRING_LENGTH];
    AppDataCbrServer *serverPtr;

    switch (msg->eventType)
    {
        case MSG_APP_FromTransport:
        {
            UdpToAppRecv *info;
            CbrData data;
#ifdef ADDON_DB
            AppMsgStatus msgStatus = APP_MSG_OLD;
#endif // ADDON_DB

            ERROR_Assert(sizeof(data) <= CBR_HEADER_SIZE,
                         "CbrData size cant be greater than CBR_HEADER_SIZE");

            info = (UdpToAppRecv *) MESSAGE_ReturnInfo(msg);
            memcpy(&data, MESSAGE_ReturnPacket(msg), sizeof(data));

            // trace recd pkt
            ActionData acnData;
            acnData.actionType = RECV;
            acnData.actionComment = NO_COMMENT;
            TRACE_PrintTrace(node,
                             msg,
                             TRACE_APPLICATION_LAYER,
                             PACKET_IN,
                             &acnData);

#ifdef DEBUG
            {
                char clockStr[MAX_STRING_LENGTH];
                char addrStr[MAX_STRING_LENGTH];
                TIME_PrintClockInSecond(data.txTime, clockStr, node);
                IO_ConvertIpAddressToString(&info->sourceAddr, addrStr);

                printf("CBR Server %ld: packet transmitted at %sS\n",
                       node->nodeId, clockStr);
                TIME_PrintClockInSecond(getSimTime(node), clockStr, node);
                printf("    received at %sS\n", clockStr);
                printf("    client is %s\n", addrStr);
                printf("    connection Id is %d\n", data.sourcePort);
                printf("    seqNo is %d\n", data.seqNo);
            }
#endif /* DEBUG */

            serverPtr = AppCbrServerGetCbrServer(node,
                                                 info->sourceAddr,
                                                 data.sourcePort);

            /* New connection, so create new cbr server to handle client. */
            if (serverPtr == NULL)
            {
                serverPtr = AppCbrServerNewCbrServer(node,
                                                     info->destAddr,
                                                     info->sourceAddr,
                                                     data.sourcePort);
#ifdef ADDON_DB
                // cbr application, clientPort == serverPort
                STATSDB_HandleSessionDescTableInsert(node, msg,
                    info->sourceAddr, info->destAddr,
                    info->sourcePort, info->destPort,
                    "CBR", "UDP");

                // Update the connectivity table
                StatsDBAppEventParam* appParamInfo = NULL;
                appParamInfo = (StatsDBAppEventParam*) MESSAGE_ReturnInfo(
                                   msg,
                                   INFO_TYPE_AppStatsDbContent);
                if (appParamInfo != NULL)
                {
                    STATSDB_HandleAppConnCreation(node, info->sourceAddr,
                        info->destAddr, appParamInfo->m_SessionId);
                }
#endif
            }

            if (serverPtr == NULL)
            {
                sprintf(error, "CBR Server: Node %d unable to "
                        "allocation server\n", node->nodeId);

                ERROR_ReportError(error);
            }

#ifdef ADDON_BOEINGFCS
            if ((serverPtr->useSeqNoCheck && data.seqNo >= serverPtr->seqNo) ||
                (serverPtr->useSeqNoCheck == FALSE))
#else
            if (data.seqNo >= serverPtr->seqNo || data.isMdpEnabled)
#endif
            {
                serverPtr->numBytesRecvd += MESSAGE_ReturnPacketSize(msg);
				serverPtr->prevSessionLastReceived = serverPtr->sessionLastReceived;
                serverPtr->sessionLastReceived = getSimTime(node);

                clocktype delay = getSimTime(node) - data.txTime;

                serverPtr->maxEndToEndDelay = MAX( delay, serverPtr->maxEndToEndDelay);
                serverPtr->minEndToEndDelay = MIN( delay, serverPtr->minEndToEndDelay);
                serverPtr->totalEndToEndDelay += delay;
                serverPtr->numPktsRecvd++;

                serverPtr->seqNo = data.seqNo + 1;




#ifdef ADDON_DB
                msgStatus = APP_MSG_NEW;
#endif // ADDON_DB

                // Calculate jitter.
                clocktype transitDelay = getSimTime(node)- data.txTime;
                clocktype tempJitter = 0;
                // Jitter can only be measured after receiving
                // two packets.
                if ( serverPtr->numPktsRecvd > 1)
                {

                    tempJitter = transitDelay -
                            serverPtr->lastTransitDelay;

                if (tempJitter < 0) {
                    tempJitter = -tempJitter;
                }
                if (serverPtr->numPktsRecvd == 2)
                {
                    serverPtr->actJitter = tempJitter;
                }
                else
                {
                    serverPtr->actJitter +=
                        ((tempJitter - serverPtr->actJitter)/16);
                }
                    serverPtr->totalJitter += serverPtr->actJitter;

                }

                serverPtr->lastTransitDelay = transitDelay;
                //serverPtr->lastPacketReceptionTime = getSimTime(node);

				
#ifdef ADDON_MIH
				clocktype instantThroughput;
				if (serverPtr->numPktsRecvd > 1) //Throughput can only be measured only after receiving two packets
				instantThroughput = (clocktype)((MESSAGE_ReturnPacketSize(msg) * 8.0 * SECOND) 
					/ (serverPtr->sessionLastReceived - serverPtr->prevSessionLastReceived));
				else
					instantThroughput = 0;


				BOOL printStats = TRUE;
				if (printStats)
				{
					clocktype currentTime = getSimTime(node);
					FILE *fp;
					int i=1;
					if (fp = fopen("delayserverstats", "a"))
					{
					fprintf(fp, "%d ", serverPtr->seqNo);
					fprintf(fp, "%u ", info->sourceAddr.interfaceAddr.ipv4);
					fprintf(fp, "%u ", info->destAddr.interfaceAddr.ipv4);
#ifdef _WIN32
					fprintf(fp, "%I64d ", currentTime);
					fprintf(fp, "%I64d ", delay);
					fprintf(fp, "%I64d ", serverPtr->actJitter);
					fprintf(fp, "%I64d\n", instantThroughput);
#else
					fprintf(fp, "%lld ", currentTime);
					fprintf(fp, "%lld ", delay);
					fprintf(fp, "%lld ", serverPtr->actJitter);
					fprintf(fp, "%lld\n", instantThroughput);
#endif
					fclose(fp);
					i++;
					}
					else
					printf("Error opening stats\n");
				}
#endif

#ifdef DEBUG
                {
                    char clockStr[24];
                    TIME_PrintClockInSecond(
                        serverPtr->totalEndToEndDelay, clockStr);
                    printf("    total end-to-end delay so far is %sS\n",
                           clockStr);
                }
#endif /* DEBUG */

                if (data.type == 'd')
                {
                    /* Do nothing. */
                }
                else if (data.type == 'c')
                {
                    serverPtr->sessionFinish = getSimTime(node);
                    serverPtr->sessionIsClosed = TRUE;
                }
                else
                {
                    assert(FALSE);
                }
            }
            else
            	printf(" packet arrived out of sequence !!!");

#ifdef ADDON_DB
            StatsDb* db = node->partitionData->statsDb;
            if (db != NULL)
            {
                clocktype delay = getSimTime(node) - data.txTime;

                if (serverPtr->numPktsRecvd == 1)
                { // Only need this info once
                    StatsDBAppEventParam* appParamInfo = NULL;
                    appParamInfo = (StatsDBAppEventParam*) MESSAGE_ReturnInfo(msg, INFO_TYPE_AppStatsDbContent);
                    if (appParamInfo != NULL)
                    {
                        serverPtr->sessionId = appParamInfo->m_SessionId;
                        serverPtr->sessionInitiator = appParamInfo->m_SessionInitiator;
                    }
                }

                SequenceNumber::Status seqStatus =
                    APP_ReportStatsDbReceiveEvent(
                        node,
                        msg,
                        &(serverPtr->seqNumCache),
                        data.seqNo,
                        delay,
                        serverPtr->actJitter,
                        MESSAGE_ReturnPacketSize(msg),
                        serverPtr->numPktsRecvd,
                        msgStatus);

                if (seqStatus == SequenceNumber::SEQ_NEW)
                {
                    // get hop count
                    StatsDBNetworkEventParam* ipParamInfo;
                    ipParamInfo = (StatsDBNetworkEventParam*)
                        MESSAGE_ReturnInfo(msg, INFO_TYPE_NetStatsDbContent);

                    if (ipParamInfo == NULL)
                    {
                        printf ("ERROR: We should have Network Events Info");
                    }
                    else
                    {
                        serverPtr->hopCount += (Int32) ipParamInfo->m_HopCount;
                    }
                }
            }
#endif // ADDON_DB
            break;
        }

        default:
        {
            char buf[MAX_STRING_LENGTH];

            TIME_PrintClockInSecond(getSimTime(node), buf, node);
            sprintf(error, "CBR Server: At time %sS, node %d received "
                    "message of unknown type "
                    "%d\n", buf, node->nodeId, msg->eventType);
            ERROR_ReportError(error);
        }
    }

    MESSAGE_Free(node, msg);
}

/*
 * NAME:        AppCbrServerInit.
 * PURPOSE:     listen on CbrServer server port.
 * PARAMETERS:  node - pointer to the node.
 * RETURN:      none.
 */
void
AppCbrServerInit(Node *node)
{
    APP_InserInPortTable(node, APP_CBR_SERVER, APP_CBR_SERVER);
}

/*
 * NAME:        AppCbrServerPrintStats.
 * PURPOSE:     Prints statistics of a CbrServer session.
 * PARAMETERS:  node - pointer to this node.
 *              serverPtr - pointer to the cbr server data structure.
 * RETURN:      none.
 */
void
AppCbrServerPrintStats(Node *node, AppDataCbrServer *serverPtr) {
    clocktype throughput;
    clocktype avgJitter;

    char addrStr[MAX_STRING_LENGTH];
    char jitterStr[MAX_STRING_LENGTH];
    char clockStr[MAX_STRING_LENGTH];
    char startStr[MAX_STRING_LENGTH];
    char closeStr[MAX_STRING_LENGTH];
    char sessionStatusStr[MAX_STRING_LENGTH];
    char throughputStr[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char buf1[MAX_STRING_LENGTH];

    TIME_PrintClockInSecond(serverPtr->sessionStart, startStr, node);
    TIME_PrintClockInSecond(serverPtr->sessionLastReceived, closeStr, node);

    if (serverPtr->sessionIsClosed == FALSE) {
        serverPtr->sessionFinish = getSimTime(node);
        sprintf(sessionStatusStr, "Not closed");
    }
    else {
        sprintf(sessionStatusStr, "Closed");
    }

    if (serverPtr->numPktsRecvd == 0) {
        TIME_PrintClockInSecond(0, clockStr);
    }
    else {
        TIME_PrintClockInSecond(
            serverPtr->totalEndToEndDelay / serverPtr->numPktsRecvd,
            clockStr);
    }

    if (serverPtr->sessionFinish <= serverPtr->sessionStart) {
        throughput = 0;
    }
    else {
        throughput = (clocktype)
                     ((serverPtr->numBytesRecvd * 8.0 * SECOND)
                      / (serverPtr->sessionFinish
                      - serverPtr->sessionStart));
    }

    ctoa(throughput, throughputStr);

    IO_ConvertIpAddressToString(&serverPtr->remoteAddr, addrStr);

    sprintf(buf, "Client address = %s", addrStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "First Packet Received at (s) = %s", startStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "Last Packet Received at (s) = %s", closeStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "Session Status = %s", sessionStatusStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);


    ctoa((Int64) serverPtr->numBytesRecvd, buf1);
    sprintf(buf, "Total Bytes Received = %s", buf1);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "Total Packets Received = %u", serverPtr->numPktsRecvd);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "Throughput (bits/s) = %s", throughputStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);

    sprintf(buf, "Average End-to-End Delay (s) = %s", clockStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);


    // Jitter can only be measured after receiving two packets.


    if (serverPtr->numPktsRecvd - 1 <= 0)
      {
        avgJitter = 0;
      }
    else
      {
        avgJitter = serverPtr->totalJitter / (serverPtr->numPktsRecvd-1) ;
      }

    TIME_PrintClockInSecond(avgJitter, jitterStr);

    sprintf(buf, "Average Jitter (s) = %s", jitterStr);
    IO_PrintStat(
        node,
        "Application",
        "CBR Server",
        ANY_DEST,
        serverPtr->sourcePort,
        buf);
}

/*
 * NAME:        AppCbrServerFinalize.
 * PURPOSE:     Collect statistics of a CbrServer session.
 * PARAMETERS:  node - pointer to the node.
 *              appInfo - pointer to the application info data structure.
 * RETURN:      none.
 */
void
AppCbrServerFinalize(Node *node, AppInfo* appInfo)
{
    AppDataCbrServer *serverPtr = (AppDataCbrServer*)appInfo->appDetail;

    if (node->appData.appStats == TRUE)
    {
        AppCbrServerPrintStats(node, serverPtr);
    }
#ifdef ADDON_DB
    if (serverPtr->seqNumCache != NULL)
    {
        delete serverPtr->seqNumCache;
        serverPtr->seqNumCache = NULL;
    }
#endif // ADDON_DB
}

/*
 * NAME:        AppCbrServerGetCbrServer.
 * PURPOSE:     search for a cbr server data structure.
 * PARAMETERS:  node - cbr server.
 *              remoteAddr - cbr client sending the data.
 *              sourcePort - sourcePort of cbr client/server pair.
 * RETURN:      the pointer to the cbr server data structure,
 *              NULL if nothing found.
 */
AppDataCbrServer * //inline//
AppCbrServerGetCbrServer(
    Node *node, Address remoteAddr, short sourcePort)
{
    AppInfo *appList = node->appData.appPtr;
    AppDataCbrServer *cbrServer;

    for (; appList != NULL; appList = appList->appNext)
    {
        if (appList->appType == APP_CBR_SERVER)
        {
            cbrServer = (AppDataCbrServer *) appList->appDetail;

            if ((cbrServer->sourcePort == sourcePort) &&
                IO_CheckIsSameAddress(
                    cbrServer->remoteAddr, remoteAddr))
            {
                return cbrServer;
            }
       }
   }

    return NULL;
}

/*
 * NAME:        AppCbrServerNewCbrServer.
 * PURPOSE:     create a new cbr server data structure, place it
 *              at the beginning of the application list.
 * PARAMETERS:  node - pointer to the node.
 *              localAddr - local address.
 *              remoteAddr - remote address.
 *              sourcePort - sourcePort of cbr client/server pair.
 * RETRUN:      the pointer to the created cbr server data structure,
 *              NULL if no data structure allocated.
 */
AppDataCbrServer * //inline//
AppCbrServerNewCbrServer(Node *node,
                         Address localAddr,
                         Address remoteAddr,
                         short sourcePort)
{
    AppDataCbrServer *cbrServer;

    cbrServer = (AppDataCbrServer *)
                MEM_malloc(sizeof(AppDataCbrServer));
    memset(cbrServer, 0, sizeof(AppDataCbrServer));
    /*
     * Fill in cbr info.
     */
    memcpy(&(cbrServer->localAddr), &localAddr, sizeof(Address));
    memcpy(&(cbrServer->remoteAddr), &remoteAddr, sizeof(Address));
    cbrServer->sourcePort = sourcePort;
    cbrServer->sessionStart = getSimTime(node);
    cbrServer->sessionFinish = getSimTime(node);
    cbrServer->sessionLastReceived = getSimTime(node);
    cbrServer->sessionIsClosed = FALSE;
    cbrServer->numBytesRecvd = 0;
    cbrServer->numPktsRecvd = 0;
    cbrServer->totalEndToEndDelay = 0;
    cbrServer->maxEndToEndDelay = 0;
    cbrServer->minEndToEndDelay = CLOCKTYPE_MAX;
    cbrServer->seqNo = 0;
    cbrServer->uniqueId = node->appData.uniqueId++;

    //    cbrServer->lastInterArrivalInterval = 0;
    //    cbrServer->lastPacketReceptionTime = 0;
    cbrServer->totalJitter = 0;
    cbrServer->actJitter = 0;
#ifdef ADDON_DB
    cbrServer->sessionId = -1;
    cbrServer->sessionInitiator = 0;
    cbrServer->hopCount = 0;
    cbrServer->seqNumCache = NULL;
#endif // ADDON_DB


#ifdef ADDON_BOEINGFCS
    BOOL retVal = FALSE;

    cbrServer->useSeqNoCheck = TRUE;

    IO_ReadBool(
        node->nodeId,
        ANY_ADDRESS,
        node->partitionData->nodeInput,
        "APP-CBR-USE-SEQUENCE-NUMBER-CHECK",
        &retVal,
        &cbrServer->useSeqNoCheck);

#endif

    // Add CBR variables to hierarchy
    std::string path;
    D_Hierarchy *h = &node->partitionData->dynamicHierarchy;

    if (h->CreateApplicationPath(
            node,
            "cbrServer",
            cbrServer->sourcePort,
            "numBytesRecvd",
            path))
    {
        h->AddObject(
            path,
            new D_Int64Obj(&cbrServer->numBytesRecvd));
    }

    APP_RegisterNewApp(node, APP_CBR_SERVER, cbrServer);

    return cbrServer;
}

#ifdef ADDON_NGCNMS

void AppCbrReset(Node* node)
{

    if (node->disabled)
    {
        AppInfo *appList = node->appData.appPtr;
        AppDataCbrClient *cbrClient;

        for (; appList != NULL; appList = appList->appNext)
        {
            if (appList->appType == APP_CBR_CLIENT)
            {
                cbrClient = (AppDataCbrClient *) appList->appDetail;
                MESSAGE_CancelSelfMsg(node, cbrClient->lastTimer);
            }
        }
    }
}

#endif
