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

/*!
 * \file mac_dot11.cpp
 * \brief Qualnet layer and message handling routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "api.h"
#include "partition.h"
#include "mac_dot11.h"
#include "mac_dot11-sta.h"
#include "mac_dot11-ap.h"
#include "mac_dot11-mgmt.h"
#include "mac_dot11-mib.cpp"
#include "mac_dot11s-frames.h"
#include "mac_dot11s.h"
#include "phy_802_11.h"
#include "mac_dot11-pc.h"
//--------------------HCCA-Updates Start---------------------------------//
#include "mac_dot11-hcca.h"
//--------------------HCCA-Updates End---------------------------------//
#include "network_ip.h"

#ifdef CYBER_LIB
#include "phy_802_11.h"
#include "phy_abstract.h"
#endif // CYBER_LIB

#ifdef ADDON_DB
#include "db.h"
#include "dbapi.h"
#endif

#ifdef ADDON_MIH
#define DEBUG_MIH	1
#endif

//--------------------------------------------------------------------------
//  Frame Functions
//--------------------------------------------------------------------------
/**
FUNCTION   :: MacDot11MgmtQueue_PeekPacket
LAYER      :: MAC
PURPOSE    :: Dequeue a frame from the management queue,
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ frameInfo : DOT11_FrameInfo** : Parameter for dequeued frame info
RETURN     :: BOOL          : TRUE if dequeue is successful
TODO       :: Add Dot11 stats if required
**/

BOOL MacDot11MgmtQueue_PeekPacket(
    Node* node,
    MacDataDot11* dot11,
    DOT11_FrameInfo** frameInfo)
{
    if (ListGetSize(node, dot11->mngmtQueue) == 0)
    {
        return FALSE;
    }

    ListItem *listItem = dot11->mngmtQueue->first;
    *frameInfo = (DOT11_FrameInfo*) (listItem->data);

    return TRUE;
}

/**
FUNCTION   :: MacDot11MgmtQueue_DequeuePacket
LAYER      :: MAC
PURPOSE    :: Dequeue a frame from the management queue,
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ frameInfo : DOT11_FrameInfo** : Parameter for dequeued frame info
RETURN     :: BOOL          : TRUE if dequeue is successful
TODO       :: Add Dot11 stats if required
**/

BOOL MacDot11MgmtQueue_DequeuePacket(
    Node* node,
    MacDataDot11* dot11)
{
    if (ListGetSize(node, dot11->mngmtQueue) == 0)
    {
        return FALSE;
    }
    ListItem *listItem = dot11->mngmtQueue->first;
    ListGet(node, dot11->mngmtQueue, listItem, FALSE, FALSE);
    MEM_free(listItem);

    return TRUE;
}
/**
FUNCTION   :: MacDot11MgmtQueue_EnqueuePacket
LAYER      :: MAC
PURPOSE    :: Enqueue a frame in the management queue.
                Notifies if it is the only item in the queue.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ frameInfo : DOT11_FrameInfo* : Enqueued frame info
RETURN     :: BOOL          : TRUE if enqueue is successful.
TODO       :: Add stats if required
**/

BOOL MacDot11MgmtQueue_EnqueuePacket(
    Node* node,
    MacDataDot11* dot11,
    DOT11_FrameInfo* frameInfo)
{
    if (frameInfo == NULL)
    {
        return FALSE;
    }

    ListAppend(node, dot11->mngmtQueue, 0, frameInfo);


    if (dot11->mgmtSendResponse == TRUE
        && frameInfo->frameType != DOT11_PROBE_RESP)
    {
        MacDot11StationTransmitAck(node,dot11,frameInfo->RA);
    }
    if (ListGetSize(node, dot11->mngmtQueue) == 1)
    {
        MacDot11MngmtQueueHasPacketToSend(node, dot11);
    }

    // No current limit to queue, etc., insertion always successful.
    return TRUE;
}


/**
FUNCTION   :: MacDot11MgmtQueue_Finalize
LAYER      :: MAC
PURPOSE    :: Deletes items in management queue & queue itself.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
RETURN     :: void
**/

void MacDot11MgmtQueue_Finalize(
    Node* node,
    MacDataDot11* dot11)
{
    LinkedList* list = dot11->mngmtQueue;
    ListItem *item;
    ListItem *tempItem;
    DOT11_FrameInfo* frameInfo;

    if (list == NULL)
    {
        return;
    }

    item = list->first;

    while (item)
    {
        tempItem = item;
        item = item->next;
        frameInfo = (DOT11_FrameInfo*) tempItem->data;

        MESSAGE_Free(node, frameInfo->msg);
        MEM_free(tempItem);
    }

    if (list != NULL)
    {
        MEM_free(list);
    }

    list = NULL;
}


/**
FUNCTION   :: MacDot11DataQueue_DequeuePacket
LAYER      :: MAC
PURPOSE    :: Dequeue a frame from the data queue.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ frameInfo : DOT11_FrameInfo** : Parameter for dequeued frame info
RETURN     :: BOOL          : TRUE if dequeue is successful
TODO       :: Add stats if required
**/

BOOL MacDot11DataQueue_DequeuePacket(
    Node* node,
    MacDataDot11* dot11,
    DOT11_FrameInfo** frameInfo)
{
    if (ListGetSize(node, dot11->dataQueue) == 0)
    {
        return FALSE;
    }

    ListItem *listItem = dot11->dataQueue->first;
    ListGet(node, dot11->dataQueue, listItem, FALSE, FALSE);
    *frameInfo = (DOT11_FrameInfo*) (listItem->data);
    MEM_free(listItem);

    return TRUE;
}


/**
FUNCTION   :: MacDot11DataQueue_EnqueuePacket
LAYER      :: MAC
PURPOSE    :: Enqueue a frame in the single data queue.
                Notifies if it is the only item in the queue.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ frameInfo : DOT11_FrameInfo* : Enqueued frame info
RETURN     :: BOOL          : TRUE if enqueue is successful.
TODO       :: Add stats if required
**/

BOOL MacDot11DataQueue_EnqueuePacket(
    Node* node,
    MacDataDot11* dot11,
    DOT11_FrameInfo* frameInfo)
{
    frameInfo->insertTime = getSimTime(node);;
    ListAppend(node, dot11->dataQueue, getSimTime(node), frameInfo);

    // Notify if single item in the queue.
    if (ListGetSize(node, dot11->dataQueue) == 1)
    {
        MacDot11DataQueueHasPacketToSend(node, dot11);
    }

    // No current queue size/packet limit, etc.,
    // insertion is always successful.
    return TRUE;
}


/**
FUNCTION   :: MacDot11DataQueue_Finalize
LAYER      :: MAC
PURPOSE    :: Free the local data queue, including items in it.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
RETURN     :: void
**/

void MacDot11DataQueue_Finalize(
    Node* node,
    MacDataDot11* dot11)
{
    LinkedList* list = dot11->dataQueue;
    ListItem *item;
    ListItem *tempItem;
    DOT11_FrameInfo* frameInfo;

    if (list == NULL)
    {
        return;
    }

    item = list->first;

    while (item)
    {
        tempItem = item;
        item = item->next;
        frameInfo = (DOT11_FrameInfo*) tempItem->data;

        MESSAGE_Free(node, frameInfo->msg);
        MEM_free(tempItem);
    }

    if (list != NULL)
    {
        MEM_free(list);
    }

    list = NULL;
}

/**
FUNCTION   :: MacDot11ReceiveNetworkLayerPacket
LAYER      :: MAC
PURPOSE    :: Process packet from Network layer at Node.
PARAMETERS ::
+ node      : Node*         : pointer to node
+ dot11     : MacDataDot11* : pointer to Dot11 data structure
+ nextHopAddr   : Mac802Address : address of next hop
+ ipNextHopAddr : Mac802Address : address of next hop from IP
+ ipDestAddr    : Mac802Address : destination address from IP
+ ipSourceAddr  : Mac802Address : source address from IP
RETURN     :: void
**/

static
void MacDot11ReceiveNetworkLayerPacket(
    Node* node,
    MacDataDot11* dot11,
    Message* msg,
    Mac802Address nextHopAddr,
    Mac802Address ipNextHopAddr,
    Mac802Address ipDestAddr,
    Mac802Address ipSourceAddr)
{

    DOT11_FrameInfo frameInfo;
    Message* newMsg;
    DOT11_FrameInfo* newFrameInfo;



    dot11->currentMessage = NULL;
    dot11->currentNextHopAddress = INVALID_802ADDRESS;


    frameInfo.msg = msg;
    // frame type is default
    frameInfo.RA = nextHopAddr;
    frameInfo.TA = dot11->selfAddr;
    frameInfo.DA = ipDestAddr;
    frameInfo.SA = ipSourceAddr;
    frameInfo.insertTime = getSimTime(node);

    MacDot11StationResetCurrentMessageVariables(node, dot11);



    newFrameInfo = (DOT11_FrameInfo*)MEM_malloc(sizeof(DOT11_FrameInfo*));
    *newFrameInfo = frameInfo;

    newMsg = MESSAGE_Duplicate(node, msg);
    MESSAGE_AddHeader(node,
                  newMsg,
                  DOT11_DATA_FRAME_HDR_SIZE,
                  TRACE_DOT11);

    newFrameInfo->msg = newMsg;
    newFrameInfo->frameType = DOT11_DATA;

    MacDot11StationSetFieldsInDataFrameHdr(dot11, (DOT11_FrameHdr*)MESSAGE_ReturnPacket(newMsg)
                                           ,ipDestAddr, newFrameInfo->frameType);

    // Enqueue the broadcast data for transmission
    MacDot11DataQueue_EnqueuePacket(node, dot11, newFrameInfo);

    // Enqueue the mesh broadcast for transmission
    MacDot11DataQueue_EnqueuePacket(node, dot11, newFrameInfo);

}
//--------------------------------------------------------------------------
//  NAME:        MacDot11ProcessNotMyFrame.
//  PURPOSE:     Handle frames that don't belong to this node.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               clocktype duration
//                  time
//               BOOL isARtsPacket
//                  Is a RTS packet
//               BOOL isAPollPacket
//                  Is a QoS Poll Packet
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static //inline//
void MacDot11ProcessNotMyFrame(
   Node* node, MacDataDot11* dot11,
   clocktype duration, BOOL isARtsPacket,BOOL isAPollPacket )
{
    clocktype currentTime = getSimTime(node);
    BOOL navHasChanged = FALSE;
    clocktype NewNAV;

    if (duration == DOT11_CF_FRAME_DURATION * MICRO_SECOND) {
        return;
    }

    if (!dot11->useDvcs) {
        NewNAV = currentTime + duration + EPSILON_DELAY;
        if (NewNAV > dot11->NAV) {
            dot11->NAV = NewNAV;
            if (DEBUG_QA)
            {
                char clockStr[20];
                TIME_PrintClockInSecond(dot11->NAV, clockStr);
                printf("Node %d set NAV to %s\n",node->nodeId, clockStr);
            }
            navHasChanged = TRUE;
        }

//--------------------HCCA-Updates Start---------------------------------//
        //Reset NAV if QoS Poll with duration 0
        else if (duration==0 && isAPollPacket)
        {
            dot11->NAV = NewNAV;
        }
//--------------------HCCA-Updates End---------------------------------//
    }
    else {
        double angleOfArrival =
            MacDot11StationGetLastSignalsAngleOfArrivalFromRadio(
            node,
            dot11);
        double delta = dot11->directionalInfo->defaultNavedDeltaAngle;

        DirectionalNav_Add(
            &dot11->directionalInfo->directionalNav,
            NormalizedAngle(angleOfArrival - delta),
            NormalizedAngle(angleOfArrival + delta),
            (currentTime + duration + EPSILON_DELAY),
            currentTime);

        DirectionalNav_GetEarliestNavExpirationTime(
            &dot11->directionalInfo->directionalNav,
            currentTime,
            &NewNAV);

        if (dot11->NAV != NewNAV) {
            dot11->NAV = NewNAV;
            if (DEBUG_QA)
            {
                char clockStr[20];
                TIME_PrintClockInSecond(dot11->NAV, clockStr);
                printf("Node %d set NAV to %s\n",node->nodeId, clockStr);
            }
            navHasChanged = TRUE;
        }

        if (dot11->beaconIsDue
                && MacDot11StationCanHandleDueBeacon(node, dot11)) {
                return;
            }

        MacDot11StationSetPhySensingDirectionForNextOutgoingPacket(
            node,
            dot11);
    }

    ERROR_Assert(
        dot11->state != DOT11_S_WF_DIFS_OR_EIFS,
        "MacDot11ProcessNotMyFrame: "
        "Invalid state -- waiting for DIFS or EIFS.\n");
    ERROR_Assert(
        dot11->state != DOT11_S_BO,
        "MacDot11ProcessNotMyFrame: "
        "Invalid state -- waiting for Backoff.\n");
    ERROR_Assert(
        dot11->state != DOT11_S_NAV_RTS_CHECK_MODE,
        "MacDot11ProcessNotMyFrame: "
        "Invalid state -- waiting for NAV RTS check mode.\n");

    // It may be the case that station has not joined and waiting for NAV to finish
    // to send Management frame and then received any frame then.
    // Update NAV and then wait for the time to transmit...
    if (MacDot11IsStationJoined(dot11)&&
        dot11->ScanStarted == FALSE)
    {
        ERROR_Assert(
            dot11->state != DOT11_S_WFNAV,
            "MacDot11ProcessNotMyFrame: "
            "Invalid state -- waiting for NAV.\n");
    }

    if ((navHasChanged) &&
        (MacDot11StationPhyStatus(node, dot11) == PHY_IDLE) &&
        (MacDot11StationHasFrameToSend(dot11)))
    {
        if (dot11->state == DOT11_S_WFCTS) {
            MacDot11Trace(node, dot11, NULL, "Wait for CTS aborted.");
            dot11->retxAbortedRtsDcf++;
        }

        // This is what we should do.
        //
        //if (isARtsPacket) {
            // If RTS-ing node failed to get a CTS
            // and start sending then reset the NAV
            // (MAC layer virtual carrier sense) for this
            // bystander node.
        //   MacDot11SetState(node,
        //      dot11, DOT11_S_NAV_RTS_CHECK_MODE);
        //   MacDot11StartTimer(node, dot11,
        //      (dot11->extraPropDelay + dot11->sifs +
        //       PHY_GetTransmissionDuration(
        //           node, dot11->myMacData->phyNumber,
        //           PHY_GetRxDataRateType(node, dot11->myMacData->phyNumber),
        //           DOT11_SHORT_CTRL_FRAME_SIZE) +
        //       dot11->extraPropDelay + 2 * dot11->slotTime));
        //} else {
        //
        //   MacDot11SetState(node, dot11, DOT11_S_WFNAV);
        //   MacDot11StartTimer(node, dot11,
        //      (dot11->NAV - currentTime));
        //  }
//---------------------------Power-Save-Mode-Updates---------------------//
    if (DEBUG_PS_NAV){
        MacDot11Trace(node, dot11, NULL,
            "Seting NAV.");
    }
//---------------------------Power-Save-Mode-End-Updates-----------------//
        // This is for ns-2 comparison.
        MacDot11StationSetState(node, dot11, DOT11_S_WFNAV);
        MacDot11StationStartTimer(node, dot11, (dot11->NAV - currentTime));
    }
}//void MacDot11ProcessNotMyFrame//


