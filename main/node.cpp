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

#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "api.h"
#include "clock.h"
#include "main.h"
#include "node.h"
#include "partition.h"
#include "scheduler.h"
#include "external.h"
#include "timer_manager.h"
#include "user.h"
#include "context.h"

#ifdef ADDON_BOEINGFCS
#include "network_ip.h"
#include "ces2.h"
#endif

#ifdef ADDON_NGCNMS
#include "phy_abstract.h"
#define DEBUG_NODE_FAULT 0
#endif

#ifdef ADDON_DB
#include "dbapi.h"
#endif


void Node::enterInterface(int intf)
{
  //printf("Entering interface %d/%d\n", nodeId, intf);
  //fflush(stdout);

// These assertions are not safe until all entry and exit points are included
  //assert(currentInterface == InterfaceNone);
  currentInterface = intf;
}

void Node::exitInterface()
{
  //printf("Exiting interface %d/%d\n", nodeId, currentInterface);
  //fflush(stdout);

  //assert(currentInterface != InterfaceNone);
  currentInterface = InterfaceNone;
}

int Node::ifidx()
{
    assert(currentInterface != InterfaceNone);
    return currentInterface;
}

/*
 * FUNCTION     NODE_CreateNode
 * PURPOSE      Function used to allocate and initialize the nodes on a
 *              partition.
 *
 * Parameters
 *     partitionData: a pre-initialized partition data structure
 *     numNodes:      the number of nodes in this partition
 *     seedVal:       the global random seed
 */
Node* NODE_CreateNode(PartitionData* partitionData,
                      NodeAddress    nodeId,
                      int            partitionId,
                      int            index)
{
    Node* newNode;
    int   i;

#ifdef DEBUG
    printf("Creating node %d on partition %d\n", nodeId, partitionData->partitionId);
#endif
    newNode = (Node*) MEM_malloc(sizeof(Node));
    assert(newNode != 0);
    memset(newNode, 0, sizeof(Node));

    newNode->partitionData = partitionData;
    newNode->nodeIndex     = index;
    newNode->numNodes      = partitionData->numNodes;
    newNode->nodeId        = nodeId;
    newNode->partitionId   = partitionId;

    newNode->guiOption     = partitionData->guiOption;

    newNode->currentTime      = &partitionData->theCurrentTime;
    newNode->startTime        = &partitionData->nodeInput->startSimClock;
    newNode->numberInterfaces = 0;
    newNode->numberChannels   = partitionData->numChannels;
    newNode->propChannel      = partitionData->propChannel;

    newNode->packetTrace      = partitionData->traceEnabled;
    newNode->packetTraceSeqno = 0;

    newNode->globalSeed       = partitionData->seedVal;

    newNode->mobilityData     = partitionData->nodePositions[index].mobilityData;

    // allocate definable size structures to let users change constants
    newNode->propData =
        (PropData*)MEM_malloc(sizeof(PropData) * partitionData->numChannels);
    assert(newNode->propData != NULL);
    memset(newNode->propData, 0,
           sizeof(PropData) * partitionData->numChannels);
    for (i = 0; i < partitionData->numChannels; i++) {
      newNode->propData[i].phyListening = (BOOL*) MEM_malloc(sizeof(BOOL) * MAX_NUM_PHYS);
      assert(newNode->propData[i].phyListening != NULL);
      memset(newNode->propData[i].phyListening, 0, sizeof(BOOL) * MAX_NUM_PHYS);
    }

    newNode->phyData = (PhyData **) MEM_malloc(sizeof(PhyData*) * MAX_NUM_PHYS);
    assert(newNode->phyData != NULL);
    memset(newNode->phyData, 0, sizeof(PhyData*) * MAX_NUM_PHYS);

    newNode->macData = (MacData**)MEM_malloc(sizeof(MacData*) * MAX_NUM_INTERFACES);
    assert(newNode->macData != 0);
    memset(newNode->macData, 0, sizeof(MacData*) * MAX_NUM_INTERFACES);

    newNode->networkData.networkVar = NULL;

    newNode->numAtmInterfaces = 0;
    newNode->atmLayer2Data = (AtmLayer2Data**)
        MEM_malloc(sizeof(AtmLayer2Data*) * MAX_NUM_INTERFACES);
    assert(newNode->atmLayer2Data != NULL);
    memset(newNode->atmLayer2Data,
           0,
           sizeof(AtmLayer2Data*) * MAX_NUM_INTERFACES);

    newNode->userData = (UserData *) MEM_malloc(sizeof(UserData));
    assert(newNode->userData != NULL);
    memset(newNode->userData, 0, sizeof(UserData));

#ifdef SNMP_INTERFACE
    newNode->snmpAgent = NULL;
    sprintf(newNode->sysContact, "EXata SNMP Engineer");
    sprintf(newNode->sysLocation, "in EXata");
#endif

    newNode->currentInterface = Node::InterfaceNone;

#ifdef HITL_INTERFACE
    // isHitlNode will be set when HITL is initialized
    newNode->isHitlNode = FALSE;
#endif

#ifdef ADDON_BOEINGFCS
    std::string path;
    D_Hierarchy *h = &partitionData->dynamicHierarchy;

    if (h->IsEnabled() && partitionId == partitionData->partitionId)
    {
        if (h->CreateNodePath(
                newNode,
                "sleep",
                path))
        {
            h->AddObject(
                path,
                new D_Sleep(newNode));
        }

        if (h->CreateNodePath(
                newNode,
                "C2Node",
                path))
        {
            h->AddObject(
                path,
                new D_C2Node(newNode));
        }

        if (h->CreateNodePath(
                newNode,
                "multicastGroups",
                path))
        {
            h->AddObject(
                path,
                new D_MulticastGroups(newNode));
        }
    }
#endif

#ifdef GATEWAY_INTERFACE
    newNode->internetGateway = INVALID_ADDRESS;
#endif

#ifdef CYBER_LIB
    newNode->resourceManager = NULL;     
#endif
#ifdef ADDON_MIH
	newNode->isMihfEnabled = FALSE;
#endif

    return newNode;
    }