//--------------------------------------------------------------------------
//  NAME:        MacDot11ProcessFrame.
//  PURPOSE:     Process incoming frame.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               Message *frame
//                  Frame being processed
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
static //inline//
void MacDot11ProcessFrame(
    Node* node, MacDataDot11* dot11, Message *frame)
{
    DOT11_FrameHdr* hdr = (DOT11_FrameHdr*)MESSAGE_ReturnPacket(frame);
    Mac802Address sourceAddr = hdr->sourceAddr;
    int flag = FALSE;

    if ((dot11->state != DOT11_S_WFDATA) &&
        (MacDot11IsWaitingForResponseState(dot11->state)))
    {

        // Waiting for Another Packet Type Ignore This one.
        MacDot11Trace(node, dot11, NULL,
            "Drop, waiting for non-data response");
#ifdef ADDON_DB
        HandleMacDBEvents(        
                node, frame, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_Drop, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif

        MESSAGE_Free(node, frame);
    }
    else {
        MacDot11Trace(node, dot11, frame, "Receive");

        if (hdr->destAddr == ANY_MAC802) {
            UInt8 hdrFlags = hdr->frameFlags;
#ifdef ADDON_DB
            /*HandleMacDBEvents(        
                node, frame, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/

            HandleMacDBSummary(node, frame,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);

            HandleMacDBAggregate(node, frame,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
            MacDot11StationHandOffSuccessfullyReceivedBroadcast(
                node, dot11, frame);

            dot11->broadcastPacketsGotDcf++;
//---------------------------Power-Save-Mode-Updates---------------------//
            // Check more data present at AP
            if (MacDot11IsBssStation(dot11)
                && MacDot11IsStationJoined(dot11)
                && (MacDot11IsStationSupportPSMode(dot11))
                && !( hdrFlags & DOT11_MORE_DATA_FIELD)&&
                dot11->ScanStarted == FALSE){
                dot11->apHasBroadcastDataToSend = FALSE;
            }
//---------------------------Power-Save-Mode-End-Updates-----------------//
            if (dot11->state == DOT11_S_IDLE){
                MacDot11StationCheckForOutgoingPacket(node, dot11, FALSE);
            }
        }
        else {
//---------------------------Power-Save-Mode-Updates---------------------//
            // Check more data present at AP
            if (MacDot11IsBssStation(dot11)
                && MacDot11IsStationJoined(dot11)
                && (MacDot11IsStationSupportPSMode(dot11))
                && !(hdr->frameFlags & DOT11_MORE_DATA_FIELD)
                && dot11->ScanStarted == FALSE){
                dot11->apHasUnicastDataToSend = FALSE;
            }
//---------------------------Power-Save-Mode-End-Updates-----------------//
            MacDot11StationCancelTimer(node, dot11);
            MacDot11StationTransmitAck(node, dot11, sourceAddr);

            if (hdr->frameType == DOT11_QOS_DATA)
            {
                DOT11e_FrameHdr* fHdr =
                               (DOT11e_FrameHdr*)MESSAGE_ReturnPacket(frame);

                if (MacDot11StationCorrectSeqenceNumber(node,
                    dot11,
                    sourceAddr,
                    hdr->seqNo,
                    fHdr->qoSControl.TID))
                    {
                         flag = TRUE;
                    }
             }
            else
            {
                 flag = FALSE;
                if (MacDot11StationCorrectSeqenceNumber(node,
                    dot11,
                    sourceAddr,
                    hdr->seqNo))
                 {
                     flag = TRUE;
                 }
             }

            if (flag)
            {

#ifdef ADDON_DB
                /*HandleMacDBEvents(        
                    node, frame, 
                    node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
                
                HandleMacDBSummary(node, frame,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);

                HandleMacDBAggregate(node, frame,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                MacDot11Trace(node, dot11, frame, "Receive");
                // Update the poll list for received data frame
                MacDot11CfPollListUpdateForUnicastReceived(
                    node, dot11, sourceAddr);

                MacDot11StationHandOffSuccessfullyReceivedUnicast(
                    node, dot11, frame);

                dot11->unicastPacketsGotDcf++;
            }
            else {
                // Wrong sequence number, Drop.

                MacDot11Trace(node, dot11, NULL,
                    "Drop, wrong sequence number");
#ifdef ADDON_DB
                HandleMacDBEvents(        
                node, frame, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_Drop, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif

                MESSAGE_Free(node, frame);
            }
        }
    }
}


//--------------------------------------------------------------------------
// Inter-Layer Interface Routines
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//  NAME:        MacDot11NetworkLayerHasPacketToSend
//  PURPOSE:     Called when network layer buffers transition from empty.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
void MacDot11NetworkLayerHasPacketToSend(
    Node* node,
    MacDataDot11* dot11)
{
    // Ignore if station is not joined
    if (!MacDot11IsStationJoined(dot11)||
              dot11->ScanStarted == TRUE ||
              dot11->waitForProbeDelay == TRUE){
        return;
    }

//---------------------DOT11e--Updates------------------------------------//
    if (MacDot11IsQoSEnabled(node, dot11))
    {
//--------------------HCCA-Updates Start---------------------------------//
        if (!dot11->isHCCAEnable)
        {
            if (dot11->currentMessage != NULL  &&
                   dot11->state != DOT11_S_IDLE)
            {
                // Ignore this signal if Dot11 is trying to tranamit a packet
                return;
            }
//--------------------HCCA-Updates End---------------------------------//
        }

    }
//---------------------------Power-Save-Mode-Updates---------------------//
    else if ((dot11->currentMessage != NULL)
        || (MacDot11IsATIMDurationActive(dot11)
        && (dot11->state == DOT11_S_WFACK)))
//---------------------------Power-Save-Mode-End-Updates-----------------//
    {
        // Ignore this signal if Dot11 is already holding a packet
        return;
    }
//--------------------DOT11e-End-Updates---------------------------------//

    MacDot11StationMoveAPacketFromTheNetworkLayerToTheLocalBuffer(
        node,
        dot11);
//---------------------------Power-Save-Mode-Updates---------------------//
    if (MacDot11IsIBSSStationSupportPSMode(dot11)){
        if (MacDot11IsATIMDurationActive(dot11)
            && (dot11->state == DOT11_S_IDLE)){
            MacDot11StationCheckForOutgoingPacket(node, dot11, FALSE);
        }
        return;
    }

    if (MacDot11IsAPSupportPSMode(dot11) && (dot11->currentMessage == NULL)){
       return;
    }
    if (MacDot11IsBssStation(dot11)&& MacDot11IsStationJoined(dot11)
        && (MacDot11IsStationSupportPSMode(dot11))
        && (dot11->currentMessage != NULL)){
        // If STA in sleep mode then start listening the
        //      transmission channel
        MacDot11StationCancelTimer(node, dot11);
        MacDot11StationStartListening(node, dot11);
        // cancel the previous awake timer
        dot11->awakeTimerSequenceNo++;
        return;
    }
//---------------------------Power-Save-Mode-End-Updates-----------------//

    // dot11s. Network send notification may leave currentMessage as NULL.
    // For a mesh point the network layer packet may
    // have a destination outside the BSS and result
    // in currentMessage remaining NULL.
    if (dot11->isMP && dot11->currentMessage == NULL) {
        return;
    }

    if (dot11->state == DOT11_S_IDLE) {

//--------------------HCCA-Updates Start---------------------------------//
        if (MacDot11HcAttempToStartCap(node,dot11) ||
            dot11->currentMessage == NULL)
            return;
//--------------------HCCA-Updates End----------------------------------//

        if ((dot11->useDvcs) &&
            ((MacDot11StationPhyStatus(node, dot11) == PHY_IDLE) ||
             (MacDot11StationPhyStatus(node, dot11) == PHY_SENSING)))
        {
            if (dot11->beaconIsDue
                && MacDot11StationCanHandleDueBeacon(node, dot11)) {
                return;
            }
            MacDot11StationSetPhySensingDirectionForNextOutgoingPacket(
                node,
                dot11);
        }

        if (!((MacDot11StationPhyStatus(node, dot11) == PHY_IDLE) &&
           (MacDot11IsPhyIdle(node, dot11)))||MacDot11StationWaitForNAV(node, dot11))
        {
            if (dot11->currentACIndex >= DOT11e_AC_BK){
                MacDot11StationSetBackoffIfZero(
                    node,
                    dot11,
                    dot11->currentACIndex);
            }
            else {
                MacDot11StationSetBackoffIfZero(node, dot11);
            }
        }
        MacDot11StationAttemptToGoIntoWaitForDifsOrEifsState(node, dot11);

    }
}//MacDot11NetworkLayerHasPacketToSend//


//--------------------------------------------------------------------------
//  NAME:        MacDot11DataQueueHasPacketToSend
//  PURPOSE:     Called when local data queue transition from empty.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//  RETURN:      None
//  ASSUMPTION:  None
//  NOTES:       This queue is currently used only by Mesh Points
//               and does not have QoS considerations.
//--------------------------------------------------------------------------
void MacDot11DataQueueHasPacketToSend(
    Node* node,
    MacDataDot11* dot11)
{
    if (dot11->currentMessage != NULL) {
        return;
    }

    MacDot11StationMoveAPacketFromTheDataQueueToTheLocalBuffer(
        node,
        dot11);

    ERROR_Assert(dot11->currentMessage != NULL,
        "MacDot11DataQueueHasPacketToSend: "
        "Unable to dequeue packet.\n");

    if (dot11->state == DOT11_S_IDLE) {
        if ((dot11->useDvcs) &&
            ((MacDot11StationPhyStatus(node, dot11) == PHY_IDLE) ||
             (MacDot11StationPhyStatus(node, dot11) == PHY_SENSING)))
        {
            if (dot11->beaconIsDue
                && MacDot11StationCanHandleDueBeacon(node, dot11)) {
                return;
            }
            MacDot11StationSetPhySensingDirectionForNextOutgoingPacket(
                node,
                dot11);
        }

        if (dot11->BO == 0 &&
            MacDot11StationPhyStatus(node, dot11) == PHY_IDLE &&
            !MacDot11StationWaitForNAV(node, dot11))
        {
            // Not holding for NAV and phy is idle,
            // so no backoff.
        }
        else {
            // Otherwise set backoff.
            MacDot11StationSetBackoffIfZero(node, dot11);
        }
        MacDot11StationAttemptToGoIntoWaitForDifsOrEifsState(node, dot11);

    }
}
//--------------------------------------------------------------------------
//  NAME:        MacDot11MngmtQueueHasPacketToSend
//  PURPOSE:     Called when management queue transition from empty.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//  RETURN:      None
//  ASSUMPTION:  None
//  NOTES:       This queue is currently used only by Mesh Points
//               and does not have QoS considerations.
//--------------------------------------------------------------------------
// dot11s. New function for Mgmt queue notification.
void MacDot11MngmtQueueHasPacketToSend(
    Node* node,
    MacDataDot11* dot11)
{
    if (dot11->state == DOT11_S_IDLE && dot11->waitForProbeDelay == FALSE)
    {
        MacDot11StationCheckForOutgoingPacket(node,dot11,FALSE);
    }
}
//--------------------------------------------------------------------------
//  NAME:        MacDot11MgmtQueueHasPacketToSend
//  PURPOSE:     Called when management queue transition from empty.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//  RETURN:      None
//  ASSUMPTION:  None
//  NOTES:       This queue is currently used only by Mesh Points
//               and does not have QoS considerations.
//--------------------------------------------------------------------------
void MacDot11MgmtQueueHasPacketToSend(
    Node* node,
    MacDataDot11* dot11)
{
     if (dot11->currentMessage!= NULL || dot11->ACs[0].frameToSend != NULL)
        return;
    MacDot11sStationMoveAPacketFromTheMgmtQueueToTheLocalBuffer(node, dot11);

}


//--------------------------------------------------------------------------
//  NAME:        MacDot11ProcessBeacon
//  PURPOSE:     A station (IBSS, BSS station, AP/PC) receives a beacon.
//
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               Message* msg
//                  Received beacon message
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static // inline
void MacDot11ProcessBeacon(
    Node* node,
    MacDataDot11* dot11,
    Message* msg)
{
    DOT11_BeaconFrame* beacon =
        (DOT11_BeaconFrame*) MESSAGE_ReturnPacket(msg);

    dot11->pcfEnable = FALSE;
    MacDot11Trace(node, dot11, msg, "Receive");

     DOT11_CFParameterSet cfSet ;
     DOT11_TIMFrame timFrame = {0};
     clocktype newNAV = 0;
    if (Dot11_GetCfsetFromBeacon(node, dot11, msg, &cfSet))
        {
         dot11->pcfEnable = TRUE;
         int offset = ( sizeof(DOT11_BeaconFrame)+
         sizeof(DOT11_CFParameterSet)+
         DOT11_IE_HDR_LENGTH);
         Dot11_GetTimFromBeacon(node, dot11, msg,
         offset, &timFrame);
         dot11->DTIMCount =  timFrame.dTIMCount;
    }

    if (MacDot11IsBssStation(dot11) ){
        if (MacDot11IsFrameFromMyAP(dot11, beacon->hdr.sourceAddr)) {
            //This is beacon from my AP, update measurement
            MacDot11StationUpdateAPMeasurement(node, dot11, msg);
            MacDot11StationProcessBeacon(node, dot11, msg);
        }

        if (MacDot11IsAssociationDynamic(dot11) &&
            (!MacDot11IsStationJoined(dot11) ||
              dot11->ScanStarted == TRUE )) {

            MacDot11ManagementProcessFrame(node, dot11, msg);
        }

    }
        // If Not From My AP set NAV
    if (dot11->pcfEnable)
    {
        if (!MacDot11IsPC(dot11))
        {
            if (!(MacDot11IsFrameFromMyAP(dot11, beacon->hdr.sourceAddr))
                ||(MacDot11IsAp(dot11)))
            {
                    // If it is CFP Start Beacon
                 if (cfSet.cfpCount == 0 && timFrame.dTIMCount == 0)
                 {
                     // Set NAV for CFP duration.

                     int dataRateType = PHY_GetRxDataRateType(node,
                                                dot11->myMacData->phyNumber);

                     clocktype beaconTransmitTime = getSimTime(node) -
                                            PHY_GetTransmissionDuration(
                                                node,
                                                dot11->myMacData->phyNumber,
                                                dataRateType,
                                                MESSAGE_ReturnPacketSize(
                                                                       msg));

                     newNAV = beaconTransmitTime;


                     if ((unsigned short)(cfSet.cfpBalDuration) > 0)
                     {
                        newNAV += MacDot11TUsToClocktype
                                  ((unsigned short)(cfSet.cfpBalDuration))
                                    + EPSILON_DELAY;
                     }

                     if (newNAV > dot11->NAV)
                     {
                            dot11->NAV = newNAV;
                      }
                  }
            }
        }
    }
     else if (MacDot11IsPC(dot11)) {
        MacDot11CfpPcCheckBeaconOverlap(node, dot11, msg);
     }
     else {
            // dot11s. Process beacon.
        // Pass beacon to mesh point for processing.
        // The isProcessed in the signature is for
        // symmetry with other mesh receive functions.
        BOOL isProcessed;
        if (dot11->isMP) {
            Dot11s_ReceiveBeaconFrame(node, dot11, msg, &isProcessed);
        }

        // BSS station (also AP) receives beacon from outside BSS.
        // IBSS station receives beacon.
//---------------------------Power-Save-Mode-Updates---------------------//
        if (MacDot11IsIBSSStationSupportPSMode(dot11)
            && (dot11->stationHasReceivedBeconInAdhocMode == FALSE)){

            MacDot11StationBeaconTransmittedOrReceived(
                node,
                dot11,
                BEACON_RECEIVED);
            return;
        }// end of if
//---------------------------Power-Save-Mode-End-Updates-----------------//

        clocktype newNAV = getSimTime(node);

        if (newNAV > dot11->NAV) {
            MacDot11StationCancelTimer(node, dot11);

            // dot11s. Do not reset Tx values on beacon Rx.
            if (dot11->isMP == FALSE)
            {
                //MacDot11StationResetBackoffAndAckVariables(node, dot11);
            }

            dot11->NAV = newNAV;
            if (DEBUG_QA)
            {
                char clockStr[20];
                TIME_PrintClockInSecond(dot11->NAV, clockStr);
                printf("Node %d set NAV to %s\n",node->nodeId, clockStr);
            }

        }
    }

    if (MacDot11IsBssStation(dot11) ){
        if (MacDot11IsFrameFromMyAP(dot11, beacon->hdr.sourceAddr)) {
            if ((dot11->apHasBroadcastDataToSend == FALSE)
                && (dot11->currentMessage == NULL)
                && dot11->apHasUnicastDataToSend == TRUE){
                    // send PS Poll frame to AP
                    MacDot11StationTransmitPSPollFrame(node,
                                                       dot11,
                                                       dot11->bssAddr);
                }
        }
    }


}// MacDot11ProcessBeacon


//--------------------------------------------------------------------------
//  NAME:        MacDot11ProcessAnyFrame
//  PURPOSE:     Process a broadcast frame
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               Mac802Address address
//                  Node address of station
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static
void MacDot11ProcessAnyFrame(
    Node* node,
    MacDataDot11* dot11,
    Message* msg)
{

    DOT11_ShortControlFrame* hdr =
        (DOT11_ShortControlFrame*) MESSAGE_ReturnPacket(msg);

    switch (hdr->frameType) {
        case DOT11_DATA:
        case DOT11_QOS_DATA:
        case DOT11_CF_DATA_ACK: {
//---------------------------Power-Save-Mode-Updates---------------------//
            if (MacDot11IsIBSSStationSupportPSMode(dot11)){
                MacDot11SetExchangeVariabe(node, dot11, msg);
            }
//---------------------------Power-Save-Mode-End-Updates-----------------//
            BOOL processFrame = MacDot11IsMyBroadcastDataFrame(
                node,
                dot11,
                msg);
            // dot11s. Skip non-mesh broadcasts.
            // Mesh Points do not receive broadcasts from the BSS
            // and do not relay non-MeshData broadcasts.
            if (dot11->isMP) {
                processFrame = FALSE;
            }

            if (processFrame) {
                Mac802Address sourceNodeAddress =
                    ((DOT11_FrameHdr*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
                if (MacDot11IsAp(dot11))
                {

                    DOT11_ApStationListItem* stationItem =
                    MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        sourceNodeAddress);
                    if (stationItem) {

                        stationItem->data->LastFrameReceivedTime =
                                                      getSimTime(node);
                    }
                  }

                if (dot11->useDvcs) {
                    BOOL nodeIsInCache;

                    MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                        node,
                        dot11,
                        sourceNodeAddress,
                        &nodeIsInCache);
                }

                MacDot11ProcessFrame(node, dot11, msg);
            }
            else {
                MacDot11Trace(node, dot11, NULL,
                    "Ignored data broadcast");
#ifdef ADDON_DB
                HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_Drop, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                MESSAGE_Free(node, msg);
            }
            break;
        }

        case DOT11_MESH_DATA: {
            // dot11s. Process mesh BCs once initialized.
            // Mesh Points always process broadcasts
            // once AP services are initialized.
            BOOL processFrame = FALSE;

            if (dot11->isMP) {
                processFrame = MacDot11IsStationJoined(dot11);
            }

            if (processFrame) {
                Mac802Address sourceNodeAddress =
                    ((DOT11_FrameHdr*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
                if (MacDot11IsAp(dot11))
                {

                    DOT11_ApStationListItem* stationItem =
                    MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        sourceNodeAddress);
                    if (stationItem) {

                        stationItem->data->LastFrameReceivedTime =
                                                      getSimTime(node);
                    }
                 }

                if (dot11->useDvcs) {
                    BOOL nodeIsInCache;

                    MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                        node, dot11, sourceNodeAddress,
                        &nodeIsInCache);
                }

                MacDot11ProcessFrame(node, dot11, msg);
            }
            else {
                MacDot11Trace(node, dot11, NULL,
                    "Ignored mesh data broadcast");
#ifdef ADDON_DB
                HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_Drop, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                MESSAGE_Free(node, msg);
            }
            break;
        }

        case DOT11_BEACON:
        {
            // Currently handling only beacons.
            MacDot11ProcessBeacon(node, dot11, msg);
            MESSAGE_Free(node, msg);
            break;
        }
        case DOT11_CF_END:
        case DOT11_CF_END_ACK:
        {   if (!MacDot11IsPC(dot11)){
            MacDot11ProcessCfpEnd(node, dot11, msg);
            }
            MESSAGE_Free(node, msg);
            break;
        }
        default:
        {
            // dot11s. Hook for Mgmt broadcast.
            // If the management broadcast contains mesh elements,
            // the rx frame is processed completely, else it is
            // passed to the management code for further processing.
            if (dot11->isMP) {
#ifdef ADDON_DB
                Message *dbMsg = MESSAGE_Duplicate(node, msg);
#endif
                if (Dot11s_ReceiveMgmtBroadcast(node, dot11, msg)) {
#ifdef ADDON_DB
                /*HandleMacDBEvents(        
                    node, dbMsg,
                    node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
                
                HandleMacDBSummary(node, dbMsg,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
                HandleMacDBAggregate(node, dbMsg,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                    dot11->broadcastPacketsGotDcf++;
                    break;
                }
#ifdef ADDON_DB
                MESSAGE_Free(node, dbMsg);
#endif
            }

            // Check if this is a Management frame that needs processing
            if (MacDot11ManagementCheckFrame(node, dot11, msg)){
//---------------------------Power-Save-Mode-Updates---------------------//
                //MESSAGE_Free(node, msg);
//---------------------------Power-Save-Mode-End-Updates-----------------//
                return;
            }

            char errString[MAX_STRING_LENGTH];

            sprintf(errString,
                "MacDot11ProcessAnyFrame: "
                "Received unknown broadcast frame type %d\n",
                hdr->frameType);
            ERROR_ReportWarning(errString);
            MESSAGE_Free(node, msg);
            break;
         }
    }//switch
}//MacDot11ProcessAnyFrame

//--------------------------------------------------------------------------
//  NAME:        MacDot11ProcessMyFrame
//  PURPOSE:     Process a frame unicast frame
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               Mac802Address address
//                  Node address of station
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static
void MacDot11ProcessMyFrame(
    Node* node,
    MacDataDot11* dot11,
    Message* msg)
{

    DOT11_ShortControlFrame* hdr =
        (DOT11_ShortControlFrame*) MESSAGE_ReturnPacket(msg);

#ifdef ADDON_MIH

if (dot11->isTimerExpired)
{
	DOT11_ManagementVars* mngmtVars =
        (DOT11_ManagementVars *) dot11->mngmtVars;
	MacDot11StationGetMeasurementInfoAndSendLinkParametersReportIndication(node, dot11, mngmtVars->channelInfo->headAPList);
	dot11->isTimerExpired = FALSE;
}
#endif

#ifdef ADDON_DB
    StatsDb* db = node->partitionData->statsDb;
#endif

    switch (hdr->frameType) {

        case DOT11_RTS: {
            DOT11_LongControlFrame* hdr =
            (DOT11_LongControlFrame*) MESSAGE_ReturnPacket(msg);
#ifdef ADDON_DB
            /*HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/

            HandleMacDBSummary(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
            HandleMacDBAggregate(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
             if (MacDot11IsAp(dot11))
             {
                   DOT11_ApStationListItem* stationItem =
                   MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        hdr->sourceAddr);
                   if (stationItem) {

                       stationItem->data->LastFrameReceivedTime =
                                                    getSimTime(node);
                    }
              }
            if (!dot11->useDvcs) {
                if (MacDot11IsWaitingForResponseState(dot11->state)) {
                    // Do nothing
                }
                else if ((!MacDot11StationWaitForNAV(node, dot11)) &&
                         (MacDot11StationPhyStatus(node, dot11) == PHY_IDLE)){

                    // Transmit CTS only if NAV (software carrier sense)
                    // and the phy says the channel is idle.

                    MacDot11Trace(node, dot11, msg, "Receive");

                    // Received RTS, check if it already joined or not
                    if (!MacDot11IsAp(dot11))
                    {
                        //DOT11_ManagementVars * mngmtVars =
                        //            (DOT11_ManagementVars*) dot11->mngmtVars;

//---------------------------Power-Save-Mode-Updates---------------------//
                        // in Infrastructure mode
                        //Check if the frame is from AP and update measurement
                        DOT11_LongControlFrame* hdr =
                            (DOT11_LongControlFrame*)MESSAGE_ReturnPacket(msg);
                        if (MacDot11IsStationNonAPandJoined(dot11) &&
                            hdr->sourceAddr == dot11->bssAddr)
                        {
                           MacDot11StationUpdateAPMeasurement(
                               node,
                               dot11,
                               msg);
                        }
                        // In Ad hoc mode no need to check about association
                        MacDot11StationCancelTimer(node, dot11);
                        MacDot11StationTransmitCTSFrame(
                            node,
                            dot11,
                            msg);


                    } else {
                        MacDot11StationCancelTimer(node, dot11);
                        MacDot11StationTransmitCTSFrame(node, dot11, msg);
                   }

                }
            }
            else {
                Mac802Address sourceNodeAddress =
                    ((DOT11_LongControlFrame *) MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
                BOOL nodeIsInCache;

                if (dot11->useDvcs) {
                    MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                        node, dot11, sourceNodeAddress, &nodeIsInCache);
                }

                if (MacDot11IsWaitingForResponseState(dot11->state)) {
                    // Do Nothing.
                } else if (
                    (!DirectionalNav_SignalIsNaved(
                        &dot11->directionalInfo->directionalNav,
                        MacDot11StationGetLastSignalsAngleOfArrivalFromRadio(
                        node,
                        dot11),
                        getSimTime(node))) &&
                    (PHY_MediumIsIdleInDirection(
                        node,
                        dot11->myMacData->phyNumber,
                        MacDot11StationGetLastSignalsAngleOfArrivalFromRadio(
                        node,
                        dot11))))
                {
                    // Transmit CTS only if NAV (software carrier sense)
                    // and the phy says the channel is idle.

                    MacDot11Trace(node, dot11, msg, "Receive");

                    // Received RTS, check if it already joined or not
                    if (!MacDot11IsAp(dot11))
                    {

                        // in Infrastructure mode...
                        if (MacDot11IsBssStation(dot11))
                        {
                            // Station joined, DOT11_S_M_BEACON
                            if (MacDot11IsStationNonAPandJoined(dot11) &&
                            hdr->sourceAddr == dot11->bssAddr)
                            {
                                //Received frame from AP. Time to measure signal strength
                                MacDot11StationUpdateAPMeasurement(
                                    node,
                                    dot11,
                                    msg);
                                MacDot11StationCancelTimer(node, dot11);
                                MacDot11StationTransmitCTSFrame(
                                    node,
                                    dot11,
                                    msg);
                            }
                        } else {

                            // In Ad hoc mode no need to check about association
                            MacDot11StationCancelTimer(node, dot11);
                            MacDot11StationTransmitCTSFrame(
                                node,
                                dot11,
                                msg);
                        }


                    } else {
                        MacDot11StationCancelTimer(node, dot11);
                        MacDot11StationTransmitCTSFrame(node, dot11, msg);
                   }
                }
            }

            MESSAGE_Free(node, msg);
            break;
        }

        case DOT11_CTS: {
#ifdef ADDON_DB
            /*HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
            
            HandleMacDBSummary(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
            HandleMacDBAggregate(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif

            MacDot11Trace(node, dot11, msg, "Receive");

            if ((dot11->state == DOT11_S_WFNAV) ||
                ((!((MacDot11StationPhyStatus(node, dot11) == PHY_IDLE) &&
                (MacDot11IsPhyIdle(node, dot11))))
                && (dot11->state == DOT11_S_WFCTS))) {
                // Cannot respond to this CTS
                // because NAV is set or Phy
                //isnot idle or state is not WF_CTS
                MESSAGE_Free(node, msg);
                break;
            }
#ifdef CYBER_LIB
            // In IA, the adversary may inject any packets, including
            // CTS packets.  It is invalid for QualNet to quit or warn
            // on any injected packet.
            if (dot11->state != DOT11_S_WFCTS)
            {
                // Ignore the packet
                MESSAGE_Free(node, msg);
                break;
            }
#else
            if (dot11->state != DOT11_S_WFCTS)
            {
                printf("Node %d cts failed because of state %d\n",
                node->nodeId,
                dot11->state);
            }
            ERROR_Assert(dot11->state == DOT11_S_WFCTS,
                "MacDot11ProcessMyFrame: "
                "Received unexpected CTS packet.\n");
#endif // CYBER_LIB

            if (dot11->useDvcs) {
                BOOL nodeIsInCache;

                MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                    node, dot11, dot11->waitingForAckOrCtsFromAddress,
                    &nodeIsInCache);

            }
            //Check if the frame is from AP and update measurement
            if (MacDot11IsStationNonAPandJoined(dot11) &&
                dot11->waitingForAckOrCtsFromAddress == dot11->bssAddr)
            {
                MacDot11StationUpdateAPMeasurement(node, dot11, msg);
            }
            MacDot11StationCancelTimer(node, dot11);

            dot11->SSRC = 0;
            if (MacDot11IsQoSEnabled(node, dot11) && dot11->currentACIndex >= DOT11e_AC_BK){
                dot11->ACs[dot11->currentACIndex].QSRC = dot11->SSRC;
            }
             if (MacDot11IsAp(dot11))
              {
                   DOT11_ApStationListItem* stationItem =
                   MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        dot11->waitingForAckOrCtsFromAddress);
                    if (stationItem) {

                        stationItem->data->LastFrameReceivedTime =
                                                      getSimTime(node);
                    }
                }

           // Check if there is a management frame to send
            MacDot11StationTransmitDataFrame(node, dot11);

            MESSAGE_Free(node, msg);
            break;
        }

        case DOT11_DATA:
        case DOT11_QOS_DATA:
        case DOT11_MESH_DATA:
        case DOT11_CF_DATA_ACK: {
//---------------------------Power-Save-Mode-Updates---------------------//
            if (MacDot11IsIBSSStationSupportPSMode(dot11)){
                MacDot11SetExchangeVariabe(node, dot11, msg);
            }
//---------------------------Power-Save-Mode-End-Updates-----------------//

            BOOL processFrame = MacDot11IsMyUnicastDataFrame(
                node,
                dot11,
                msg);

            // dot11s. Skip QoS and PCF unicasts.
            // MAPs have no current support for QoS or PCF.
            // Also, unicasts are not processed till initialized.
            if (dot11->isMP) {
                if (hdr->frameType == DOT11_QOS_DATA
                    ||  hdr->frameType == DOT11_CF_DATA_ACK)
                {
                    processFrame = FALSE;
                }
                else {
                    processFrame =
                        (MacDot11IsStationJoined(dot11) ? TRUE : FALSE);
                }
            }
            else {
                // Only MPs receive unicast mesh data frames.
                ERROR_Assert(hdr->frameType != DOT11_MESH_DATA,
                    "MacDot11ProcessMyFrame: "
                    "Non-MP receives mesh data unicast.\n");
            }

            if (processFrame) {
                Mac802Address sourceNodeAddress =
                    ((DOT11_FrameHdr*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;

                 if (MacDot11IsAp(dot11))
                 {
                   DOT11_ApStationListItem* stationItem =
                   MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        sourceNodeAddress);
                    if (stationItem) {

                            stationItem->data->LastFrameReceivedTime =
                                                          getSimTime(node);
                    }
                 }

                if (dot11->useDvcs) {
                    BOOL nodeIsInCache;

                    MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                        node, dot11, sourceNodeAddress,
                        &nodeIsInCache);
                }

                // This is not in the standard, but ns-2 does.
                // MacDot11ResetCW(node, dot11);
                // This is not in the standard, but ns-2 does.
                // dot11->SSRC = 0;

                MacDot11ProcessFrame(node, dot11, msg);
            }
            else {
                MacDot11Trace(node, dot11, NULL,
                    "Ignored directed data frame.");
#ifdef ADDON_DB
                HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_Drop, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                MESSAGE_Free(node, msg);
            }
            break;
        }

        case DOT11_ACK: {

#ifdef ADDON_DB
            /*HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
           
            HandleMacDBSummary(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);

            HandleMacDBAggregate(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
            MacDot11Trace(node, dot11, msg, "Receive");

#ifdef CYBER_LIB
            // In IA, the adversary may inject any packets, including
            // ACK packets.  It is invalid for QualNet to quit or warn
            // on any injected packet.
            if (dot11->state != DOT11_S_WFACK &&
               dot11->state != DOT11_S_WFPSPOLL_ACK)
            {
                // Ignore the packet
                MESSAGE_Free(node, msg);
                break;
            }
#else // CYBER_LIB
//---------------------------Power-Save-Mode-Updates---------------------//
            ERROR_Assert((dot11->state == DOT11_S_WFACK ||
                dot11->state == DOT11_S_WFPSPOLL_ACK),
                "MacDot11ProcessMyFrame: "
                "Received unexpected ACK packet.\n");
//---------------------------Power-Save-Mode-End-Updates-----------------//
#endif // CYBER_LIB
         if (MacDot11IsAp(dot11))
         {
               DOT11_ApStationListItem* stationItem =
               MacDot11ApStationListGetItemWithGivenAddress(
                    node,
                    dot11,
                    dot11->currentNextHopAddress);
                if (stationItem) {

                        stationItem->data->LastFrameReceivedTime =
                                                           getSimTime(node);
                }
         }

            if (dot11->useDvcs) {
                BOOL nodeIsInCache;

                MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                    node, dot11, dot11->waitingForAckOrCtsFromAddress,
                    &nodeIsInCache);

            }
            ERROR_Assert(dot11->dataRateInfo->ipAddress ==
                         dot11->currentNextHopAddress,
                         "Address does not match");

            ERROR_Assert(dot11->dataRateInfo->ipAddress ==
                         dot11->waitingForAckOrCtsFromAddress,
                         "Address does not match");

            dot11->dataRateInfo->numAcksInSuccess++;
            dot11->dataRateInfo->firstTxAtNewRate = FALSE;
            MacDot11StationCancelTimer(node, dot11);

            MacDot11StationProcessAck(node, dot11, msg);
            MESSAGE_Free(node, msg);
            break;
        }
//--------------------HCCA-Updates Start---------------------------------//
        case DOT11_QOS_DATA_POLL:
        case DOT11_QOS_CF_POLL:{
#ifdef ADDON_DB
        /*HandleMacDBEvents(        
            node, msg, 
            node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
            dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
            node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
        
        HandleMacDBSummary(node, msg,
            dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
            node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
        HandleMacDBAggregate(node, msg,
            dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
            node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
        MacDot11Trace(node, dot11, msg, "CAP POLL Receive");
        if (dot11->state == DOT11_S_WFCTS) {
            MacDot11Trace(node, dot11, NULL, "Wait for CTS aborted.");
            dot11->retxAbortedRtsDcf++;
        }
        if (dot11->state == DOT11_S_WFACK) {
                MacDot11StationResetAckVariables(dot11);
        }

        if (!MacDot11IsAp(dot11)){
            // in Infrastructure mode...
            if (MacDot11IsFrameFromMyAP(dot11,
                ((DOT11_LongControlFrame *)
                            MESSAGE_ReturnPacket(msg))->sourceAddr) &&
                   dot11->associatedWithHC){
                //Receive poll request from HC. Start the CAP
                MacDot11StationSetState(
                    node, dot11, DOT11e_CAP_START);
                MacDot11StationCancelTimer(node, dot11);

                MacDot11eCfpStationReceiveUnicast(node, dot11, msg);
            } else {
                    //Ignore, Connection with HC might be lost
            }

        } else {
                ERROR_ReportError(
                    "MacDot11ProcessMyFrame: POLL Receive at AP\n");
        }
        break;
    }
//--------------------HCCA-Updates End-----------------------------------//

//---------------------------Power-Save-Mode-Updates---------------------//
        case DOT11_PS_POLL: {
#ifdef ADDON_DB
            /*HandleMacDBEvents(        
                node, msg, 
                node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
            
            HandleMacDBSummary(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
            HandleMacDBAggregate(node, msg,
                dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
            dot11->psPollRequestsReceived++;
            if (DEBUG_PS && DEBUG_PS_PSPOLL){
                MacDot11Trace(node, dot11, msg, "AP Received PS POLL Frame");
            }
            Mac802Address sourceNodeAddress = ((DOT11_FrameHdr*)
                MESSAGE_ReturnPacket(msg))->sourceAddr;

             if (MacDot11IsAp(dot11))
             {
                   DOT11_ApStationListItem* stationItem =
                   MacDot11ApStationListGetItemWithGivenAddress(
                        node,
                        dot11,
                        sourceNodeAddress);
                    if (stationItem) {

                         stationItem->data->LastFrameReceivedTime =
                                                   getSimTime(node);
                    }
             }

            if (dot11->currentMessage != NULL){
                // Send PS POLL ACK
                MacDot11StationTransmitAck(node, dot11, sourceNodeAddress);
                MESSAGE_Free(node, msg);
                return;
            }

            // Tx buffered data packet here
            if (!MacDot11APDequeueUnicastPacket(
                    node,
                    dot11,
                    sourceNodeAddress)){
                // Send PS POLL ACK
                MacDot11StationTransmitAck(node, dot11, sourceNodeAddress);

                MESSAGE_Free(node, msg);
                return;
            }

            if (dot11->useDvcs) {
                BOOL nodeIsInCache;

                MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                    node, dot11, dot11->waitingForAckOrCtsFromAddress,
                    &nodeIsInCache);
            }
            MacDot11StationCancelTimer(node, dot11);

            dot11->SSRC = 0;
            dot11->dataRateInfo = MacDot11StationGetDataRateEntry(
                node, dot11, dot11->currentNextHopAddress);
            ERROR_Assert(dot11->dataRateInfo->ipAddress ==
                        dot11->currentNextHopAddress,
                        "Address does not match");
            MacDot11StationAdjustDataRateForNewOutgoingPacket(node,
                                                              dot11);
           // Check if there is a management frame to send
            if (MacDot11StationHasManagementFrameToSend(node,dot11) ){
                MacDot11StationTransmitDataFrame(node, dot11);
            } else {
                MacDot11StationTransmitDataFrame(node, dot11);
            }

            MESSAGE_Free(node, msg);
            break;
        }
//---------------------------Power-Save-Mode-End-Updates-----------------//

        default:
        {
            char errString[MAX_STRING_LENGTH];

            // dot11s. Hook for mgmt unicasts.
            // If a mesh management frame, process.
            // Else, prevent an MAP handling currently implemented
            // management frames till AP services are initialized.
            if (dot11->isMP) {
#ifdef ADDON_DB
                Message *dbMsg = MESSAGE_Duplicate(node, msg);
#endif
                if (Dot11s_ReceiveMgmtUnicast(node, dot11, msg)) {
                    dot11->unicastPacketsGotDcf++;
#ifdef ADDON_DB
                /*HandleMacDBEvents(        
                    node, dbMsg, 
                    node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);*/
                
                HandleMacDBSummary(node, dbMsg,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
                HandleMacDBAggregate(node, dbMsg,
                    dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy,
                    node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                    return;
                }
                else {
                    if (MacDot11IsStationJoined(dot11) == FALSE) {
                        MacDot11Trace(node, dot11, NULL,
                            "MP: AP services not started. "
                            "Dropping management frame.");
#ifdef ADDON_DB
                       
                        HandleMacDBEvents(        
                            node, msg, 
                            node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
                            dot11->myMacData->interfaceIndex, MAC_Drop, 
                            node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
                        MESSAGE_Free(node, msg);
                        return;
                    }
                }
#ifdef ADDON_DB
                MESSAGE_Free(node, dbMsg);
#endif
            }

            // Check if this is a Management frame that needs processing
            if (MacDot11ManagementCheckFrame(node, dot11, msg))
                return;
            // FIXME: This does work right now
            // For a BSS station that could have missed a beacon,
            // enter CFP and process CF frame as normal.
            if (MacDot11IsBssStation(dot11) &&
                MacDot11IsFrameFromMyAP(dot11,
                    ((DOT11_LongControlFrame*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr) &&
                hdr->duration == DOT11_CF_FRAME_DURATION)
            {
                MacDot11Trace(node, dot11, msg,
                    "BSS station misses beacon, goes into CFP");

                // Enter CFP and process frame as normal
                MacDot11CfpStationInitiateCfPeriod(node, dot11);
                MacDot11CfpReceivePacketFromPhy(node, dot11, msg);
                return;
            }
            else if (hdr->duration == DOT11_CF_FRAME_DURATION)
            {
                return ;
            }
            sprintf(errString,
                "MacDot11ProcessMyFrame: "
                "Received unknown unicast frame type %d\n",
                hdr->frameType);
            ERROR_ReportError(errString);

            break;
        }
    }//switch
}//MacDot11ProcessMyFrame

//--------------------------------------------------------------------------
//  NAME:        MacDot11ReceivePacketFromPhy
//  PURPOSE:     Receive a packet from physical layer
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               Mac802Address address
//                  Node address of station
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
void MacDot11ReceivePacketFromPhy(
    Node* node,
    MacDataDot11* dot11,
    Message* msg)
{
    DOT11_ShortControlFrame* hdr =
        (DOT11_ShortControlFrame*) MESSAGE_ReturnPacket(msg);

    dot11->IsInExtendedIfsMode = FALSE;

    //
    // Since in QualNet it's possible to have two events occurring
    // at the same time, enforce the fact that when a node is
    // transmitting, a node can't be receiving a frame at the same time.
    ERROR_Assert(!(MacDot11IsTransmittingState(dot11->state)||
        MacDot11IsCfpTransmittingState(dot11->cfpState)),
        "MacDot11ReceivePacketFromPhy: "
        "Cannot receive packet while in transmit state.\n");
#ifdef ADDON_DB
    HandleMacDBEvents(        
            node, msg, 
            node->macData[dot11->myMacData->interfaceIndex]->phyNumber,
            dot11->myMacData->interfaceIndex, MAC_ReceiveFromPhy, 
            node->macData[dot11->myMacData->interfaceIndex]->macProtocol);
#endif
#ifdef PAS_INTERFACE
#ifndef _WIN32
    if (PAS_LayerCheck(node, dot11->myMacData->interfaceIndex, PACKET_SNIFFER_DOT11))
    {
        PAS_CreateFrames(node, msg);
    }
#endif
#endif

    // Check if we're in CFP
    if (MacDot11IsInCfp(dot11->state)) {
        MacDot11CfpReceivePacketFromPhy(node, dot11, msg);
        return;
    }

//--------------------HCCA-Updates Start---------------------------------//
    if (MacDot11IsInCap(dot11->state)) {
        MacDot11eCFPReceivePacketFromPhy(node, dot11, msg);
        return;
    }
//--------------------HCCA-Updates End-----------------------------------//
    BOOL isMyAddr = FALSE;

    if (NetworkIpIsUnnumberedInterface(
            node, dot11->myMacData->interfaceIndex))
    {
        MacHWAddress linkAddr;
        Convert802AddressToVariableHWAddress(
                                node, &linkAddr, &(hdr->destAddr));

        isMyAddr = MAC_IsMyAddress(node, &linkAddr);
    }
    else
    {
        isMyAddr = (dot11->selfAddr == hdr->destAddr);
    }
    if (isMyAddr) {
    //if (dot11->selfAddr == hdr->destAddr) {
        MacDot11ProcessMyFrame (node, dot11, msg);
    }
    else if (hdr->destAddr == ANY_MAC802) {
        MacDot11ProcessAnyFrame (node, dot11, msg);
    }
    else {
        // Does not belong to this node

//--------------------HCCA-Updates Start---------------------------------//
        MacDot11ProcessNotMyFrame(
            node, dot11, MacDot11MicroToNanosecond(hdr->duration),
            (hdr->frameType == DOT11_RTS),
            (hdr->frameType == DOT11_QOS_CF_POLL));
//--------------------HCCA-Updates End-----------------------------------//

        if (dot11->useDvcs) {
            Mac802Address sourceNodeAddress = INVALID_802ADDRESS;

            if (hdr->frameType == DOT11_RTS) {
                sourceNodeAddress =
                    ((DOT11_LongControlFrame*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
            }
            else if (hdr->frameType == DOT11_DATA
                  || hdr->frameType == DOT11_MESH_DATA)   // dot11s
            {
                sourceNodeAddress =
                    ((DOT11_FrameHdr*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
            }
            else if (hdr->frameType == DOT11_QOS_DATA) {
                sourceNodeAddress =
                    ((DOT11e_FrameHdr*)MESSAGE_ReturnPacket(msg))
                    ->sourceAddr;
            }

            if (sourceNodeAddress != INVALID_802ADDRESS) {
                BOOL nodeIsInCache;

                MacDot11StationUpdateDirectionCacheWithCurrentSignal(
                    node, dot11, sourceNodeAddress,
                    &nodeIsInCache);
            }
        }

        if (dot11->myMacData->promiscuousMode == TRUE) {
            DOT11_FrameHdr* fHdr = (DOT11_FrameHdr*)hdr;

            BOOL sendForPromiscuousMode =
                    MacDot11IsFrameAUnicastDataTypeForPromiscuousMode(
                        (DOT11_MacFrameType) hdr->frameType, msg);

            // dot11s. Restrict promiscuous handling of mesh data to MPs.
            // Only MPs send mesh data frames for a promiscuous view.
            if (hdr->frameType == DOT11_MESH_DATA) {
                if (dot11->isMP == FALSE) {
                    sendForPromiscuousMode = FALSE;
                }
                else {
                    // Filter out duplicate mesh unicast frames.
                    if (Dot11sDataSeenList_Lookup(node, dot11, msg) != NULL) {
                        sendForPromiscuousMode = FALSE;
                    }
                    else {
                        Dot11sDataSeenList_Insert(node, dot11, msg);
                        sendForPromiscuousMode = TRUE;
                    }
                }
            }

            if (sendForPromiscuousMode) {
                MacDot11HandlePromiscuousMode(node, dot11, msg,
                    fHdr->sourceAddr, fHdr->destAddr);
            }
        }

        MESSAGE_Free(node, msg);
    }

    if (dot11->useDvcs && dot11->state == DOT11_S_IDLE) {
        MacDot11StationSetPhySensingDirectionForNextOutgoingPacket(
            node,
            dot11);
    }

    if (MacDot11StationPhyStatus(node, dot11) == PHY_IDLE &&
        dot11->state == DOT11_S_IDLE) {
        MacDot11StationPhyStatusIsNowIdleStartSendTimers(node, dot11);
    }
}//MacDot11ReceivePacketFromPhy//


//--------------------------------------------------------------------------
//  NAME:        MacDot11ReceivePhyStatusChangeNotification
//  PURPOSE:     React to change in physical status.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               PhyStatusType oldPhyStatus
//                  The previous physical state
//               PhyStatusType newPhyStatus
//                  New physical state
//               clocktype receiveDuration
//                  Receiving duration
//               const Message* potentialIncomingPacket
//                  Incoming packet
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
void MacDot11ReceivePhyStatusChangeNotification(
    Node* node,
    MacDataDot11* dot11,
    PhyStatusType oldPhyStatus,
    PhyStatusType newPhyStatus,
    clocktype receiveDuration,
    const Message* potentialIncomingPacket)
{
    switch (oldPhyStatus) {
        case PHY_TRX_OFF: {
            switch(newPhyStatus)
            {
                case PHY_SENSING:
                case PHY_IDLE :{
                    break;
                }
                default: {
                    ERROR_ReportError(
                        "MacDot11ReceivePhyStatusChangeNotification: "
                        "Invalid physical state change from PHY_TRX_OFF.\n");
                    break;
                }
            }
            break;
        }
        case PHY_IDLE: {
            switch (newPhyStatus) {
                case PHY_SENSING: {
                    MacDot11StationCancelSomeTimersBecauseChannelHasBecomeBusy
                        (node, dot11);
                    break;
                }

                case PHY_RECEIVING: {

                    MacDot11StationCancelSomeTimersBecauseChannelHasBecomeBusy(
                        node,
                        dot11);
                    MacDot11StationExaminePotentialIncomingMessage(
                        node, dot11, newPhyStatus,
                        receiveDuration, potentialIncomingPacket);
                    break;
                }

                case PHY_TRANSMITTING: {
                    break;
                }

                default: {
                    ERROR_ReportError(
                        "MacDot11ReceivePhyStatusChangeNotification: "
                        "Invalid physical state change from IDLE.\n");
                    break;
                }
            }//switch//
            break;
        }

        case PHY_SENSING: {
            switch (newPhyStatus) {
                case PHY_IDLE: {
                    MacDot11StationPhyStatusIsNowIdleStartSendTimers(
                        node,
                        dot11);
                    break;
                }
                case PHY_RECEIVING: {
                    if (dot11->state == DOT11e_CAP_START)
                    {
                        MacDot11StationCancelSomeTimersBecauseChannelHasBecomeBusy
                           (node, dot11);
                    }
                    MacDot11StationExaminePotentialIncomingMessage(
                        node,
                        dot11,
                        newPhyStatus,
                        receiveDuration,
                        potentialIncomingPacket);
                    break;
                }
                case PHY_TRANSMITTING: {
                    // Dot11 will transmit ACKs and CTS even when sensing.
                    break;
                }
                default: {
                    ERROR_ReportError(
                        "MacDot11ReceivePhyStatusChangeNotification: "
                        "Invalid physical state change from SENSING.\n");
                    break;
                }
            }//switch//

            break;
        }

        case PHY_RECEIVING: {

            switch (newPhyStatus) {
                case PHY_IDLE: {

                    MacDot11StationPhyStatusIsNowIdleStartSendTimers
                        (node, dot11);
                    break;
                }

                case PHY_SENSING: {
                    // Receive was canceled and packet dropped.
                    if (dot11->state == DOT11_S_HEADER_CHECK_MODE) {
                        MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
                        MacDot11StationCancelTimer(node, dot11);
                    }

                    break;
                }

                case PHY_RECEIVING: {
                    // "Captured" stronger packet.
                    MacDot11StationExaminePotentialIncomingMessage(
                        node, dot11, newPhyStatus,
                        receiveDuration, potentialIncomingPacket);
                    break;
                }

                default: {
                    ERROR_ReportError(
                        "MacDot11StationReceivePhyStatusChangeNotification: "
                        "Invalid physical state change from RECEIVING.\n");
                    break;
                }
            }//switch//

            break;
        }

        case PHY_TRANSMITTING: {
            switch (newPhyStatus) {
                case PHY_IDLE:
                case PHY_SENSING:
                case PHY_RECEIVING:
                {
                    MacDot11StationTransmissionHasFinished(node, dot11);
                    break;
                }

                default: {
                    ERROR_ReportError(
                        "MacDot11ReceivePhyStatusChangeNotification: "
                        "Invalid physical state change from TRANSMITTING.\n");
                    break;
                }
            }//switch//

            break;
        }

        default: {
            ERROR_ReportError(
                "MacDot11ReceivePhyStatusChangeNotification: "
                "Invalid old physical state.\n");
            break;
        }
    }//switch//


}//MacDot11ReceivePhyStatusChangeNotification//



//--------------------------------------------------------------------------
//  NAME:        MacDot11HandleTimeout.
//  PURPOSE:     Handle expiry of timer depending on state.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
static //inline//
void MacDot11HandleTimeout(
    Node* node,
    MacDataDot11* dot11,
    Message* msg)
{

    if (dot11->beaconIsDue
        && MacDot11StationCanHandleDueBeacon(node, dot11))
    {
        return;
    }

    if (dot11->useDvcs) {
        //
        // A Packet sequence could possibly have failed,
        // go back to sending/receiving omnidirectionally if not already
        // doing so.
        //
        PHY_UnlockAntennaDirection(
            node, dot11->myMacData->phyNumber);
    }

    switch (dot11->state) {
        case DOT11_S_WF_DIFS_OR_EIFS: {
            if (dot11->BO == 0) {
                if (MacDot11StationHasFrameToSend(dot11)) {
                    if (MacDot11IsQoSEnabled(node, dot11)){
                        if (dot11->currentACIndex >= DOT11e_AC_BK) {
                            MacDot11PauseOtherAcsBo(
                                 node, dot11, 0);
                        }
                    }
                    MacDot11StationTransmitFrame(node, dot11);
                }
                else {
                    MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
                }
            }
            else {
                MacDot11StationContinueBackoff(node, dot11);
                MacDot11StationSetState(node, dot11, DOT11_S_BO);
                MacDot11StationStartTimer(node, dot11, dot11->BO);
//---------------------------Power-Save-Mode-Updates---------------------//
                if (DEBUG_PS && DEBUG_PS_BO){
                    MacDot11Trace(node, dot11, NULL, "BO");
                }
//---------------------------Power-Save-Mode-End-Updates-----------------//
            }

            break;
        }

        case DOT11_S_BO: {
            ERROR_Assert(
                (dot11->lastBOTimeStamp + dot11->BO) == getSimTime(node),
                "MacDot11HandleTimeout: "
                "Backoff period does not match simulation time.\n");
            if (MacDot11IsQoSEnabled(node, dot11)){
                if (dot11->currentACIndex >= DOT11e_AC_BK) {
                    MacDot11PauseOtherAcsBo(
                         node, dot11, dot11->ACs[dot11->currentACIndex].BO);
                    dot11->ACs[dot11->currentACIndex].BO = 0;
                }
            }
            else if (dot11->currentACIndex != DOT11e_INVALID_AC)
            {
                dot11->ACs[dot11->currentACIndex].BO = 0;
            }

            dot11->BO = 0;

            if (MacDot11StationHasFrameToSend(dot11)) {
                MacDot11StationTransmitFrame(node, dot11);
            }
            else {
                MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
            }

            break;
        }

        case DOT11_S_WFACK: {
            ERROR_Assert(dot11->dataRateInfo->ipAddress ==
                         dot11->currentNextHopAddress,
                         "Address does not match");

            dot11->dataRateInfo->numAcksFailed++;
            MacDot11StationAdjustDataRateForResponseTimeout(node, dot11);
            dot11->retxDueToAckDcf++;
            MacDot11Trace(node, dot11, NULL, "Retransmit wait for ACK");

//---------------------------Power-Save-Mode-Updates---------------------//
            MacDot11StationRetransmit(node, dot11);
//---------------------------Power-Save-Mode-End-Updates-----------------//
            break;
        }

        case DOT11_S_NAV_RTS_CHECK_MODE:
        case DOT11_S_WFNAV:
        {
            dot11->NAV = 0;
//--------------------HCCA-Updates Start---------------------------------//
            if (!MacDot11HcAttempToStartCap(node,dot11)){
                if (MacDot11StationHasFrameToSend(dot11)) {
                        MacDot11StationAttemptToGoIntoWaitForDifsOrEifsState(
                            node, dot11);
                }
                else {
                    MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
                }
//--------------------HCCA-Updates End-----------------------------------//
            }
            break;
        }

        case DOT11_S_WFCTS: {
            ERROR_Assert(dot11->dataRateInfo->ipAddress ==
                         dot11->currentNextHopAddress,
                         "Address does not match");

            dot11->dataRateInfo->numAcksFailed++;
            MacDot11StationAdjustDataRateForResponseTimeout(node, dot11);
            dot11->retxDueToCtsDcf++;
            MacDot11Trace(node, dot11, NULL, "Retransmit wait for CTS");
            MacDot11StationRetransmit(node, dot11);
            break;
        }

        case DOT11_S_WFDATA: {
            MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
            MacDot11StationCheckForOutgoingPacket(node, dot11, FALSE);
            break;
        }

        case DOT11_CFP_START: {
            MacDot11CfpHandleTimeout(node, dot11);
            break;
        }

//--------------------HCCA-Updates Start---------------------------------//
        case DOT11e_WF_CAP_START:
        {
            if (MacDot11StationPhyStatus(node, dot11) != PHY_IDLE)
            {
                MacDot11StationCancelTimer(node, dot11);
                MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
            }
            else
            {
                MacDot11StationSetState(node, dot11, DOT11e_CAP_START);
                dot11->hcVars->capStartTime = getSimTime(node);
                dot11->hcVars->capEndTime += dot11->txPifs;
                MacDot11eCfpHandleTimeout(node,dot11);
            }
            break;
         }
        case DOT11e_CAP_START:{
            MacDot11eCfpHandleTimeout(node,dot11);
            break;
          }
//--------------------HCCA-Updates End-----------------------------------//
        case DOT11_S_HEADER_CHECK_MODE: {
            const DOT11_LongControlFrame* hdr =
                (const DOT11_LongControlFrame*)
                MESSAGE_ReturnHeader(dot11->potentialIncomingMessage, 1);

            BOOL frameHeaderHadError;
            clocktype endSignalTime;

            BOOL packetForMe =
                ((hdr->destAddr == dot11->selfAddr) ||
                 (hdr->destAddr == ANY_MAC802));

            ERROR_Assert((hdr->frameType != DOT11_CTS) &&
                         (hdr->frameType != DOT11_ACK),
                         "Frame type should not be CTS or ACK");

            PHY_TerminateCurrentReceive(
                node, dot11->myMacData->phyNumber, packetForMe,
                &frameHeaderHadError, &endSignalTime);

            if (MacDot11StationPhyStatus(node, dot11) == PHY_RECEIVING) {
                // Keep Receiving (wait).
                MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
            }
            else {
                dot11->noOutgoingPacketsUntilTime = endSignalTime;

                if (DEBUG) {
                    char timeStr[MAX_STRING_LENGTH];
                    TIME_PrintClockInSecond(getSimTime(node), timeStr);

                    if (packetForMe) {
                        printf("%d : %s For Me/Error\n",
                               node->nodeId, timeStr);
                    }
                    else if (frameHeaderHadError) {
                        printf("%d : %s For Other/Error\n",
                               node->nodeId, timeStr);
                    }
                    else if (dot11->currentMessage != NULL) {
                        printf("%d : %s For Other/Have Message\n",
                               node->nodeId, timeStr);
                    }
                    else {
                        printf("%d : %s For Other/No Message\n",
                               node->nodeId, timeStr);
                    }
                }

                if (!frameHeaderHadError) {
                    clocktype headerCheckDelay =
                        PHY_GetTransmissionDuration(
                            node,
                            dot11->myMacData->phyNumber,
                            PHY_GetRxDataRateType(
                                node, dot11->myMacData->phyNumber),
                            DOT11_SHORT_CTRL_FRAME_SIZE);


                    // Extended IFS mode if for backing off more when a
                    // collision happens and a packet is garbled.
                    // We are forcably terminating the packet and thus
                    // we should not use extended IFS delays.

                    dot11->IsInExtendedIfsMode = FALSE;

                    // It should be able to learn AOA, but temporarily disabled.
                    // UpdateDirectionCacheWithCurrentSignal(
                    //     node, dot11, hdr->sourceAddr, TRUE);

//--------------------HCCA-Updates Start---------------------------------//
                    MacDot11ProcessNotMyFrame(
                        node, dot11,
                        (MacDot11MicroToNanosecond(hdr->duration)
                            - headerCheckDelay),
                        (hdr->frameType == DOT11_RTS),
                        (hdr->frameType == DOT11_QOS_CF_POLL));
//--------------------HCCA-Updates End-----------------------------------//
                }
                else {
                    // Header had error.  Assume no information received.
                }

                MacDot11StationCheckForOutgoingPacket(node, dot11, FALSE);
            }

            break;
        }

        case DOT11_S_WFJOIN: {
            MacDot11ManagementHandleTimeout(node, dot11,msg);
            break;
        }

        case DOT11_S_WFMANAGEMENT: {

            dot11->dataRateInfo->numAcksFailed++;
            MacDot11StationAdjustDataRateForResponseTimeout(node, dot11);
            dot11->retxDueToAckDcf++;
            MacDot11Trace(node, dot11, NULL, "Retransmit wait for management frame");
            MacDot11StationRetransmit(node, dot11);
            break;
        }

        case DOT11_S_WFBEACON: {
            ERROR_Assert( MacDot11IsAp(dot11),
                "MacDot11HandleTimeout: "
                "Wait For Beacon state for an AP or non-BSS station.\n");

            MacDot11ApAttemptToTransmitBeacon(node, dot11);

            break;
        }
//---------------------------Power-Save-Mode-Updates---------------------//
        case DOT11_S_WFIBSSBEACON: {

            MacDot11StationAttemptToTransmitBeacon(node, dot11);
            break;
        }
        case DOT11_S_WFIBSSJITTER:{
            MacDot11StationSetState(node, dot11, DOT11_S_WFIBSSBEACON);
            MacDot11StationStartTimer(node, dot11, dot11->txSifs);
            break;
        }
        case DOT11_S_WFPSPOLL_ACK:{
            MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
            break;
        }
//---------------------------Power-Save-Mode-End-Updates-----------------//
        default: {
            printf("MAC_DOT11: Node %u got unknown state type %d\n",
                   node->nodeId, dot11->state);

            ERROR_ReportError("MacDot11HandleTimeout: "
                "Timeout in unexpected state.\n");
        }
    }


}//MacDot11HandleTimeout//
//--------------------------------------------------------------------------
//  NAME:        MacDot11DeleteAssociationId.
//  PURPOSE:     Handle timers and layer messages.
//  PARAMETERS:  Node* node
//                  Node handling the incoming messages
//               int interfaceIndex
//                  Interface index
//               Message* msg
//                  Message for node to interpret.
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
void MacDot11DeleteAssociationId(Node* node,
     MacDataDot11* dot11,
     DOT11_ApStationListItem* stationItem)
 {
     //DOT11_ApVars* ap = dot11->apVars;
    int arrayIndex = stationItem->data->assocId / 8;
    int bitLocation = (stationItem->data->assocId % 8);
    UInt8 tempVal = dot11->apVars->AssociationVector[arrayIndex];
    if (((tempVal >> bitLocation) & 0x01) == 0x01)
    {
        tempVal ^= (0x01 << bitLocation);
    }
    dot11->apVars->AssociationVector[arrayIndex] = tempVal;
 }
//--------------------------------------------------------------------------
//  NAME:        MacDot11Layer.
//  PURPOSE:     Handle timers and layer messages.
//  PARAMETERS:  Node* node
//                  Node handling the incoming messages
//               int interfaceIndex
//                  Interface index
//               Message* msg
//                  Message for node to interpret.
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
void MacDot11Layer(Node* node, int interfaceIndex, Message* msg)
{
    MacDataDot11* dot11 =
        (MacDataDot11*)node->macData[interfaceIndex]->macVar;

    // dot11s. Some Mesh/HWMP timers do not use sequence numbers.
    //unsigned timerSequenceNumber = *(int*)(MESSAGE_ReturnInfo(msg));

    switch (msg->eventType) {
        case MSG_MAC_TimerExpired: {
            unsigned timerSequenceNumber =
                (unsigned)(*(int*)(MESSAGE_ReturnInfo(msg)));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");
//---------------------------Power-Save-Mode-Updates---------------------//
            if (DEBUG_PS_TIMERS) {
            MacDot11Trace(
                node,
                dot11,
                NULL,
                "MSG_MAC_TimerExpired Timer expired");
            }
//---------------------------Power-Save-Mode-End-Updates-----------------//
            if (timerSequenceNumber == dot11->timerSequenceNumber) {
                MacDot11HandleTimeout(node, dot11,msg);
            }

            MESSAGE_Free(node, msg);
            break;
        }

//---------------------------Power-Save-Mode-Updates---------------------//
        case MSG_MAC_DOT11_Beacon: {
            if (DEBUG_PS_TIMERS) {
            MacDot11Trace(
                node,
                dot11,
                NULL,
                "MSG_MAC_DOT11_Beacon Timer expired");
                }

            unsigned timerSequenceNumber =
                 (unsigned)(*(int*)(MESSAGE_ReturnInfo(msg)));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");

            if (timerSequenceNumber == dot11->beaconSequenceNumber) {
                dot11->beaconIsDue = TRUE;

                if (MacDot11IsIBSSStationSupportPSMode(dot11)){
                    dot11->stationHasReceivedBeconInAdhocMode = FALSE;

                    if (dot11->currentMessage != NULL){
                        if (!(MacDot11IsTransmittingState(dot11->state)
                            || MacDot11IsWaitingForResponseState(dot11->state))){
                            MacDot11StationResetCurrentMessageVariables(
                                node,
                                dot11);
                            MacDot11StationSetState(
                                node,
                                dot11,
                                DOT11_S_IDLE);
                            MacDot11StationResetCW(node, dot11);
                        }// end of if
                    }

                    // If STA in sleep mode then start listening the
                    //      transmission channel.
                    MacDot11StationStartListening(node, dot11);
                    // Start timer to check,
                    // in a Beacon Period STA has received it or not.
                    dot11->beaconTime += dot11->beaconInterval;

                    MacDot11StationStartTimerOfGivenType(
                        node,
                        dot11,
                        MacDot11TUsToClocktype(dot11->atimDuration),
                        MSG_MAC_DOT11_ATIMWindowTimerExpired);
                }
                MacDot11StationCanHandleDueBeacon(node, dot11);
          }

          MESSAGE_Free(node, msg);
          break;
        }
        case MSG_MAC_DOT11_ATIMWindowTimerExpired: {

            if (DEBUG_PS_TIMERS) {
                MacDot11Trace(
                    node,
                    dot11,
                    NULL,
                    "MSG_MAC_DOT11_ATIMWindowTimerExpired Timer expired");
            }

            unsigned timerSequenceNumber =
                (unsigned)(*(int*)(MESSAGE_ReturnInfo(msg)));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");

            MacDot11IBSSStationStartBeaconTimer(node, dot11);

               dot11->isIBSSATIMDurationNeedToEnd = TRUE;

            dot11->beaconIsDue = FALSE;

                // send buffered packet here
                MacDot11StationCheckForOutgoingPacket(
                    node,
                    dot11,
                    FALSE);
       }
       MESSAGE_Free(node, msg);
       break;
    // Start listening the transmission channel
        case MSG_MAC_DOT11_PSStartListenTxChannel: {

            MacDot11Trace(
                node,
                dot11,
                NULL,
                "MSG_MAC_DOT11_PSStartListenTxChannel Timer expired");

            unsigned timerSequenceNumber =
                (unsigned)(*(int*)(MESSAGE_ReturnInfo(msg)));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");

            if (DEBUG_PS_TIMERS) {
                MacDot11Trace(
                    node,
                    dot11,
                    NULL,
                    "MSG_MAC_DOT11_PSStartListenTxChannel Timer expired");
            }

            if (timerSequenceNumber == dot11->awakeTimerSequenceNo){
                // If STA in sleep mode then start listening the
                //      transmission channel
                MacDot11StationStartListening(node, dot11);
            }
            MESSAGE_Free(node, msg);
            break;
        }// end of case MSG_MAC_DOT11_ PSStartListenTxChannel

//---------------------------Power-Save-Mode-End-Updates-----------------//

        case MSG_MAC_DOT11_Management: {
            unsigned timerSequenceNumber =
                (unsigned)(*(int*)(MESSAGE_ReturnInfo(msg)));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");

//---------------------------Power-Save-Mode-Updates---------------------//

            if (DEBUG_PS_TIMERS) {
            MacDot11Trace(
                node,
                dot11,
                NULL,
                "MSG_MAC_DOT11_Management Timer expired");
            }

//---------------------------Power-Save-Mode-End-Updates-----------------//
            if (timerSequenceNumber == dot11->managementSequenceNumber) {
                dot11->mgmtSendResponse = FALSE;
                MacDot11ManagementHandleTimeout(node, dot11,msg);
            }
            MESSAGE_Free(node, msg);
            break;
        }
        case MSG_MAC_DOT11_CfpEnd: {
            unsigned timerSequenceNumber = (unsigned)*(int*)(MESSAGE_ReturnInfo(msg));
            ERROR_Assert(timerSequenceNumber <= dot11->timerSequenceNumber,
                "MacDot11Layer: Received invalid timer message.\n");

            if (timerSequenceNumber == dot11->cfpEndSequenceNumber) {
                MacDot11CfpHandleEndTimer(node, dot11);
            }

            MESSAGE_Free(node, msg);
            break;
        }

        case MSG_MAC_DOT11s_AssocRetryTimeout:
        case MSG_MAC_DOT11s_AssocOpenTimeout:
        case MSG_MAC_DOT11s_AssocCancelTimeout:
        case MSG_MAC_DOT11s_LinkSetupTimer:
        case MSG_MAC_DOT11s_LinkStateTimer:
        case MSG_MAC_DOT11s_PathSelectionTimer:
        case MSG_MAC_DOT11s_InitCompleteTimeout:
        case MSG_MAC_DOT11s_MaintenanceTimer:
        case MSG_MAC_DOT11s_PannTimer:
        case MSG_MAC_DOT11s_PannPropagationTimeout:
        case MSG_MAC_DOT11s_LinkStateFrameTimeout:
        case MSG_MAC_DOT11s_QueueAgingTimer:
        case MSG_MAC_DOT11s_HwmpActiveRouteTimeout:
        case MSG_MAC_DOT11s_HwmpDeleteRouteTimeout:
        case MSG_MAC_DOT11s_HwmpRreqReplyTimeout:
        case MSG_MAC_DOT11s_HwmpSeenTableTimeout:
        case MSG_MAC_DOT11s_HwmpBlacklistTimeout:
        case MSG_MAC_DOT11s_HwmpTbrEventTimeout:
        case MSG_MAC_DOT11s_HwmpTbrRannTimeout:
        case MSG_MAC_DOT11s_HwmpTbrMaintenanceTimeout:
        case MSG_MAC_DOT11s_HmwpTbrRrepTimeout:
        {
            Dot11s_HandleTimeout(node, dot11, msg);
            break;
        }
        case MSG_MAC_DOT11_Station_Inactive_Timer:
        {
            clocktype timerDelay = DOT11_MANAGEMENT_STATION_INACTIVE_TIMEOUT;
            DOT11_ApStationListItem* stationItem;
            if (MacDot11IsAp(dot11))
            {

                DOT11_ApVars* ap = dot11->apVars;
                DOT11_ApStationListItem* next = NULL;
                DOT11_ApStationListItem* first;

                if (!MacDot11ApStationListIsEmpty(
                    node,
                    dot11,
                    ap->apStationList))
                {
                    next = first = MacDot11ApStationListGetFirst(node,
                        dot11);

                    while (next != NULL)
                    {
                        stationItem = next;
                        next = next->next;

                        if ((getSimTime(node)
                         - stationItem->data->LastFrameReceivedTime)
                        >timerDelay)
                        {
                            ap->stationCount--;
                          MacDot11DeleteAssociationId(
                                       node,
                                       dot11,
                                       stationItem);
                          MacDot11ApStationListItemRemove(
                                     node,
                                     dot11,
                                     ap->apStationList,
                                     stationItem);

                         }// end of if
                    } // end of while
                }

                MacDot11StationStartStationCheckTimer(
                    node,
                    dot11,
                    timerDelay,
                    MSG_MAC_DOT11_Station_Inactive_Timer);
                dot11->stationCheckTimerStarted = TRUE;
           }
           MESSAGE_Free(node, msg);
           break;
        }

        case MSG_MAC_DOT11_Probe_Delay_Timer:
        {

            unsigned timerSequenceNumber = *(int*)(MESSAGE_ReturnInfo(msg));

            if (timerSequenceNumber == dot11->timerSequenceNumber)
            {
                DOT11_ManagementVars * mngmtVars =
                    (DOT11_ManagementVars*) dot11->mngmtVars;
                if (mngmtVars)
                {
                    mngmtVars->probeResponseRecieved = FALSE;
                    mngmtVars->channelInfo->dwellTime =
                                      DOT11_SHORT_SCAN_TIMER_DEFAULT;
                }

                dot11->waitForProbeDelay = FALSE;
                MacDot11StationCheckForOutgoingPacket(node, dot11, FALSE);
                dot11->ActiveScanShortTimerFunctional = FALSE;
                dot11->MayReceiveProbeResponce = FALSE;
            }
            MESSAGE_Free(node, msg);
            break;
        }
//---------------------------Power-Save-Mode-End-Updates-----------------//
        case MSG_MAC_DOT11_Active_Scan_Short_Timer:
        case MSG_MAC_DOT11_Active_Scan_Long_Timer:
        case MSG_MAC_DOT11_Management_Authentication:
        case MSG_MAC_DOT11_Management_Association:
        case MSG_MAC_DOT11_Management_Reassociation:
        case MSG_MAC_DOT11_Beacon_Wait_Timer:
        case MSG_MAC_DOT11_Authentication_Start_Timer:
        case MSG_MAC_DOT11_Reassociation_Start_Timer:
        case MSG_MAC_DOT11_Scan_Start_Timer:
        case MSG_MAC_DOT11_Enable_Management_Timer:
        {
            unsigned timerSequenceNumber = *(int*)(MESSAGE_ReturnInfo(msg));

//---------------------------Power-Save-Mode-End-Updates-----------------//
            if (timerSequenceNumber == dot11->managementSequenceNumber)
            {
                 MacDot11ManagementHandleTimeout(node, dot11,msg);
            }
            MESSAGE_Free(node, msg);
            break;
        }

#ifdef ADDON_MIH
        case MSG_Link_Get_Parameters_request:
        {
        	local_link_parameters *param_req =(local_link_parameters *)MESSAGE_ReturnInfo(msg);
        	DOT11_ApInfo apInfo;

        	if(param_req->linkParamReq->elem->selector == 1)
        	{
        		ParamTypeValue value = param_req->linkParamReq->elem->value;

        		l_LINK_PARAM *LinkParametersStatusList =  (l_LINK_PARAM *) MEM_malloc(sizeof(l_LINK_PARAM));
        		memset(LinkParametersStatusList, 0, sizeof(l_LINK_PARAM));
        		LinkParametersStatusList->length = 1;

        		if (value.lp_gen == 2) // lp_gen = 2 means we want cinr
        		{ //build of primitve

        			LINK_PARAM_TYPE *param =  (LINK_PARAM_TYPE*) MEM_malloc(sizeof(LINK_PARAM_TYPE));
        			memset(param, 0, sizeof(LINK_PARAM_TYPE));
        			ParamTypeValue confirmValue;
        			confirmValue.lp_gen = value.lp_gen;
        			param->value = value;
        			param->selector = 0;
        			param->next = NULL;

        			LINK_PARAM *cinr =  (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
        			memset(cinr, 0, sizeof(LINK_PARAM));
        			cinr->lpt = param;
        			cinr->selector = 0;
        			LinkParamValue paramValue;
        			paramValue.lpv = (LINK_PARAM_VAL) ConvertDoubleToInt(apInfo.cinrMean);
        			cinr->value = paramValue;
        			cinr->next = NULL;

        			LinkParametersStatusList->elem = cinr;}

        		if (value.lp_gen==1) //for rss
        		{//build of primitive

        			LINK_PARAM_TYPE *param =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
        			memset(param, 0, sizeof(LINK_PARAM_TYPE));
        			ParamTypeValue confirmValue;
        			confirmValue.lp_gen = value.lp_gen;
        			param->value = value;
        			param->selector = 0;
        			param->next = NULL;

        			LINK_PARAM *rss =  (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
        			memset(rss, 0, sizeof(LINK_PARAM));
        			rss->lpt = param;
        			rss->selector = 0;
        			LinkParamValue paramValue;
        			paramValue.lpv = (LINK_PARAM_VAL) ConvertDoubleToInt(apInfo.rssMean);
        			rss->value = paramValue;
        			rss->next = NULL;

        			LinkParametersStatusList->elem->next = rss;}


        		NodeAddress mihfID = param_req->srcMihfID;
        		Link_Get_Parameters_confirm(node, 0, LinkParametersStatusList, NULL, NULL, param_req->srcMihfID, param_req->destMihfID, param_req->transactionId, param_req->transactionInitiated); //STATUS = 1 - unknown failure,0-success
        	}
        	MESSAGE_Free(node, msg);
        	break;
        }

        case MSG_Link_Configure_Thresholds_request:
        {
        	L_LINK_CFG_STATUS* list=(L_LINK_CFG_STATUS *)MESSAGE_ReturnInfo(msg);
        	DOT11_ApInfo apInfo;

        	L_LINK_CFG_STATUS* LinkConfigureStatusList=(L_LINK_CFG_STATUS *) MEM_malloc(sizeof(L_LINK_CFG_STATUS));
        	memset(LinkConfigureStatusList, 0, sizeof(L_LINK_CFG_STATUS));

        	/*LINK_PARAM_TYPE *param =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
     memset(param, 0, sizeof(LINK_PARAM_TYPE));
	ParamTypeValue confirmValue;
	confirmValue.lp_gen = value.lp_gen;
	param->value = value;
	param->selector = 1;
	param->next = NULL;

	LinkConfigureStatusList->length=1;
	LinkConfigureStatusList->elem->lpt=param;

	LinkConfigureStatusList->elem->threshold.t_val=0;
	LinkConfigureStatusList->elem->threshold.t_dir=0;
	LinkConfigureStatusList->elem->cs=TRUE;*/

        	LinkConfigureStatusList=list;
        	STATUS Status=0;             //success

        	Link_Configure_Thresholds_confirm(node, Status, LinkConfigureStatusList);
        	MESSAGE_Free(node, msg);
        	break;
        }

        case MSG_MAC_Link_Action_request:
        {
        	action_req *req = (action_req *)MESSAGE_ReturnInfo(msg);

        	STATUS Status=0;
			//todo: put switch condition
			if (dot11->associatedAP != NULL)
    		{
    			MacDot11ManagementDisassociate(node, dot11);
    			MacDot11sta_Link_Down_indication(node, dot11);
    		}
        	/*L_LINK_SCAN_RSP* ScanResponseSet=( L_LINK_SCAN_RSP *) MEM_malloc(sizeof( L_LINK_SCAN_RSP));
             ScanResponseSet->length=1;
ScanResponseSet->elem->la=req->PoALinkAddress;
ScanResponseSet->elem->nid=
             ScanResponseSet->elem->sigs.ss_dBm_units=//servingBs->rssMean */

        	LINK_AC_RESULT LinkActionResult=0;
        	Link_Action_confirm( node,Status,NULL,LinkActionResult);
        	MESSAGE_Free(node, msg);

        	break;
        }
        /*
        case MSG_MAC_TransmissionFinished:
        {
        	MESSAGE_Free(node, msg);
        	break;
        }
*/
		case MSG_MAC_DOT11_TIMER_SendPeriodicReport:
			{
				unsigned timerSequenceNumber = *(int*)(MESSAGE_ReturnInfo(msg));
				dot11->isTimerExpired = TRUE;

            
				DOT11_ManagementVars* mngmtVars =
					(DOT11_ManagementVars *) dot11->mngmtVars;
                 MacDot11StationGetMeasurementInfoAndSendLinkParametersReportIndication(node, dot11, mngmtVars->channelInfo->headAPList);
				 
				 MacDot11StationStartTimerOfGivenType(node, dot11, DOT11_TIMER_SEND_PERIODIC_REPORT_INTERVAL,
					 MSG_MAC_DOT11_TIMER_SendPeriodicReport);
            
            MESSAGE_Free(node, msg);
			break;
			}
		case MSG_MIH_Report_Best_PoA:
			{
							
				MihReportBestPoa *info = (MihReportBestPoa *)MESSAGE_ReturnInfo(msg);
				dot11->bestPoaId = info->bestPoaId;
#if DEBUG_MIH
				{
					char clockStr[MAX_STRING_LENGTH];
					char buff[MAX_STRING_LENGTH];
					TIME_PrintClockInSecond(getSimTime(node), clockStr);
					printf("MIH Function: Node %u received MIH message report best PoA on 802.11 MAC at %s s\n",
			   node->nodeId, clockStr);
					switch (info->macProtocol)
					{
					case MAC_PROTOCOL_CELLULAR:
						{
				
							sprintf(buff, "Cellular MAC");
							printf("best PoA ID is %u of protocol %s\n", dot11->bestPoaId, buff);
							break;
						}
					case MAC_PROTOCOL_DOT11:
						{
				
							sprintf(buff, "802.11");
							printf("best PoA ID is %u of protocol %s\n", dot11->bestPoaId, buff);
							break;
						}
					default:
						{
							printf("best PoA ID is %u of protocol %d\n", dot11->bestPoaId, info->macProtocol);
							break;
						}
					}
				}
#endif /* DEBUG */

				MESSAGE_Free(node, msg);
				break;
			}
#endif

        default: {
#ifdef ADDON_MIH
        	char tmpString[MAX_STRING_LENGTH];
        	            sprintf(tmpString,
        	                    "DOT11  node%d: Unknown message event type %d\n",
        	                    node->nodeId, msg->eventType);
        	            ERROR_ReportError(tmpString);
#else
            ERROR_ReportError("MacDot11Layer: "
                "Unknown message event type.\n");
            MESSAGE_Free(node, msg);
#endif
            break;

        }
    } //switch

    // dot11s. Some mesh messages reuse timers.
    //MESSAGE_Free(node, msg);
}//MacDot11Layer

//-------------------------------------------------------------------------
// Initalization functions
//-------------------------------------------------------------------------
//--------------------------------------------------------------------------
//  NAME:        MacDot11BssInit
//  PURPOSE:     Initialize BSS related variables.
//  PARAMETERS:  Node* node
//                  Pointer to current node
//               const NodeInput* nodeInput
//                  Pointer to node input
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               SubnetMemberData* subnetList
//                  Pointer to subnet list
//               int nodesInSubnet
//                  Number of nodes in subnet
//               int subnetListIndex
//                  Subnet list index
//               NodeAddress subnetAddress
//                  Subnet address
//               int numHostBits
//                  Number of host bits
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static //inline//
void MacDot11BssInit(
    Node* node,
    const NodeInput* nodeInput,
    MacDataDot11* dot11,
    SubnetMemberData* subnetList,
    int nodesInSubnet,
    int subnetListIndex,
    NodeAddress subnetAddress,
    int numHostBits,
    NetworkType networkType = NETWORK_IPV4,
    in6_addr* ipv6SubnetAddr = 0,
    unsigned int prefixLength = 0)
{
    char input[MAX_STRING_LENGTH];

    Address address;
    int interfaceIndex = dot11->myMacData->interfaceIndex;
    BOOL wasFound = FALSE;

    NetworkGetInterfaceInfo(node, interfaceIndex, &address, networkType);

    // dot11s. Unsupported static association.
    if (dot11->isMP)
    {
        ERROR_ReportError(
            "MacDot11BssInit: "
            "Mesh subnets do not support static association.\n");
    }
#ifdef STATIC_ASSOCIATION
//as staic association is disabled so there is no need to read AP address
//and statistically associate with it.

    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-AP",
        &wasFound,
        input);

    if (wasFound)
    {
        NodeId isNodeId = 0;

        if (networkType == NETWORK_IPV6)
        {
            anApAddress6 = address.interfaceAddr.ipv6;
            IO_ParseNodeIdOrHostAddress(
            input,
            &anApAddress6,
            &isNodeId);
        }
        else
        {
            IO_ParseNodeIdOrHostAddress(
                input,
                &anApAddress,
                (BOOL*)&isNodeId);
        }

        if (isNodeId)
        {
            // Input for ACCESS-POINT is a node ID
            // In this case, verify that access point is one of
            // the nodes in subnet by checking that the address of
            // one of its interfaces is part of subnetList.
            if (networkType == NETWORK_IPV6)
            {
                anApAddress = MAPPING_GetInterfaceAddressForSubnet(
                    node, atoi(input), ipv6SubnetAddr, prefixLength,
                    &anApAddress6, &interfaceIndex);
            }
            else
            {
                anApAddress = MAPPING_GetInterfaceAddressForSubnet(
                    node, atoi(input), subnetAddress, numHostBits);
            }

            ERROR_Assert(anApAddress != INVALID_MAPPING,
                "MacDot11BssInit: "
                "Erroneous MAC-DOT11-AP in configuration file.\n"
                "The node for the AP is not a node in the SUBNET list.\n");

            if (networkType == NETWORK_IPV6)
            {
                dot11->bssAddr = MAPPING_CreateIpv6LinkLayerAddr(isNodeId, //node->nodeId,
                                    interfaceIndex);
            }
            else
            {
                 dot11->bssAddr = anApAddress;
            }
        }
        else
        {
            // Input for ACCESS-POINT is an IP address
            // Verify that access point address is part of subnetList
            if (networkType == NETWORK_IPV6)
            {
                int i;
                int infIndex;
                in6_addr aListAddress;
                anApAddress6 = GetIPv6Address(address);
                for (i = 0; i < nodesInSubnet; i++)
                {
                    MAPPING_GetInterfaceAddressForSubnet(
                        node,
                        subnetList[i].nodeId,
                        ipv6SubnetAddr,
                        prefixLength,
                        &aListAddress,
                        &infIndex);

                    if (SAME_ADDR6(aListAddress, anApAddress6))
                    {
                        break;
                    }
                }
                ERROR_Assert(SAME_ADDR6(aListAddress, anApAddress6),
                "MacDot11BssInit: "
                "Erroneous MAC-DOT11-AP in configuration file.\n"
                "The address for the AP in not part of the SUBNET list.\n");

                dot11->bssAddr = MAPPING_CreateIpv6LinkLayerAddr(node->nodeId,
                                interfaceIndex);
            }
            else
            {
                int i;
                NodeAddress aListAddress = 0;
                for (i = 0; i < nodesInSubnet; i++)
                {
                    aListAddress = MAPPING_GetInterfaceAddressForSubnet(
                        node, subnetList[i].node->nodeId,
                        subnetAddress, numHostBits);
                    if (aListAddress == anApAddress)
                    {
                        break;
                    }
                }
                aListAddress = anApAddress;

                ERROR_Assert(anApAddress == aListAddress,
                    "MacDot11BssInit: "
                    "Erroneous MAC-DOT11-AP in configuration file.\n"
                    "The address for the AP in not part of the SUBNET list.\n");

                dot11->bssAddr = anApAddress;
            }
        }

        if (dot11->selfAddr == dot11->bssAddr)
        {
            dot11->stationType = DOT11_STA_AP;

            // Init access point variables
            if (networkType == NETWORK_IPV6)
            {
                MacDot11ApInit(node, nodeInput, dot11,
                    subnetList, nodesInSubnet, subnetListIndex,
                    0, 0, networkType, ipv6SubnetAddr, prefixLength);
            }
            else
            {
                MacDot11ApInit(node, nodeInput, dot11,
                    subnetList, nodesInSubnet, subnetListIndex,
                    subnetAddress, numHostBits, networkType);
            }
        }
        else
        {
            dot11->stationType = DOT11_STA_BSS;
        }
    }
//---------------------------Power-Save-Mode-Updates---------------------//
    else {
#endif
        int beaconInterval;
        int beaconStart;
        int atimDuration;
        Queue*  queue = NULL;

        IO_ReadString(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11-IBSS-SUPPORT-PS-MODE",
            &wasFound,
            input);
        if (wasFound){
            if (strcmp(input,"YES") == 0){

                dot11->isPSModeEnabledInAdHocMode = TRUE;
                beaconInterval = DOT11_BEACON_INTERVAL_DEFAULT;

                if (networkType == NETWORK_IPV6){
                    IO_ReadInt(
                        node->nodeId,
                        ipv6SubnetAddr,
                        nodeInput,
                        "MAC-DOT11-IBSS-BEACON-INTERVAL",
                        &wasFound,
                        &beaconInterval);
                }
                else{
                    IO_ReadInt(
                        node->nodeId,
                        subnetAddress,
                        nodeInput,
                        "MAC-DOT11-IBSS-BEACON-INTERVAL",
                        &wasFound,
                        &beaconInterval);
                }
                    if (wasFound) {
                    ERROR_Assert(beaconInterval >= 0
                        && beaconInterval <= DOT11_BEACON_INTERVAL_MAX,
                        "MacDot11BeaconInit: "
                        "Out of range value for MAC-DOT11-BEACON-INTERVAL in "
                        "configuration file.\n"
                        "Valid range is 0 to 32767.\n");
                }


                dot11->beaconInterval =
                    MacDot11TUsToClocktype((unsigned short) beaconInterval);

                beaconStart = DOT11_BEACON_START_DEFAULT;
                if (networkType == NETWORK_IPV6){
                    IO_ReadInt(
                        node->nodeId,
                        ipv6SubnetAddr,
                        nodeInput,
                        "MAC-DOT11-IBSS-BEACON-START-TIME",
                        &wasFound,
                        &beaconStart);
                }
                else{
                    IO_ReadInt(
                        node->nodeId,
                        subnetAddress,
                        nodeInput,
                        "MAC-DOT11-IBSS-BEACON-START-TIME",
                        &wasFound,
                        &beaconStart);
                }
                if (wasFound) {
                    ERROR_Assert(beaconStart >= 0
                        && beaconStart
                            <= beaconInterval,
                        "MacDot11BeaconStartInit: "
                        "Out of range value for MAC-DOT11-BEACON-START-TIME "
                        "in configuration file.\n"
                        "Valid range is 0 to beacon interval.\n");
                }

                dot11->beaconTime =
                    MacDot11TUsToClocktype((unsigned short)beaconStart);

                atimDuration = DOT11_PS_IBSS_ATIM_DURATION;
                if (networkType == NETWORK_IPV6){
                    IO_ReadInt(
                        node->nodeId,
                        ipv6SubnetAddr,
                        nodeInput,
                        "MAC-DOT11-IBSS-PS-MODE-ATIM-DURATION",
                        &wasFound,
                        &atimDuration);
                }
                else{
                    IO_ReadInt(
                        node->nodeId,
                        subnetAddress,
                        nodeInput,
                        "MAC-DOT11-IBSS-PS-MODE-ATIM-DURATION",
                        &wasFound,
                        &atimDuration);
                }
                if (wasFound) {
                    ERROR_Assert(atimDuration >= 0
                        && atimDuration
                            <= beaconInterval,
                        "MacDot11ATIMIntervalInit: "
                        "Out of range value for MAC-DOT11-ATIM-INTERVAL "
                        "in configuration file.\n"
                        "Valid range is 0 to beacon interval.\n");
                }

                dot11->atimDuration = (unsigned short)atimDuration;
                // If Atim duration is zero
                // then IBSS should not support PS mode
                if (dot11->atimDuration == 0){
                    dot11->isPSModeEnabledInAdHocMode = FALSE;
                    Int8 buf[MAX_STRING_LENGTH];
                    sprintf(buf, "Node Id: %d, STA disabled the PS Mode"
                        " because defined ATIM duration is zero",
                        node->nodeId);
                    ERROR_ReportWarning(buf);
                }
            }
            else if (strcmp(input,"NO") != 0){
                ERROR_ReportError("MacDot11BssInit: "
                    "Invalid value for MAC-DOT11-IBSS-SUPPORT-PS-MODE "
                    "in configuration file.\n"
                    "Expecting YES or NO.\n");
            }
        }

        if (dot11->isPSModeEnabledInAdHocMode == TRUE){
            // Init station management
            MacDot11ManagementInit(node, nodeInput, dot11, networkType);

            // Start beacon timer
            MacDot11StationStartTimerOfGivenType(
                node,
                dot11,
                dot11->beaconTime,
                MSG_MAC_DOT11_Beacon);
            dot11->beaconSequenceNumber = dot11->timerSequenceNumber;

            // Set up the broadcast queue here
            queue = new Queue; // "FIFO"
            ERROR_Assert(queue != NULL,\
                "AP: Unable to allocate memory for station queue");
            queue->SetupQueue(node, "FIFO", dot11->broadcastQueueSize, 0, 0,
                0, FALSE);
            dot11->broadcastQueue = queue;
        }
#ifdef Enable_Static_Association

    }// end of if-else if AP not found
#endif
//---------------------------Power-Save-Mode-End-Updates-----------------//
}

//--------------------------------------------------------------------------
//  NAME:        MacDot11BssDynamicInit
//  PURPOSE:     Initialize a dynamic BSS related variables.
//  PARAMETERS:  Node* node
//                  Pointer to current node
//               const NodeInput* nodeInput
//                  Pointer to node input
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               SubnetMemberData* subnetList
//                  Pointer to subnet list
//               int nodesInSubnet
//                  Number of nodes in subnet
//               int subnetListIndex
//                  Subnet list index
//               NodeAddress subnetAddress
//                  Subnet address
//               int numHostBits
//                  Number of host bits
//  RETURN:      None
//  ASSUMPTION:  None
//--------------------------------------------------------------------------
static //inline//
void MacDot11BssDynamicInit(
    Node* node,
    const NodeInput* nodeInput,
    MacDataDot11* dot11,
    SubnetMemberData* subnetList,
    int nodesInSubnet,
    int subnetListIndex,
    NodeAddress subnetAddress,
    int numHostBits,
    NetworkType networkType = NETWORK_IPV4,
    in6_addr* ipv6SubnetAddr = 0,
    unsigned int prefixLength = 0)
{
    char retString[MAX_STRING_LENGTH];
    BOOL wasFound = FALSE;

    // For an infrastructure network, the access point is given by
    // MAC-DOT11-AP e.g.
    // [25] MAC-DOT11-AP YES
    // would indicate that node 25 was the access point.
    //
    Address address;
    NetworkGetInterfaceInfo(
                node,
                dot11->myMacData->interfaceIndex,
                &address,
                networkType);

    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-AP",
        &wasFound,
        retString);

    // dot11s. MP (non-MAPs) behave as APs for beaconing etc.
    if (dot11->isMP)
    {
        if ((wasFound) && (strcmp(retString, "YES") == 0))
        {
            dot11->isMAP = TRUE;
        }
        else
        {
            dot11->isMAP = FALSE;
            wasFound = TRUE;
            strcpy(retString , "YES");
        }
    }

    if ((wasFound) && (strcmp(retString, "YES") == 0))
    {
        dot11->stationType = DOT11_STA_AP;
        // Init access point variables
        if (networkType == NETWORK_IPV6)
        {
            MacDot11ApInit(node, nodeInput, dot11,
                subnetList, nodesInSubnet, subnetListIndex,
                0, 0, networkType, ipv6SubnetAddr, prefixLength);
        }
        else
        {
            MacDot11ApInit(node, nodeInput, dot11,
                subnetList, nodesInSubnet, subnetListIndex,
                subnetAddress, numHostBits, networkType);
        }
        // Set inital state
        MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
    }
    else
    {
        dot11->stationType = DOT11_STA_BSS;
//---------------------------Power-Save-Mode-Updates---------------------//
        IO_ReadString(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11-STA-PS-MODE-ENABLED",
            &wasFound,
            retString);

        if (wasFound){
            int listenInterval;
            if (strcmp(retString, "YES") == 0){
                dot11->isPSModeEnabled = TRUE;
            }
            else if (strcmp(retString, "NO") != 0){
                ERROR_ReportError("MacDot11BssDynamicInit: "
                    "Invalid value for MAC-DOT11-STA-PS-MODE-ENABLED "
                    "in configuration file.\n"
                    "Expecting YES or NO.\n");
            }
            IO_ReadInt(
                node->nodeId,
                &address,
                nodeInput,
                "MAC-DOT11-STA-PS-MODE-LISTEN-INTERVAL",
                &wasFound,
                &listenInterval);
            if (wasFound){
                dot11->listenIntervals = listenInterval;
                ERROR_Assert(dot11->listenIntervals >= 0
                    && dot11->listenIntervals <= 32767,
                    "MacDot11BssDynamicInit: "
                    "Value of MAC-DOT11-STA-PS-MODE-LISTEN-INTERVAL "
                        "is negative.or greater then 32767\n");
            }

            IO_ReadString(
                node->nodeId,
                &address,
                nodeInput,
                "MAC-DOT11-STA-PS-MODE-LISTEN-DTIM-FRAME",
                &wasFound,
                retString);
            if (wasFound){
                if (strcmp(retString, "NO") == 0){
                    dot11->isReceiveDTIMFrame = FALSE;
                }
                else if (strcmp(retString, "YES") != 0){
                    ERROR_ReportError("MacDot11BssDynamicInit: Invalid value"
                        " for MAC-DOT11-STA-PS-MODE-LISTEN-DTIM-FRAME "
                        "in configuration file.\n"
                        "Expecting YES or NO.\n");
                }
            }
        }
//---------------------------Power-Save-Mode-End-Updates-----------------//

        // Set inital state
        //Set IDLE in place of WFJOIN so that DCF state machine
        //works correctly
        //MacDot11StationSetState(node, dot11, DOT11_S_WFJOIN);
        MacDot11StationSetState(node, dot11, DOT11_S_IDLE);
    }

    // Init and start beacon.
    MacDot11BeaconInit(node, nodeInput, dot11, networkType);

    // Init MIBs
    MacDot11MibInit(node, nodeInput, dot11, networkType);

    // Init station management
    MacDot11ManagementInit(node, nodeInput, dot11, networkType);

}// MacDot11BssDynamicInit


//---------------------DOT11e--Updates------------------------------------//

// /**
// FUNCTION   :: MacDot11eACInit
// LAYER      :: MAC
// PURPOSE    :: Initialize Access categories of Dot11e with default values
// PARAMETERS ::
// + node      : Node* :Node being Initialized
// + dot11     : MacDataDot11* : Structure of dot11.
// + phyModel  : PhyModel : PHY model used.
// RETURN     :: void : NULL
// **/
static void MacDot11eACInit(Node* node, MacDataDot11* dot11, PhyModel phyModel)
{
    // EDCA default values for Contention window and
    // parameters of Access categories.

    if (phyModel == PHY802_11a){

        // Access Category 0
        dot11->ACs[DOT11e_AC_BK].cwMin = DOT11_802_11a_CW_MIN;
        dot11->ACs[DOT11e_AC_BK].cwMax = DOT11_802_11a_CW_MAX;
        dot11->ACs[DOT11e_AC_BK].slotTime = DOT11_802_11a_SLOT_TIME;

        // Arbitary Inter space number
        dot11->ACs[DOT11e_AC_BK].AIFSN = DOT11e_802_11a_AC_BK_AIFSN;

        // Transmission opportunity limit
        dot11->ACs[DOT11e_AC_BK].TXOPLimit = DOT11e_802_11a_AC_BK_TXOPLimit;

        dot11->ACs[DOT11e_AC_BK].AIFS = dot11->sifs +
                   dot11->ACs[DOT11e_AC_BK].AIFSN * dot11->ACs[0].slotTime;

        // Access Category 1
        dot11->ACs[DOT11e_AC_BE].cwMin = DOT11_802_11a_CW_MIN;
        dot11->ACs[DOT11e_AC_BE].cwMax = DOT11_802_11a_CW_MAX;
        dot11->ACs[DOT11e_AC_BE].slotTime = DOT11_802_11a_SLOT_TIME;

        // Arbitary Inter space number
        dot11->ACs[DOT11e_AC_BE].AIFSN = DOT11e_802_11a_AC_BE_AIFSN;

        // Transmission opportunity limit
        dot11->ACs[DOT11e_AC_BE].TXOPLimit = DOT11e_802_11a_AC_BE_TXOPLimit;
        dot11->ACs[DOT11e_AC_BE].AIFS = dot11->sifs +
                    dot11->ACs[DOT11e_AC_BE].AIFSN * dot11->slotTime;

        // Access Category 2
        dot11->ACs[DOT11e_AC_VI].cwMin = ((DOT11_802_11a_CW_MIN + 1) / 2) - 1;
        dot11->ACs[DOT11e_AC_VI].cwMax = DOT11_802_11a_CW_MIN;
        dot11->ACs[DOT11e_AC_VI].slotTime = DOT11_802_11a_SLOT_TIME;

        // Arbitary Inter space number
        dot11->ACs[DOT11e_AC_VI].AIFSN = DOT11e_802_11a_AC_VI_AIFSN;;

        // Transmission opportunity limit
        dot11->ACs[DOT11e_AC_VI].TXOPLimit = DOT11e_802_11a_AC_VI_TXOPLimit;
        dot11->ACs[DOT11e_AC_VI].AIFS = dot11->sifs +
                    dot11->ACs[DOT11e_AC_VI].AIFSN * dot11->ACs[2].slotTime;

        // Access Category 3
        dot11->ACs[DOT11e_AC_VO].cwMin = ((DOT11_802_11a_CW_MIN + 1) / 4) - 1;
        dot11->ACs[DOT11e_AC_VO].cwMax = ((DOT11_802_11a_CW_MIN + 1) / 2) - 1;
        dot11->ACs[DOT11e_AC_VO].slotTime = DOT11_802_11a_SLOT_TIME;

        // Arbitary Inter space number
        dot11->ACs[DOT11e_AC_VO].AIFSN = DOT11e_802_11a_AC_VO_AIFSN;

        // Transmission opportunity limit
        dot11->ACs[DOT11e_AC_VO].TXOPLimit = DOT11e_802_11a_AC_VO_TXOPLimit;
        dot11->ACs[DOT11e_AC_VO].AIFS = dot11->sifs +
        dot11->ACs[DOT11e_AC_VO].AIFSN * dot11->ACs[3].slotTime;
   } else if (phyModel == PHY802_11b || phyModel == PHY_ABSTRACT) {

       // Access Category 0
       dot11->ACs[DOT11e_AC_BK].cwMin = DOT11_802_11b_CW_MIN;
       dot11->ACs[DOT11e_AC_BK].cwMax = DOT11_802_11b_CW_MAX;
       dot11->ACs[DOT11e_AC_BK].slotTime = DOT11_802_11b_SLOT_TIME;

       // Arbitary Inter space number
       dot11->ACs[DOT11e_AC_BK].AIFSN = DOT11e_802_11b_AC_BK_AIFSN ;

       // Transmission opportunity limit
       dot11->ACs[DOT11e_AC_BK].TXOPLimit = DOT11e_802_11a_AC_BK_TXOPLimit;
       dot11->ACs[DOT11e_AC_BK].AIFS = dot11->sifs +
                  dot11->ACs[DOT11e_AC_BK].AIFSN * dot11->ACs[0].slotTime;

       // Access Category 1
       dot11->ACs[DOT11e_AC_BE].cwMin = DOT11_802_11b_CW_MIN;
       dot11->ACs[DOT11e_AC_BE].cwMax = DOT11_802_11b_CW_MAX;
       dot11->ACs[DOT11e_AC_BE].slotTime = DOT11_802_11b_SLOT_TIME;

       // Arbitary Inter space number
       dot11->ACs[DOT11e_AC_BE].AIFSN = DOT11e_802_11b_AC_BE_AIFSN;

       // Transmission opportunity limit
       dot11->ACs[DOT11e_AC_BE].TXOPLimit = DOT11e_802_11b_AC_BE_TXOPLimit;
       dot11->ACs[DOT11e_AC_BE].AIFS = dot11->sifs +
                   dot11->ACs[DOT11e_AC_BE].AIFSN * dot11->slotTime;

       // Access Category 2
       dot11->ACs[DOT11e_AC_VI].cwMin = ((DOT11_802_11b_CW_MIN + 1) / 2) - 1;
       dot11->ACs[DOT11e_AC_VI].cwMax = DOT11_802_11b_CW_MIN;
       dot11->ACs[DOT11e_AC_VI].slotTime = DOT11_802_11b_SLOT_TIME;

       // Arbitary Inter space number
       dot11->ACs[DOT11e_AC_VI].AIFSN = DOT11e_802_11b_AC_VI_AIFSN;

       // Transmission opportunity limit
       dot11->ACs[DOT11e_AC_VI].TXOPLimit = DOT11e_802_11b_AC_VI_TXOPLimit;
       dot11->ACs[DOT11e_AC_VI].AIFS = dot11->sifs +
                   dot11->ACs[DOT11e_AC_VI].AIFSN * dot11->ACs[2].slotTime;

       // Access Category 3
       dot11->ACs[DOT11e_AC_VO].cwMin = ((DOT11_802_11b_CW_MIN + 1) / 4) - 1;
       dot11->ACs[DOT11e_AC_VO].cwMax = ((DOT11_802_11b_CW_MIN + 1) / 2) - 1;
       dot11->ACs[DOT11e_AC_VO].slotTime = DOT11_802_11b_SLOT_TIME;

       // Arbitary Inter space number
       dot11->ACs[DOT11e_AC_VO].AIFSN = DOT11e_802_11b_AC_VO_AIFSN;

       // Transmission opportunity limit
       dot11->ACs[DOT11e_AC_VO].TXOPLimit = DOT11e_802_11b_AC_VO_TXOPLimit;
       dot11->ACs[DOT11e_AC_VO].AIFS = dot11->sifs +
       dot11->ACs[DOT11e_AC_VO].AIFSN * dot11->ACs[3].slotTime;
    }
    for (int acCounter = 0; acCounter < DOT11e_NUMBER_OF_AC; acCounter++){
        dot11->ACs[acCounter].BO = 0;
        dot11->ACs[acCounter].lastBOTimeStamp = 0;
        dot11->ACs[acCounter].frameToSend = NULL;
        dot11->ACs[acCounter].currentNextHopAddress = INVALID_802ADDRESS;
        dot11->ACs[acCounter].totalNoOfthisTypeFrameQueued = 0;
        dot11->ACs[acCounter].totalNoOfthisTypeFrameDeQueued = 0;
    }
} // end of AC init

//--------------------DOT11e-End-Updates---------------------------------//

//--------------------------------------------------------------------------
// NAME:       MacDot11Init
// PURPOSE:    Initialize Dot11, read user input.
// PARAMETERS  Node* node
//                Node being initialized.
//             NodeInput* nodeInput
//                Structure containing contents of input file.
//             SubnetMemberData* subnetList
//                Number of nodes in subnet.
//             int nodesInSubnet
//                Number of nodes in subnet.
//             int subnetListIndex
//             NodeAddress subnetAddress
//                Subnet address.
//             int numHostBits
//                number of host bits.
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
void MacDot11Init(
    Node* node,
    int interfaceIndex,
    const NodeInput* nodeInput,
    PhyModel phyModel,
    SubnetMemberData* subnetList,
    int nodesInSubnet,
    int subnetListIndex,
    NodeAddress subnetAddress,
    int numHostBits,
    BOOL isQosEnabled,
    NetworkType networkType,
    in6_addr *ipv6SubnetAddr,
    unsigned int prefixLength)
{
    BOOL wasFound;
    char retString[MAX_STRING_LENGTH];
    int anIntInput;
    Address address;
    clocktype maxExtraPropDelay;
    MacDataDot11* dot11 =
        (MacDataDot11*)MEM_malloc(sizeof(MacDataDot11));

    MacDot11StationCheckHeaderSizes();

    memset(dot11, 0, sizeof(MacDataDot11));

#if  0
    StatsDb* db = node->partitionData->statsDb;
    // If MAC aggregate table is enabled, we need to allocate memory for
    // the aggregate stats
    if (db->statsAggregateTable->createMacAggregateTable)
    {
        dot11->aggregateStats =
            (StatsDBMacAggregate *) MEM_malloc(sizeof(StatsDBMacAggregate));
        memset(dot11->aggregateStats,
            0, sizeof(StatsDBMacAggregate));
    }
#endif

    dot11->myMacData = node->macData[interfaceIndex];
    dot11->myMacData->macVar = (void*)dot11;

    RANDOM_SetSeed(dot11->seed,
                   node->globalSeed,
                   node->nodeId,
                   (UInt32)MAC_PROTOCOL_DOT11,
                   (UInt32)interfaceIndex);

    if (phyModel == PHY802_11a) {
        dot11->cwMin = DOT11_802_11a_CW_MIN;
        dot11->cwMax = DOT11_802_11a_CW_MAX;
        dot11->slotTime = DOT11_802_11a_SLOT_TIME;
        dot11->sifs = DOT11_802_11a_SIFS;
        dot11->delayUntilSignalAirborn =
            DOT11_802_11a_DELAY_UNTIL_SIGNAL_AIRBORN;
    }
    else if (phyModel == PHY802_11b || phyModel == PHY_ABSTRACT)
    {
        dot11->cwMin = DOT11_802_11b_CW_MIN;
        dot11->cwMax = DOT11_802_11b_CW_MAX;
        dot11->slotTime = DOT11_802_11b_SLOT_TIME;
        dot11->sifs = DOT11_802_11b_SIFS;
        dot11->delayUntilSignalAirborn =
            DOT11_802_11b_DELAY_UNTIL_SIGNAL_AIRBORN;
    }
     dot11->propagationDelay = DOT11_PROPAGATION_DELAY;

    dot11->txSifs = dot11->sifs - dot11->delayUntilSignalAirborn;
    dot11->difs = dot11->sifs + (2 * dot11->slotTime);
    dot11->txDifs = dot11->txSifs + (2 * dot11->slotTime);
    //---------------------DOT11e--Updates------------------------------------//
    if (isQosEnabled)
    {
        dot11->isQosEnabled = TRUE;
        dot11->associatedWithQAP = FALSE;

        // Call ACs initialization function
        MacDot11eACInit(node, dot11, phyModel);
        dot11->currentACIndex = DOT11e_INVALID_AC;

        // Initailize other elements
        // Element Id is vendor specific ids setting to 1
        dot11->qBSSLoadElement.stationCount = 0;
        dot11->qBSSLoadElement.channelUtilization = 0;

        // Admission Control is currently ommited feature.
        dot11->qBSSLoadElement.availableAdmissionControl = 0;
        memset(&(dot11->edcaParamSet.qosInfo), 0,
                sizeof(DOT11e_QosInfoField)),

        dot11->edcaParamSet.reserved = 0;

        for (int acCounter = DOT11e_AC_BK;
                 acCounter < DOT11e_NUMBER_OF_AC;
                 acCounter++)
        {
             dot11->edcaParamSet.ACs[acCounter]. ACI =
             (unsigned char)(acCounter);
             dot11->edcaParamSet.ACs[acCounter].ECWmin =
             (unsigned char)(dot11->ACs[acCounter].cwMin);
             dot11->edcaParamSet.ACs[acCounter].ECWmax =
             (unsigned char)(dot11->ACs[acCounter].cwMax);
         }

         // Initialize Stats
         dot11->totalNoOfQoSDataFrameSend = 0;
         dot11->totalNoOfnQoSDataFrameSend = 0;
         dot11->totalNoOfQoSDataFrameReceived = 0;
         dot11->totalNoOfnQoSDataFrameReceived = 0;
//--------------------HCCA-Updates Start---------------------------------//
         //For QAP
         dot11->totalQoSNullDataReceived = 0;
         dot11->totalNoOfQoSDataCFPollsTransmitted = 0;
         dot11->totalNoOfQoSCFPollsTransmitted = 0;
         dot11->totalNoOfQoSCFAckTransmitted = 0;
         dot11->totalNoOfQoSCFDataFramesReceived = 0;
         //For QSTA
         dot11->totalQoSNullDataTransmitted = 0;
         dot11->totalNoOfQoSCFDataFrameSend=0;
         dot11->totalNoOfQoSDataCFPollsReceived = 0;
         dot11->totalNoOfQoSCFPollsReceived = 0;
         dot11->totalNoOfQoSCFAckReceived = 0;

//--------------------HCCA-Updates End---------------------------------//
    } //--------------------DOT11e-End-Updates-----------------------------//
    else
    {
        dot11->isQosEnabled = FALSE;

        dot11->ACs[DOT11e_AC_BK].cwMin = dot11->cwMin;
        dot11->ACs[DOT11e_AC_BK].cwMax = dot11->cwMax;
        dot11->ACs[DOT11e_AC_BK].slotTime = dot11->slotTime;
        dot11->ACs[DOT11e_AC_BK].AIFS = dot11->difs;
        dot11->ACs[DOT11e_AC_BK].CW = dot11->ACs[DOT11e_AC_BK].cwMin;
        dot11->ACs[DOT11e_AC_BK].BO = 0;
    }
    dot11->extendedIfsDelay =
        dot11->sifs +
        PHY_GetTransmissionDuration(
            node, dot11->myMacData->phyNumber,
            MAC_PHY_LOWEST_MANDATORY_RATE_TYPE,
            DOT11_SHORT_CTRL_FRAME_SIZE) +
        dot11->difs;

    dot11->state = DOT11_S_IDLE;
    dot11->prevState = DOT11_S_IDLE;
    dot11->IsInExtendedIfsMode = FALSE;

    dot11->noResponseTimeoutDuration = 0;
    dot11->CW = dot11->cwMin;
    dot11->BO = 0;
    MacDot11StationResetAckVariables(dot11);
    dot11->NAV = 0;
    dot11->noOutgoingPacketsUntilTime = 0;

    dot11->seqNoHead = NULL;
    dot11->dataRateHead = NULL;

    dot11->timerSequenceNumber = 0;

    dot11->extraPropDelay =
        dot11->myMacData->propDelay - dot11->propagationDelay;
#ifdef CYBER_LIB
    // Add the PHY layer turnaround delay which should be counted but is
    // ignored (however, this is important in assessing wormhole victims;
    // In particular, if MAC authentication is implemented against
    // external adversaries, then crypto-delay must be included in the
    // turnaround delay)
    NetworkDataIp *ip = node->networkData.networkVar;
    IpInterfaceInfoType* intf =
        (IpInterfaceInfoType*)ip->interfaceInfo[interfaceIndex];
    if (intf->countWormholeVictimTurnaroundTime)
    {
        clocktype turnaroundtime = 0;
        if (intf->wormholeVictimTurnaroundTime != 0)
        {
            turnaroundtime = intf->wormholeVictimTurnaroundTime;
        }
        else
        {
            int phyIndex = dot11->myMacData->phyNumber;
            PhyData *thisPhy = node->phyData[phyIndex];
            switch(thisPhy->phyModel)
            {
#ifdef WIRELESS_LIB
                case PHY802_11b:
                case PHY802_11a: {
                    PhyData802_11* phy802_11 = (PhyData802_11*)(thisPhy->phyVar);
                    turnaroundtime = phy802_11->rxTxTurnaroundTime;
                    break;
                }
                case PHY_ABSTRACT: {
                    turnaroundtime = PHY_ABSTRACT_RX_TX_TURNAROUND_TIME;
                    break;
                }
#endif // WIRELESS_LIB
                default: {
                    ERROR_ReportError("Unsupported or disabled PHY model");
                }
            }
        }
        dot11->extraPropDelay += turnaroundtime;
    }
#endif // CYBER_LIB

    dot11->txFrameInfo = NULL;
    MacDot11StationResetCurrentMessageVariables(node, dot11);

    PHY_GetLowestTxDataRateType(
        node, dot11->myMacData->phyNumber, &(dot11->lowestDataRateType));
    PHY_GetHighestTxDataRateType(
        node, dot11->myMacData->phyNumber, &(dot11->highestDataRateType));
    PHY_GetHighestTxDataRateTypeForBC(
        node,
        dot11->myMacData->phyNumber,
        &(dot11->highestDataRateTypeForBC));
    dot11->rateAdjustmentTime = DOT11_DATA_RATE_ADJUSTMENT_TIME;

    dot11->stationType = DOT11_STATION_TYPE_DEFAULT;

    dot11->bssAddr = INVALID_802ADDRESS;
    dot11->selfAddr = INVALID_802ADDRESS;

    dot11->BeaconAttempted = FALSE;
    dot11->beaconIsDue = FALSE;
    dot11->beaconInterval = 0;
    dot11->beaconsMissed = 0;
    dot11->stationCheckTimerStarted = FALSE;

    dot11->pktsToSend = 0;

    dot11->unicastPacketsSentDcf = 0;
    dot11->broadcastPacketsSentDcf = 0;
    dot11->broadcastPacketsSentCfp = 0;

    dot11->unicastPacketsGotDcf = 0;
    dot11->broadcastPacketsGotDcf = 0;

    dot11->ctsPacketsSentDcf = 0;
    dot11->rtsPacketsSentDcf = 0;
    dot11->ackPacketsSentDcf = 0;

    dot11->retxDueToCtsDcf = 0;
    dot11->retxDueToAckDcf = 0;

    dot11->retxAbortedRtsDcf = 0;
    dot11->pktsDroppedDcf = 0;

    dot11->potentialIncomingMessage = NULL;
    dot11->directionalInfo = NULL;

    // Management vars
    dot11->associationType = DOT11_ASSOCIATION_NONE;
    dot11->networkType = DOT11_NET_UNKNOWN;
    dot11->stationAuthenticated = FALSE;
    dot11->stationAssociated = FALSE;

    maxExtraPropDelay = (DOT11_MAX_DURATION -
            (dot11->sifs +
                PHY_GetTransmissionDuration(
                    node, dot11->myMacData->phyNumber,
                    dot11->lowestDataRateType,
                    DOT11_SHORT_CTRL_FRAME_SIZE) + dot11->sifs +
                PHY_GetTransmissionDuration(
                    node, dot11->myMacData->phyNumber,
                    dot11->lowestDataRateType,
                    DOT11_FRAG_THRESH) +
                dot11->sifs + PHY_GetTransmissionDuration(
                    node, dot11->myMacData->phyNumber,
                    dot11->lowestDataRateType,
                    DOT11_SHORT_CTRL_FRAME_SIZE)))/4;
    if (dot11->extraPropDelay > maxExtraPropDelay)
    {
        ERROR_ReportWarning("Value of MAC-PROPAGATION-DELAY is too big "
                            "for 802.11 MAC. It may be truncated.\n");
        dot11->extraPropDelay = maxExtraPropDelay ;
    }

    NetworkGetInterfaceInfo(
        node,
        dot11->myMacData->interfaceIndex,
        &address,
        networkType);

    dot11->QosFrameSent = FALSE;

//---------------------------Power-Save-Mode-Updates---------------------//
    // Statistics data in PS mode
    dot11->atimFramesReceived = 0;
    dot11->atimFramesSent = 0;
    dot11->atimFramesACKReceived = 0;

    dot11->psPollPacketsSent = 0;
    dot11->psModeDTIMFrameReceived = 0;
    dot11->psModeTIMFrameReceived = 0;

    dot11->psModeDTIMFrameSent = 0;
    dot11->psModeTIMFrameSent = 0;
    dot11->psPollRequestsReceived = 0;
    dot11->unicastDataPacketSentToPSModeSTAs = 0;
    dot11->broadcastDataPacketSentToPSModeSTAs = 0;

    dot11->assignAssociationId = 0;
    dot11->beaconExpirationTime = 0;
    dot11->awakeTimerSequenceNo = 0;
    dot11->stationHasReceivedBeconInAdhocMode = FALSE;
    dot11->attachNodeList = NULL;
    dot11->receiveNodeList = NULL;
    // create the initialize the broadcast queue here
    dot11->noOfBufferedBroadcastPacket = 0;
    dot11->broadcastQueue = NULL;
    dot11->joinedAPSupportPSMode = TRUE;
    dot11->isPSModeEnabled = FALSE;
    dot11->listenIntervals = DOT11_PS_LISTEN_INTERVAL;
    dot11->isReceiveDTIMFrame = TRUE;
    dot11->isIBSSATIMDurationActive = FALSE;
    dot11->isMoreDataPresent = FALSE;
    dot11->nextAwakeTime = 0;
    dot11->phyStatus = MAC_DOT11_PS_ACTIVE;
    dot11->psModeMacQueueDropPacket = 0;
    dot11->tempQueue = NULL;
    dot11->tempAttachNodeList = NULL;
    dot11->broadcastQueueSize = DOT11_PS_MODE_DEFAULT_BROADCAST_QUEUE_SIZE;
    dot11->unicastQueueSize = DOT11_PS_MODE_DEFAULT_UNICAST_QUEUE_SIZE;
//---------------------------Power-Save-Mode-End-Updates-----------------//
    // Read short retry count.
    // Format is :
    // MAC-DOT11-SHORT-PACKET-TRANSMIT-LIMIT <value>
    // Default is DOT11_SHORT_RETRY_LIMIT (value 7)

    IO_ReadInt(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-SHORT-PACKET-TRANSMIT-LIMIT",
        &wasFound,
        &anIntInput);

    if (wasFound) {
        dot11->shortRetryLimit = anIntInput;
        ERROR_Assert(dot11->shortRetryLimit > 0,
            "MacDot11Init: "
            "Value of MAC-DOT11-SHORT-PACKET-TRANSMIT-LIMIT "
            "should be greater than zero.\n");
    }
    else {
        dot11->shortRetryLimit = DOT11_SHORT_RETRY_LIMIT;
    }

    dot11->directionalShortRetryLimit = dot11->shortRetryLimit + 1;

    // Read long retry count.
    // Format is :
    // MAC-DOT11-LONG-PACKET-TRANSMIT-LIMIT <value>
    // Default is DOT11_LONG_RETRY_LIMIT (value 4)

    IO_ReadInt(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-LONG-PACKET-TRANSMIT-LIMIT",
        &wasFound,
        &anIntInput);

    if (wasFound)
    {
        dot11->longRetryLimit = anIntInput;
        ERROR_Assert(dot11->longRetryLimit > 0,
            "MacDot11Init: "
            "Value of MAC-DOT11-LONG-PACKET-TRANSMIT-LIMIT "
            "should be greater than zero.\n");
    }
    else
    {
        dot11->longRetryLimit = DOT11_LONG_RETRY_LIMIT;
    }

    // Read RTS threshold.
    // Format is :
    // MAC-DOT11-RTS-THRESHOLD <value>
    // Default is DOT11_RTS_THRESH (value 0, RTS/CTS for all packets)

    IO_ReadInt(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-RTS-THRESHOLD",
        &wasFound,
        &anIntInput);

    if (wasFound)
    {
        dot11->rtsThreshold = anIntInput;
        ERROR_Assert(dot11->rtsThreshold >= 0,
            "MacDot11Init: "
            "Value of MAC-DOT11-RTS-THRESHOLD is negative.\n");
    }
    else
    {
        dot11->rtsThreshold = DOT11_RTS_THRESH;
    }

    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-STOP-RECEIVING-AFTER-HEADER-MODE",
        &wasFound,
        retString);

    if ((!wasFound) || (strcmp(retString, "NO") == 0))
    {
        dot11->stopReceivingAfterHeaderMode = FALSE;
    }
    else if (strcmp(retString, "YES") == 0)
    {
        dot11->stopReceivingAfterHeaderMode = TRUE;
    }

    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-DIRECTIONAL-ANTENNA-MODE",
        &wasFound,
        retString);

    if ((!wasFound) || (strcmp(retString, "NO") == 0))
    {
        dot11->useDvcs = FALSE;
    }
    else if (strcmp(retString, "YES") == 0)
    {
        clocktype directionCacheExpirationDelay;
        double defaultNavedDeltaAngle;

        dot11->useDvcs = TRUE;
        IO_ReadTime(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11-DIRECTION-CACHE-EXPIRATION-TIME",
            &wasFound,
            &directionCacheExpirationDelay);

        if (!wasFound)
        {
            ERROR_ReportError(
                "Expecting "
                "MAC-DOT11-DIRECTION-CACHE-EXPIRATION-TIME "
                "parameter\n");
        }
        if (wasFound && directionCacheExpirationDelay < 0)
        {
            ERROR_ReportError(
                "Expecting positive "
                "MAC-DOT11-DIRECTION-CACHE-EXPIRATION-TIME "
                "parameter\n");
        }

        IO_ReadDouble(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11-DIRECTIONAL-NAV-AOA-DELTA-ANGLE",
            &wasFound,
            &defaultNavedDeltaAngle);

        if (!wasFound)
        {
            ERROR_ReportError(
                "Expecting "
                "MAC-DOT11-DIRECTIONAL-NAV-AOA-DELTA-ANGLE "
                "parameter\n");
        }

        dot11->directionalInfo = (DirectionalAntennaMacInfoType*)
            MEM_malloc(sizeof(DirectionalAntennaMacInfoType));

        InitDirectionalAntennalMacInfo(
            dot11->directionalInfo,
            directionCacheExpirationDelay,
            defaultNavedDeltaAngle);

        IO_ReadInt(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11-DIRECTIONAL-SHORT-PACKET-TRANSMIT-LIMIT",
            &wasFound,
            &dot11->directionalShortRetryLimit);
    }
    else
    {
        ERROR_ReportError(
            "Expecting YES or NO for "
            "MAC-DOT11-DIRECTIONAL-ANTENNA-MODE parameter\n");
    }

    ConvertVariableHWAddressTo802Address(node,
        &dot11->myMacData->macHWAddr,
        &dot11->selfAddr);

    // dot11s. Check if configured for mesh services.
    dot11->isMP = FALSE;

    // Check if node is a Mesh Point
    //       MAC-DOT11s-MESH-POINT YES | NO
    // Default is NO.
    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11s-MESH-POINT",
        &wasFound,
        retString);

    if (wasFound)
    {
        if (!strcmp(retString, "YES"))
        {
            dot11->isMP = TRUE;
        }
        else if (!strcmp(retString, "NO"))
        {
            dot11->isMP = FALSE;
        }
        else
        {
            ERROR_ReportError("MacDot11Init: "
                "Invalid value for MAC-DOT11s-MESH-POINT "
                "in configuration file.\n"
                "Expecting YES or NO.\n");
        }
    }

    // Initially, assume that interface is part of IBSS.
    // The use of a BSS address is not significant for
    // IBSSs in the implementation.
    dot11->bssAddr = dot11->selfAddr;

    // Association type
    // Format is :
    //      MAC-DOT11-ASSOCIATION_TYPE <value>
    // Default is MAC-DOT11-ASSOCIATION (value STATIC)
    //
    IO_ReadString(
        node->nodeId,
        &address,
        nodeInput,
        "MAC-DOT11-ASSOCIATION",
        &wasFound,
        retString);

    if (!wasFound)
    {
        dot11->associationType = DOT11_ASSOCIATION_NONE;
    }
    else if (strcmp(retString, "STATIC") == 0)
    {
        dot11->associationType = DOT11_ASSOCIATION_STATIC;

        // Static association type is currently disabled
        ERROR_ReportError(
                "MAC-DOT11-ASSOCIATION type STATIC "
                "is currently disabled.\n");
    }
    else if (strcmp(retString, "DYNAMIC") == 0)
    {
        dot11->associationType = DOT11_ASSOCIATION_DYNAMIC;

    }
    else
    {
        dot11->associationType = DOT11_ASSOCIATION_NONE;
    }

    // Init the BSS
    if (dot11->associationType == DOT11_ASSOCIATION_DYNAMIC )
    {
        // Ipv6 Modification
        if (networkType == NETWORK_IPV6)
        {
            MacDot11BssDynamicInit(node, nodeInput, dot11,
            subnetList, nodesInSubnet, subnetListIndex,
            0, 0,
            networkType,ipv6SubnetAddr, prefixLength);
        }
        else
        {
            MacDot11BssDynamicInit(node, nodeInput, dot11,
                subnetList, nodesInSubnet, subnetListIndex,
                subnetAddress, numHostBits);

            // dot11s. Auto default routes for the mesh
            NetworkUpdateForwardingTable(
                node,
                subnetAddress,
                ConvertNumHostBitsToSubnetMask(numHostBits),
                0,                          // use mapped address
                interfaceIndex,
                1,                          // cost
                ROUTING_PROTOCOL_DEFAULT);
        }
    }
    else
    {
        if (networkType == NETWORK_IPV6)
        {
            MacDot11BssInit(node, nodeInput, dot11,
            subnetList, nodesInSubnet, subnetListIndex,
            0, 0,
            networkType, ipv6SubnetAddr, prefixLength);
        }
        else
        {
            MacDot11BssInit(node, nodeInput, dot11,
                subnetList, nodesInSubnet, subnetListIndex,
                subnetAddress, numHostBits);
        }
    }


//--------------------HCCA-Updates Start---------------------------------//
    dot11->isHCCAEnable = FALSE;

    char input[MAX_STRING_LENGTH];

    IO_ReadString(
            node->nodeId,
            &address,
            nodeInput,
            "MAC-DOT11e-HCCA",
            &wasFound,
            input);
    if (wasFound)
    {
        if (!strcmp(input, "YES"))
        {
            // dot11s. MPs do not support HCCA.
            if (dot11->isMP)
            {
                ERROR_ReportError("MacDot11Init: "
                    "Mesh Points do not support QoS and HCCA.\n"
                    "MAC-DOT11e-HCCA is YES in configuration.\n");
            }

            dot11->isHCCAEnable = TRUE;
            dot11->associatedWithHC = FALSE;
            if (networkType == NETWORK_IPV6)
            {
                MacDot11eHCInit(
                    node,
                    dot11,
                    phyModel,
                    nodeInput,
                    networkType,
                    ipv6SubnetAddr,
                    prefixLength);

            }
            else
            {
                MacDot11eHCInit(node, dot11, phyModel, nodeInput, networkType);
            }

        }
        else if (!strcmp(input, "NO"))
        {
            dot11->isHCCAEnable = FALSE;
        }
        else
        {
            ERROR_ReportError("MacDot11Init: "
                "Invalid value for MAC-DOT11e-HCCA in configuration file.\n"
                "Expecting YES or NO.\n");
        }
    }

//--------------------HCCA-Updates End-----------------------------------//
    if (MacDot11IsAp(dot11) ||
        MacDot11IsBssStation(dot11))
    {

        if (networkType == NETWORK_IPV6)
        {
            MacDot11CfpInit(node, nodeInput, dot11,
                subnetList, nodesInSubnet, subnetListIndex,
                0, 0, networkType, ipv6SubnetAddr, prefixLength);
        }
        else
        {
        MacDot11CfpInit(node, nodeInput, dot11,
            subnetList, nodesInSubnet, subnetListIndex,
            subnetAddress, numHostBits);
        }
    }

    if (MacDot11IsIBSSStation(dot11))
    {
        memset(&(dot11->bssAddr), 0,
                sizeof(dot11->bssAddr));
        if (MacDot11IsQoSEnabled(node,dot11))
            dot11->bssAddr.byte[0] = 0x01;
        else
            dot11->bssAddr.byte[0] = 0x00;
    }

    // dot11s
    ListInit(node, &(dot11->mgmtQueue));
    ListInit(node, &(dot11->dataQueue));
    //dot11 Mgmnt
    ListInit(node, &(dot11->mngmtQueue));

    if (dot11->isMP)
    {
        Dot11s_Init(node, nodeInput, dot11, networkType);
    }

    MacDot11TraceInit(node, nodeInput, dot11);

#ifdef PARALLEL //Parallel
    PARALLEL_SetProtocolIsNotEOTCapable(node);
    PARALLEL_SetMinimumLookaheadForInterface(node,
                                             dot11->delayUntilSignalAirborn);
#endif //endParallel

    std::string path;
    D_Hierarchy *h = &node->partitionData->dynamicHierarchy;

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "extraPropDelay",
            path))
    {
        h->AddObject(
            path,
            new D_ClocktypeObj(&dot11->extraPropDelay));
    }

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "propagationDelay",
            path))
    {
        h->AddObject(
            path,
            new D_ClocktypeObj(&dot11->propagationDelay));
    }

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "beaconInterval",
            path))
    {
        h->AddObject(
            path,
            new D_ClocktypeObj(&dot11->beaconInterval));
    }

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "pktsToSend",
            path))
    {
        h->AddObject(
            path,
            new D_Int32Obj(&dot11->pktsToSend));
    }

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "unicastPacketsSentDcf",
            path))
    {
        h->AddObject(
            path,
            new D_Int32Obj(&dot11->unicastPacketsSentDcf));
    }

    if (h->CreateMacPath(
            node,
            interfaceIndex,
            "802.11",
            "broadcastPacketsSentDcf",
            path))
    {
        h->AddObject(
            path,
            new D_Int32Obj(&dot11->broadcastPacketsSentDcf));
    }
}//MacDot11Init



//--------------------------------------------------------------------------
//  NAME:        MacDot11PrintStats
//  PURPOSE:     Print protocol statistics.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               MacDataDot11* dot11
//                  Pointer to Dot11 structure
//               int interfaceIndex
//                  interfaceIndex, Interface index
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
static //inline//
void MacDot11PrintStats(Node* node, MacDataDot11* dot11,
    int interfaceIndex)
{
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "Packets from network = %d",
           (int) dot11->pktsToSend);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Unicast packets sent to channel = %d",
           dot11->unicastPacketsSentDcf + dot11->unicastPacketsSentCfp);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Broadcast packets sent to channel = %d",
           (int)(dot11->broadcastPacketsSentDcf + dot11->broadcastPacketsSentCfp));
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Unicast packets received clearly = %d",
           dot11->unicastPacketsGotDcf + dot11->unicastPacketsGotCfp);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Broadcast packets received clearly = %d",
           dot11->broadcastPacketsGotDcf + dot11->broadcastPacketsGotCfp);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

#ifdef ADDON_MIH
    sprintf(buf, "Number of Link_Parameters_Report indications sent = %d",
               dot11->numLink_Parameters_Report_indication);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Number of Link_Going_Down indications sent = %d",
                   dot11->numLinkGoingDownIndSent);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Number of Link_Down indications sent = %d",
                      dot11->numLink_Down_indication);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Number of Link_Up indications sent = %d",
    		dot11->numLink_Up_indication);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Number of Link_Handover_Imminent indications sent = %d",
    		dot11->numLinkHandoverIminent);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Number of Link_Handover_Complete indications sent = %d",
    		dot11->numLinkHandoverComplete);
    IO_PrintStat(node, "MAC", DOT11_MAC_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

#endif

    sprintf(buf, "Unicasts sent = %d",
           (int) dot11->unicastPacketsSentDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Broadcasts sent = %d",
           (int) dot11->broadcastPacketsSentDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Unicasts received = %d",
           dot11->unicastPacketsGotDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Broadcasts received = %d",
           dot11->broadcastPacketsGotDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "CTS packets sent = %d",
           dot11->ctsPacketsSentDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "RTS packets sent = %d",
           dot11->rtsPacketsSentDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "ACK packets sent = %d",
           dot11->ackPacketsSentDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "RTS retransmissions due to timeout = %d",
           dot11->retxDueToCtsDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    // This value is captured but not printed. It is rarely non-zero.
    //sprintf(buf, "RTS retransmits aborted = %d",
    //       dot11->retxAbortedRtsDcf);
    //IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Packet retransmissions due to ACK timeout = %d",
           dot11->retxDueToAckDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

    sprintf(buf, "Packet drops due to retransmission limit = %d",
           dot11->pktsDroppedDcf);
    IO_PrintStat(node, "MAC", DOT11_DCF_STATS_LABEL, ANY_DEST, interfaceIndex, buf);

}//MacDot11PrintStats


//--------------------------------------------------------------------------
//  NAME:        MacDot11Finalize.
//  PURPOSE:     Print stats and clear protocol variables.
//  PARAMETERS:  Node* node
//                  Pointer to node
//               int interfaceIndex
//                  interfaceIndex, Interface index
//  RETURN:      None.
//  ASSUMPTION:  None.
//--------------------------------------------------------------------------
void MacDot11Finalize(
    Node* node,
    int interfaceIndex)
{
    MacDataDot11* dot11 =
        (MacDataDot11*)node->macData[interfaceIndex]->macVar;
    DOT11_SeqNoEntry* entry;
    DOT11_DataRateEntry* rEntry;

#if 0
        StatsDb* db = node->partitionData->statsDb;
        // If MAC aggregate table is enabled, we must free the aggreagte
        // stats
        if (db->statsAggregateTable->createMacAggregateTable)
        {
            MEM_free(dot11->aggregateStats);
        }
#endif

    if (dot11->myMacData->macStats == TRUE) {
        MacDot11PrintStats(node, dot11, interfaceIndex);
        MacDot11CfpPrintStats(node, dot11, interfaceIndex);
        MacDot11ManagementPrintStats(node, dot11, interfaceIndex);
        MacDot11ApPrintStats(node, dot11, interfaceIndex);
//---------------------------Power-Save-Mode-Updates---------------------//
        MacDot11PSModePrintStats(node, dot11, interfaceIndex);
//---------------------------Power-Save-Mode-End-Updates-----------------//
        if (MacDot11IsQoSEnabled(node, dot11))
        {
//--------------------HCCA-Updates Start---------------------------------//
           if (dot11->isHCCAEnable)
               MacDot11eCfpPrintStats(node,dot11,interfaceIndex);
//--------------------HCCA-Updates End-----------------------------------//

           MacDot11ACPrintStats(node, dot11, interfaceIndex);
           char buf[MAX_STRING_LENGTH];

           sprintf(buf, "QoS Data Frames send = %u",
                   dot11->totalNoOfQoSDataFrameSend);

           IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                        interfaceIndex, buf);

           sprintf(buf, "Non-QoS Data Frames send = %u",
                   dot11->totalNoOfnQoSDataFrameSend);

           IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                        interfaceIndex, buf);

           sprintf(buf, "QoS Data Frame received = %u",
                   dot11->totalNoOfQoSDataFrameReceived);

           IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                        interfaceIndex, buf);

           sprintf(buf, "Non-QoS Data Frame received = %u",
                   dot11->totalNoOfnQoSDataFrameReceived);

           IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                        interfaceIndex, buf);

        }
    }
    // Clear PC related variables
    MacDot11CfpFreeStorage(node, interfaceIndex);


    // Clear sequence list
    while (dot11->seqNoHead != NULL) {
        entry = dot11->seqNoHead;
        dot11->seqNoHead = entry->next;
        MEM_free(entry);
    }

        if (MacDot11IsQoSEnabled(node,dot11))
        {
            for (int AcIndex =0; AcIndex<4; AcIndex++)
            {
                entry = dot11->ACs[AcIndex].seqNoHead;
                if (entry!= NULL)
                {
                    dot11->ACs[AcIndex].seqNoHead = entry->next;
                    MEM_free(entry);
                    if (dot11->ACs[AcIndex].frameToSend!= NULL)
                        MESSAGE_Free(node,dot11->ACs[AcIndex].frameToSend);
                    if (dot11->ACs[AcIndex].frameInfo != NULL)
                        MEM_free(dot11->ACs[AcIndex].frameInfo);
                }
            }
        }
    // Clear data rate list
    while (dot11->dataRateHead != NULL) {
        rEntry = dot11->dataRateHead;
        dot11->dataRateHead = rEntry->next;
        MEM_free(rEntry);
    }

    // dot11s. MP finalization.
    // Clear frame queues
    Dot11sMgmtQueue_Finalize(node, dot11);
    Dot11sDataQueue_Finalize(node, dot11);
    if (dot11->txFrameInfo != NULL)
    {
        Dot11s_MemFreeFrameInfo(node, &(dot11->txFrameInfo), TRUE);
    }
    // Print Mesh Point stats and clear data, if applicable
    if (dot11->isMP) {
        Dot11s_Finalize(node, dot11, interfaceIndex);
    }

    // Free Dot11 data structure
    MEM_free(dot11);
}// MacDot11Finalize