/*
 * FUNCTION     NODE_ProcessEvent
 * PURPOSE      Function used to call the appropriate layer to execute
 *              instructions for the message
 *
 * Parameters
 *     node:         node for which message is to be delivered
 *     msg:          message for which instructions are to be executed
 */
void
NODE_ProcessEvent(Node *node, Message *msg)
{
    SimContext::setCurrentNode(node);

    msg->setSent(false);

    // Debug and trace this message
    MESSAGE_DebugProcess(node->partitionData, node, msg);

    if (msg->timerManager)
    {
        msg->timerManager->scheduleNextTimer();
    }

    if (msg->cancelled)
    {
        MESSAGE_Free(node, msg);
        SimContext::unsetCurrentNode();
        return;
    }

#ifdef CYBER_LIB
    if (node->resourceManager)
    {
        node->resourceManager->timerExpired();
    }
#endif

#ifdef ADDON_DB
    if (MESSAGE_GetLayer(msg) == STATSDB_LAYER)
    {
        STATSDB_ProcessEvent(node, msg);
        SimContext::unsetCurrentNode();
        return;
    }
#endif

    if (msg->eventType != MSG_PROP_SignalReleased)
    {
        // These events are not counted to maintain consistent event
        // counts during parallel execution.
        node->partitionData->numberOfEvents++;
    }
    
    /* This statement should be updated as new layers are incorporated. */
    switch (MESSAGE_GetLayer(msg))
    {
        case PROP_LAYER:
        {
            PROP_ProcessEvent(node, msg);
            break;
        }
        case PHY_LAYER:
        {
            PHY_ProcessEvent(node, msg);
            break;
        }
        case MAC_LAYER:
        {
            MAC_ProcessEvent(node, msg);
            break;
        }
        case NETWORK_LAYER:
        {
            NETWORK_ProcessEvent(node, msg);
            break;
        }
        case TRANSPORT_LAYER:
        {
            TRANSPORT_ProcessEvent(node, msg);
            break;
        }
        case QOS_LAYER:
        {
            APP_ProcessEvent(node, msg);
            break;
        }
        case APP_LAYER:
        {
            APP_ProcessEvent(node, msg);
            break;
        }
#ifdef CELLULAR_LIB
        case USER_LAYER:
        {
            USER_ProcessEvent(node, msg);
            break;
        }
#endif // CELLULAR_LIB
        case EXTERNAL_LAYER:
        {
            EXTERNAL_ProcessEvent(node, msg);
            break;
        }
        case WEATHER_LAYER:
        {
            WEATHER_ProcessEvent(node, msg);
            break;
        }
        case ATM_LAYER2:
        {
            ATMLAYER2_ProcessEvent(node, msg);
            break;
        }
        case ADAPTATION_LAYER:
        {
            ADAPTATION_ProcessEvent(node, msg);
            break;
        }
#ifdef WIRELESS_LIB
        case BATTERY_MODEL:
        {
            BatteryProcessEvent(node, msg);
            break;
        }
#endif // WIRELESS_LIB
#if defined(SATELLITE_LIB)
        case UTIL_LAYER:
        {
            extern void UTIL_ProcessEvent(Node*, Message*);
            UTIL_ProcessEvent(node, msg);
            break;
        }
#endif /* SATELLITE_LIB */
#ifdef ADDON_BOEINGFCS
        case CONFIG_LAYER:
        {
            CONFIG_ProcessEvent(node, msg);
            break;
        }
#endif

        default:
            printf(
                "Received message for unknown layer %d.\n",
                msg->layerType);
            abort();
    }

    SimContext::unsetCurrentNode();
}


/*
 * FUNCTION     NODE_PrintLocation
 * PURPOSE      Prints the node's three dimensional coordinates.
 *
 * Parameters
 *     node:                   the node
 *     coordinateSystemType:   Cartesian or LatLonAlt
 */
void NODE_PrintLocation(Node* node,
                        int   coordinateSystemType) {
    double x,y,z;

    x = node->mobilityData->current->position.common.c1,
    y = node->mobilityData->current->position.common.c2,
    z = node->mobilityData->current->position.common.c3;

    if (coordinateSystemType == CARTESIAN)
    {
        printf("Partition %d, Node %u (%.2f, %.2f, %.2f).\n",
               node->partitionData->partitionId,
               node->nodeId, x, y, z);
    }
    else
    if (coordinateSystemType == LATLONALT)
    {
        printf("Partition %d, Node %u (%.6f, %.6f, %.6f).\n",
               node->partitionData->partitionId,
               node->nodeId, x, y, z);
    }
    fflush(stdout);
}

// /**
// API        :: NODE_GetTerrainPtr
// PURPOSE    :: Get terrainData pointer.
// PARAMETERS ::
// + node                 : Node* : the node
// RETURN :: TerrainData* : TerrainData pointer
// **/
TerrainData* NODE_GetTerrainPtr(Node* node)
{
    return PARTITION_GetTerrainPtr(node->partitionData);
}