// /**
// FUNCTION   :: MacDot11eTrafficSpecification
// LAYER      :: MAC
// PURPOSE    :: Insert traffic in proper AC
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// + msg       : Message* : Pointer to the Message
// + priority  : TosType : Priority
// + currentNextHopAddress  : Mac802Address : next hop address
// + networkType  : int : Pointer to network type
// RETURN     :: void : NULL
// **/
void MacDot11eTrafficSpecification(
        Node* node,
        MacDataDot11* dot11,
        Message* msg,
        TosType priority,
        Mac802Address currentNextHopAddress,
        int networkType)
{
    int acIndex = 0;
    if (priority == 0)
    {
        // If priority 0 then put it to BK
        acIndex = DOT11e_AC_BK;
    }
    else if (priority >= 1 && priority <= 2)
    {
        // If priority 1 - 2 then put it to BE
        acIndex = DOT11e_AC_BE;
    }
    else if (priority >= 3 && priority <= 5)
    {
        // If priority 3 - 5 then put it to VI
        acIndex = DOT11e_AC_VI;
    }
    else
    {
        // If priority 6 - 7 then put it to VO
        acIndex = DOT11e_AC_VO;
    }
    dot11->ACs[acIndex].frameToSend = msg;
    dot11->ACs[acIndex].priority = priority;
    dot11->ACs[acIndex].currentNextHopAddress = currentNextHopAddress;
    dot11->ACs[acIndex].networkType = networkType;
    dot11->ACs[acIndex].totalNoOfthisTypeFrameQueued++;

    if (DEBUG_QA)
    {
        unsigned char* frame_type =
            (unsigned char*) MESSAGE_ReturnPacket(msg);
        printf(" Node %u ac[ %d] used for priority %d for frame %x\n",
            node->nodeId, acIndex, priority, *frame_type);
    }
}//MacDot11eTrafficSpecification


// /**
// FUNCTION   :: MacDot11eEDCA_TOXPSchedulerRetrieveFrame
// LAYER      :: MAC
// PURPOSE    :: To retrieve frame from the contention winning AC
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// + msg       : Message** : Pointer to the Message array
// + priority  : TosType* : Priority
// + currentNextHopAddress  : Mac802Address* : next hop address
// + networkType  : int* : Network type
// RETURN     :: void : NULL
// **/
void MacDot11eEDCA_TOXPSchedulerRetrieveFrame(
        Node* node,
        MacDataDot11* dot11,
        Message** msg,
        TosType* priority,
        Mac802Address* currentNextHopAddress,
        int* networkType)
{
    // Rank the Access Category according to transmission opportunity.
    // the highest opportunity limit wins the contention.

    clocktype highestTXOPLimit = 0;
    int acIndex = 0;

    for (int acCounter = 0; acCounter < DOT11e_NUMBER_OF_AC;
         acCounter++)
    {
        if (highestTXOPLimit <= dot11->ACs[acCounter].TXOPLimit &&
            dot11->ACs[acCounter].frameToSend != NULL)
        {
            highestTXOPLimit = dot11->ACs[acCounter].TXOPLimit;
            acIndex = acCounter;
        }

    }

    if (dot11->ACs[acIndex].frameToSend == NULL)
    {
        ERROR_ReportError("There is no frame in AC\n");
    }

    (*msg) = dot11->ACs[acIndex].frameToSend;
    (*priority) = dot11->ACs[acIndex].priority;
    (*currentNextHopAddress) = dot11->ACs[acIndex].currentNextHopAddress;
    (*networkType) = dot11->ACs[acIndex].networkType;
    dot11->currentACIndex = acIndex;

    // The frame is transfered and hence set to NULL;
    dot11->ACs[acIndex].frameToSend = NULL;
    dot11->ACs[acIndex].BO = 0;
    dot11->ACs[acIndex].currentNextHopAddress = INVALID_802ADDRESS;

    // Update Statistics value:
    dot11->ACs[acIndex].totalNoOfthisTypeFrameDeQueued++;

}//MacDot11eEDCA_TOXPSchedulerRetrieveFrame


// /**
// FUNCTION   :: MacDot11eSetCurrentMessage
// LAYER      :: MAC
// PURPOSE    :: Set current Message after negociating in between the ACs.
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// + networkType  : int* : Network type
// RETURN     :: void : NULL
// **/
void MacDot11eSetCurrentMessage(
        Node* node,
        MacDataDot11* dot11)
{
    // Rank the Access Category according to transmission opportunity.
    // the highest opportunity limit wins the contention.

    clocktype TotalTime = 0;

    for (int acCounter = 0 ; acCounter < DOT11e_NUMBER_OF_AC;acCounter++) {
        if (dot11->ACs[acCounter].frameInfo != NULL)
        {
             if (dot11->ACs[acCounter].frameInfo->msg != NULL) {
                 if (TotalTime == 0) {
                            TotalTime=dot11->ACs[acCounter].BO +
                                                 dot11->ACs[acCounter].AIFS;
                    dot11->currentACIndex = acCounter;
                 }else {
                        if (TotalTime>dot11->ACs[acCounter].BO +
                                      dot11->ACs[acCounter].AIFS) {
                            TotalTime=dot11->ACs[acCounter].BO +
                                            dot11->ACs[acCounter].AIFS;
                            dot11->currentACIndex = acCounter;
                        }
                   }
              }
         }
   } // end for

    if (dot11->ACs[dot11->currentACIndex].frameInfo->msg == NULL) {
        ERROR_ReportError("There is no frame in AC\n");
    }

    //DOT11_FrameInfo* frINfo = dot11->ACs[dot11->currentACIndex].frameInfo;
    dot11->dot11TxFrameInfo = dot11->ACs[dot11->currentACIndex].frameInfo;
    dot11->currentMessage = dot11->ACs[dot11->currentACIndex].frameInfo->msg;
    dot11->ipNextHopAddr = dot11->ACs[dot11->currentACIndex].frameInfo->RA;

    if (MacDot11IsBssStation(dot11)&&
        (dot11->bssAddr!= dot11->selfAddr)) {
        dot11->currentNextHopAddress = dot11->bssAddr;
    }else {
        dot11->currentNextHopAddress =
              dot11->ACs[dot11->currentACIndex].frameInfo->RA;
    }
    dot11->BO = dot11->ACs[dot11->currentACIndex].BO;
    dot11->SSRC = dot11->ACs[dot11->currentACIndex].QSRC ;
    dot11->SLRC = dot11->ACs[dot11->currentACIndex].QLRC ;

}//MacDot11eSetCurrentMessage

// /**
// FUNCTION   :: MacDot11SetCurrentMessage
// LAYER      :: MAC
// PURPOSE    :: Set current Message after negociating in between the ACs.
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// + networkType  : int* : Network type
// RETURN     :: void : NULL
// **/
void MacDot11SetCurrentMessage(
        Node* node,
        MacDataDot11* dot11)
{
    // Rank the Access Category according to transmission opportunity.
    // the highest opportunity limit wins the contention.
    int acIndex = 0;
    dot11->currentACIndex = 0;

    if (dot11->ACs[dot11->currentACIndex].frameInfo->msg == NULL) {
        ERROR_ReportError("There is no frame in AC\n");
    }
    //DOT11_FrameInfo* frINfo = dot11->ACs[dot11->currentACIndex].frameInfo;
    dot11->dot11TxFrameInfo = dot11->ACs[dot11->currentACIndex].frameInfo;
    dot11->currentMessage = dot11->ACs[dot11->currentACIndex].frameInfo->msg;
    dot11->ipNextHopAddr = dot11->ACs[dot11->currentACIndex].frameInfo->RA;

    if (MacDot11IsBssStation(dot11)&& (dot11->bssAddr!= dot11->selfAddr)) {
        dot11->currentNextHopAddress = dot11->bssAddr;
    }else {
        dot11->currentNextHopAddress =
             dot11->ACs[dot11->currentACIndex].frameInfo->RA;
    }
    dot11->BO = dot11->ACs[dot11->currentACIndex].BO;
    dot11->SSRC = dot11->ACs[acIndex].QSRC ;
    dot11->SLRC = dot11->ACs[acIndex].QLRC ;
}//MacDot11SetCurrentMessage

// /**
// FUNCTION   :: MacDot11ACPrintStats
// LAYER      :: MAC
// PURPOSE    :: To print statistics
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// RETURN     :: void : NULL
// **/
void MacDot11ACPrintStats(Node* node, MacDataDot11* dot11,
    int interfaceIndex)
{
    char buf[MAX_STRING_LENGTH];

     for (int acCounter = DOT11e_AC_BK;
             acCounter < DOT11e_NUMBER_OF_AC;
             acCounter++)
    {

        sprintf(buf, "AC[%d] Total Frame Queued = %u",
             acCounter, dot11->ACs[acCounter].totalNoOfthisTypeFrameQueued);
        IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                     interfaceIndex, buf);

        sprintf(buf, "AC[%d] Total Frame de-Queued = %u",
                  acCounter, dot11->ACs[acCounter].totalNoOfthisTypeFrameDeQueued);
        IO_PrintStat(node, "MAC", DOT11e_MAC_STATS_LABEL, ANY_DEST,
                     interfaceIndex, buf);

    }
}//MacDot11ACPrintStats


// /**
// FUNCTION   :: MacDot11eIsAssociatedWithQAP
// LAYER      :: MAC
// PURPOSE    :: To check if the node is associated with QAP or not
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : MacDataDot11* : Pointer to the MacData structure
// RETURN     :: BOOL : TRUE or FALSE
// **/
BOOL MacDot11eIsAssociatedWithQAP(Node* node, MacDataDot11* dot11)
{
   return dot11->associatedWithQAP;
}//MacDot11eIsAssociatedWithQAP

#ifdef ADDON_DB
// /**
// FUNCTION   :: MacDot11GetPacketProperty
// LAYER      :: MAC
// PURPOSE    :: Called by the Mac Summary Stats DB
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : Message *: Pointer to the input message
// RETURN     :: BOOL : TRUE or FALSE
// **/
void MacDot11GetPacketProperty(Node *node, Message *msg,
    int interfaceIndex, int & headerSize,
    std::string &dstAddr,
    BOOL &isCtrlFrame,
    BOOL &isMyFrame,
    BOOL &isAnyFrame)
{
    unsigned char* frame_type;
    DOT11_ShortControlFrame* hdr ;
    DOT11_FrameHdr* hdr2;

    char srcAdd[24] = {'\0'};
    char destAdd[24] = {'\0'};

    if (msg != NULL)
    {
        char buf[20];

        frame_type = (unsigned char *)MESSAGE_ReturnPacket(msg);
        hdr = (DOT11_ShortControlFrame *)MESSAGE_ReturnPacket(msg);
        MacDot11MacAddressToStr(destAdd, &hdr->destAddr);

        switch(hdr->frameType)
        {
        case DOT11_ASSOC_REQ:
        case DOT11_ASSOC_RESP:
        case DOT11_REASSOC_REQ:
        case DOT11_REASSOC_RESP:
        case DOT11_BEACON :
        case DOT11_AUTHENTICATION:
//---------------------DOT11e--HCCA-Updates-------------------------------//
        case DOT11_ACTION:
//---------------------DOT11e--HCCA-End-Updates---------------------------//
        case DOT11_RTS:
            headerSize = sizeof(DOT11_FrameHdr);
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = TRUE;
            break;
        case DOT11_CTS:
        case DOT11_ACK:
            headerSize = DOT11_SHORT_CTRL_FRAME_SIZE;
            isCtrlFrame = TRUE;
            break;
        case DOT11_DATA:
            headerSize = DOT11_DATA_FRAME_HDR_SIZE;
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = FALSE;
            break;
        case DOT11_QOS_DATA:
            headerSize = sizeof(DOT11e_FrameHdr);
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = FALSE;
            break;
//---------------------------Power-Save-Mode-Updates---------------------//
        case DOT11_ATIM:
            headerSize = sizeof(DOT11_ATIMFrame);
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = TRUE;
            break;
        case DOT11_PS_POLL:
            headerSize = DOT11_DATA_FRAME_HDR_SIZE;
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = TRUE;
            break;
//---------------------------Power-Save-Mode-End-Updates-----------------//
        // dot11s. Trace Mesh data.
        case DOT11_MESH_DATA:
            headerSize = DOT11s_DATA_FRAME_HDR_SIZE;
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            isCtrlFrame = FALSE;
            break;
        default:
            sprintf( buf, "%d", hdr->frameType);
            break;
        }

        dstAddr = destAdd;

        MacDataDot11* dot11 =
            (MacDataDot11*)node->macData[interfaceIndex]->macVar;
        if (NetworkIpIsUnnumberedInterface(node, interfaceIndex))
        {
            MacHWAddress linkAddr;
            Convert802AddressToVariableHWAddress(
                                    node, &linkAddr, &(hdr->destAddr));

            isMyFrame = MAC_IsMyAddress(node, &linkAddr);
        }
        else
        {
            isMyFrame = (dot11->selfAddr == hdr->destAddr);
        }
        if (!isMyFrame)
        {
            if (hdr->destAddr == ANY_MAC802)
            {
                isAnyFrame = TRUE;
            }
        }
        return;
    }
}

// /**
// FUNCTION   :: MacDot11GetPacketProperty
// LAYER      :: MAC
// PURPOSE    :: Called by the Mac Events Stats DB
// PARAMETERS ::
// + node      : Node* : Pointer to the node
// + dot11     : Message *: Pointer to the input message
// RETURN     :: BOOL : TRUE or FALSE
// **/
void MacDot11GetPacketProperty(Node *node, Message *msg,
    int interfaceIndex,
    MacDBEventType eventType,
    StatsDBMacEventParam &macParam,
    BOOL &isMyFrame, BOOL &isAnyFrame)
{

    if (msg != NULL)
    {
        DOT11_FrameHdr* hdr2;
        char srcAdd[24] = {'\0'};
        char destAdd[24] = {'\0'};

        DOT11_ShortControlFrame *hdr =
            (DOT11_ShortControlFrame *)MESSAGE_ReturnPacket(msg);
        MacDot11MacAddressToStr(destAdd, &hdr->destAddr);
        BOOL srcAddrSpecified = FALSE;

        switch(hdr->frameType)
        {
        case DOT11_ASSOC_REQ:
            macParam.SetFrameType("ASSOC_REQ");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_ASSOC_RESP:
            macParam.SetFrameType("ASSOC_RESP");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_REASSOC_REQ:
            macParam.SetFrameType("REASSOC_REQ");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_REASSOC_RESP:
            macParam.SetFrameType("REASSOC_RESP");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));            
            hdr2 = 
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_DISASSOCIATION :
            macParam.SetFrameType("DISASSOCIATION");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));            
            hdr2 = 
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_PROBE_REQ :
            macParam.SetFrameType("PROBE_REQUEST");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));            
            hdr2 = 
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_PROBE_RESP :
            macParam.SetFrameType("PROBE_RESPONSE");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));            
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_BEACON :
            macParam.SetFrameType("BEACON");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_AUTHENTICATION:
            macParam.SetFrameType("AUTHENTICATION");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
//---------------------DOT11e--HCCA-Updates-------------------------------//
        case DOT11_ACTION:
            macParam.SetFrameType("ACTION");
            macParam.SetHdrSize(sizeof(DOT11_FrameHdr));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
//---------------------DOT11e--HCCA-End-Updates---------------------------//
        case DOT11_RTS:
            macParam.SetFrameType("RTS");
            macParam.SetHdrSize(DOT11_LONG_CTRL_FRAME_SIZE);
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CTS:
            macParam.SetFrameType("CTS");
            macParam.SetHdrSize(DOT11_SHORT_CTRL_FRAME_SIZE);
            break;
        case DOT11_ACK :
            macParam.SetFrameType("ACK");
            macParam.SetHdrSize(DOT11_SHORT_CTRL_FRAME_SIZE);
            break;
        case DOT11_DATA:
            macParam.SetFrameType("DATA");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_DATA_ACK:
            macParam.SetFrameType("CF_DATA_ACK");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_NULLDATA:
            macParam.SetFrameType("CF_NULLDATA");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_ACK:
            macParam.SetFrameType("CF_ACK");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_DATA_POLL:
            macParam.SetFrameType("CF_DATA_POLL");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_DATA_POLL_ACK:
            macParam.SetFrameType("CF_DATA_POLL_ACK");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_POLL:
            macParam.SetFrameType("CF_POLL");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_POLL_ACK:
            macParam.SetFrameType("CF_POLL_ACK");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 = (DOT11_FrameHdr*) MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_QOS_DATA:
            macParam.SetFrameType("QOS_DATA");
            macParam.SetHdrSize(sizeof(DOT11e_FrameHdr));
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_QOS_CF_DATA_ACK:
            macParam.SetFrameType("QOS_DATA_CF_ACK");    
            macParam.SetHdrSize(sizeof(DOT11e_FrameHdr));
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_QOS_CF_NULLDATA:
            macParam.SetFrameType("QOS_CF_NULLDATA");    
            macParam.SetHdrSize(sizeof(DOT11e_FrameHdr));
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_QOS_CF_POLL:
             macParam.SetFrameType("QOS_CF_POLL");    
            macParam.SetHdrSize(sizeof(DOT11e_FrameHdr));
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_QOS_POLL_ACK:
             macParam.SetFrameType("QOS_POLL_ACK");    
            macParam.SetHdrSize(sizeof(DOT11e_FrameHdr));
            hdr2 = (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_END:
            macParam.SetFrameType("CF_END");
            macParam.SetHdrSize(DOT11_LONG_CTRL_FRAME_SIZE);            
            hdr2 = 
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_CF_END_ACK:
            macParam.SetFrameType("CF_END_ACK");
            macParam.SetHdrSize(DOT11_LONG_CTRL_FRAME_SIZE);            
            hdr2 = 
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);            
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
         
//---------------------------Power-Save-Mode-Updates---------------------//
        case DOT11_ATIM:
            macParam.SetFrameType("ATIM");
            macParam.SetHdrSize(sizeof(DOT11_ATIMFrame));
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        case DOT11_PS_POLL:
            macParam.SetFrameType("PS_POLL");
            macParam.SetHdrSize(DOT11_DATA_FRAME_HDR_SIZE);
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
//---------------------------Power-Save-Mode-End-Updates-----------------//
        // dot11s. Trace Mesh data.
        case DOT11_MESH_DATA:
            macParam.SetFrameType("MESH_DATA");
            macParam.SetHdrSize(DOT11s_DATA_FRAME_HDR_SIZE);
            hdr2 =
                (DOT11_FrameHdr *)MESSAGE_ReturnPacket(msg);
            MacDot11MacAddressToStr(srcAdd, &hdr2->sourceAddr);
            srcAddrSpecified = TRUE;
            break;
        default:
            printf("unrecognized frameType %d in MacDot11GetPacketProperty.\n",
                hdr->frameType);
            return ;
        }

        macParam.SetDstAddr(destAdd);
        if (srcAddrSpecified)
        {
            macParam.SetSrcAddr(srcAdd);
        }

        MacDataDot11* dot11 =
            (MacDataDot11*)node->macData[interfaceIndex]->macVar;
        if (NetworkIpIsUnnumberedInterface(node, interfaceIndex))
        {
            MacHWAddress linkAddr;
            Convert802AddressToVariableHWAddress(
                                    node, &linkAddr, &(hdr->destAddr));

            isMyFrame = MAC_IsMyAddress(node, &linkAddr);
        }
        else
        {
            isMyFrame = (dot11->selfAddr == hdr->destAddr);
        }
        if (!isMyFrame)
        {
            if (hdr->destAddr == ANY_MAC802)
            {
                isAnyFrame = TRUE;
            }
        }

        if (hdr->frameType != DOT11_QOS_DATA &&
            hdr->frameType != DOT11_DATA &&
            hdr->frameType != DOT11_MESH_DATA &&
            hdr->frameType != DOT11_CF_DATA_ACK &&
            hdr->frameType != DOT11_CF_DATA_POLL &&
            hdr->frameType != DOT11_CF_DATA_POLL_ACK &&
            hdr->frameType != DOT11_CF_NULLDATA &&
            hdr->frameType != DOT11_CF_ACK &&
            hdr->frameType != DOT11_CF_POLL &&
            hdr->frameType != DOT11_CF_POLL_ACK &&
            hdr->frameType != DOT11_QOS_CF_DATA_ACK &&
            hdr->frameType != DOT11_QOS_CF_NULLDATA &&
            hdr->frameType != DOT11_QOS_CF_POLL &&
            hdr->frameType != DOT11_QOS_POLL_ACK)
        {
            // MAC_DOT_11 control frame type
            return ;
        }

        int shiftHeaderSize = 0;
        if (LlcIsEnabled(node, interfaceIndex))
        {
            shiftHeaderSize = sizeof(LlcHeader);
        }
        shiftHeaderSize += macParam.m_HeaderSize;

        IpHeaderType* ipHdr = (IpHeaderType*) (MESSAGE_ReturnPacket(msg)
            + shiftHeaderSize);
        ip6_hdr *ipv6Header =
            (ip6_hdr *) MESSAGE_ReturnPacket(msg) + shiftHeaderSize ;

        /*if (IpHeaderGetVersion(ipHdr->ip_v_hl_tos_len) == IPVERSION4)
        {
            macParam.SetPriority(IpHeaderGetTOS(ipHdr->ip_v_hl_tos_len));
        }
        else if (ip6_hdrGetVersion(ipv6Header->ipv6HdrVcf) ==
            IPV6_VERSION )
        {
            macParam.SetPriority(ip6_hdrGetClass(ipv6Header->ipv6HdrVcf));
        }
        else {
            printf("Please insert code to handle none IP code"
                "case in MacDot11GetPacketProperty, protocol = %d, msgSize = %d\n",
                IpHeaderGetVersion(ipHdr->ip_v_hl_tos_len),
                MESSAGE_ReturnPacketSize(msg));
        }*/
        return;
    }
}
#endif
