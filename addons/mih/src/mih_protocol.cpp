#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "mih_protocol.h"
#include "network_ip.h"
#include "transport_udp.h"
#include "qualnet_error.h"
#include "MIH_LINK_SAP.h"
#include "routing_bellmanford.h"


#define DEBUG 0

static
double ConvertIntToDouble(unsigned short x)
{
    double a = 0;
	int x_int = 0;
    double x_frac = 0;
    BOOL isNegative = FALSE;
    char intPart_reversed[16]="", conversion[16]="";
    int charCount = 0;

    while (x > 0) //Convert integer part, if any
    {
    	intPart_reversed[charCount++] = (int) (x - 2*floor((double)x/2));
    	x = floor((double)x/2);
    	//        intPart_reversed[charCount++] = (int)(2*x_int);
    	//        x_int = floor(x_int/10);
    }

    for (int i=0; i<16; i++) //Reverse the integer part, if any
          conversion[i] = intPart_reversed[15-i/*-1*/];

//    x_int = RoundToInt (x);
//    x_frac = x - (double)x_int;


    unsigned int p = 1;
    double q = 1;
    for (int j=1; j<9; j++)
    {
    	x_int += p*conversion[9-j];
    	p *= 2;
    }

    for (int k=9; k<16; k++)
    {
    	q /= 2;
    	x_frac += q*conversion[k];
    }

    a = (double) x_int + x_frac;

    if (conversion[0] == 1)
    	a = (-1)*a;

    return a;
}

static 
MAC_PROTOCOL GetProtocolForPoaId(Node* node,
									   MihData* mihf,
									   NodeAddress poaId)
{
	NbrMihData* nbrMihf;
	if (mihf->nbrMihfList->count!=0) //get info from nbr list
	{
		nbrMihf = mihf->nbrMihfList->first;
		while (nbrMihf!=NULL)
		{
			if (nbrMihf->mihfId == poaId)
				return nbrMihf->macProtocol;
			nbrMihf = nbrMihf->next;
		}
	}

	return MAC_PROTOCOL_NONE;
}
static 
MAC_PROTOCOL GetProtocolForLinkAddress(Node* node,
									   MihData* mihf,
									   LINK_ADDR linkAddr)
{
	NodeAddress poaId;
	switch (linkAddr.selector)
	{
	case 0:
		{
			Mac802Address macAddr;
			memcpy(&macAddr.byte,
					linkAddr.ma.octet_string,
					MAC_ADDRESS_LENGTH_IN_BYTE);
			MacHWAddress macHWAddr;
			Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
			int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
			poaId = MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr));
			break;
		}
	case 1:
		{
			poaId = (NodeAddress) linkAddr.g3.cid[0] / 16;
			break;
		}
	default:
		{
			poaId = 0;
			break;
		}
	}
		 
	return GetProtocolForPoaId(node, mihf, poaId);

}

MAC_PROTOCOL MihGetCurrentPoaMacProtocol(Node* node)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

//	return mihf->currentPoaIdMacProtocol;
	return mihf->targetMihfMacProtocol; //it is not the currentPoaId Mac Protocol because ???
}

/**
FUNCTION   :: CreateTransactionSrc
LAYER      :: IP
PURPOSE    :: Create an instance of a transaction source machine; called by function AddTransactionSourceToList
PARAMETERS ::
+ node      	: Node*         : pointer to node
+ srcAddress	: NodeAddress   : transaction source node IP address
+ destAddress	:NodeAddress	: transaction destination node IP address
+ transactionId	: int		: identifier of the transaction
+ isMulticast	: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq	: BOOL		: TRUE if the acknowledgment service is started
RETURN     :: TransactionSource*: Pointer to the transaction source node structure
**/
TransactionSource* CreateTransactionSrc (Node* node,
										 NodeAddress srcAddress,
										 NodeAddress destAddress,
										 int transactionId,
										 BOOL isMulticast,
										 BOOL ackReq)
{
	TransactionSource *transactionSrc =  (TransactionSource *) MEM_malloc(sizeof(TransactionSource));
	memset(transactionSrc, 0, sizeof(TransactionSource));

	transactionSrc->responseReceived = FALSE;
	transactionSrc->startAckRequestor = ((ackReq) && (!isMulticast));
	transactionSrc->startAckResponder = FALSE;
	transactionSrc->isBroadcast = FALSE;
	transactionSrc->transactionId = transactionId;
	transactionSrc->state = STATE_INIT;
	transactionSrc->ackReqStatus = NOT_ACTIVE;
	transactionSrc->ackReqState = ACK_REQ_NOT_STARTED;
	transactionSrc->ackRspState = ACK_RSP_NOT_STARTED;
	transactionSrc->ackReqNumRetransmissions = 0;
	transactionSrc->transactionStopWhen = getSimTime(node) + TRANSACTION_LIFETIME;

	transactionSrc->transactionStatus = ONGOING;

	transactionSrc->myMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, srcAddress);
	transactionSrc->peerMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, destAddress);

	transactionSrc->currentMessage = NULL;
	transactionSrc->currentRetransmissionTimer = NULL; //pointer to retransmission timer for future cancellation
	
	transactionSrc->next = NULL;

	return transactionSrc;
}

/**
FUNCTION   :: CreateTransactionDest
LAYER      :: IP
PURPOSE    :: Create an instance of a transaction destination machine; called by function AddTransactionDestToList
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ srcAddress		: NodeAddress   : transaction source node IP address
+ destAddress		: NodeAddress	: transaction destination node IP address
+ transactionId		: int		: identifier of the transaction
+ isMulticast		: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq		: BOOL		: TRUE if the acknowledgement service is started
+ transactionLifetime	:clocktype	: remaining time during which the transaction is still active
RETURN     :: TransactionDest*	: Pointer to the transaction destination node structure
**/
TransactionDest* CreateTransactionDest (Node* node,
										 NodeAddress srcAddress,
										 NodeAddress destAddress,
										 int transactionId,
										 BOOL isMulticast,
										 BOOL ackReq,
										 clocktype transactionLifetime)
{
	TransactionDest *transactionDest =  (TransactionDest *) MEM_malloc(sizeof(TransactionDest));
	memset(transactionDest, 0, sizeof(TransactionDest));

	transactionDest->startAckRequestor = FALSE;
	transactionDest->startAckResponder = ((ackReq) && (!isMulticast));
	transactionDest->isBroadcast = isMulticast;
	transactionDest->transactionId = transactionId;
	transactionDest->state = STATE_INIT;
	transactionDest->ackReqStatus = NOT_ACTIVE;
	transactionDest->ackReqState = ACK_REQ_NOT_STARTED;
	transactionDest->ackRspState = ACK_RSP_NOT_STARTED;
	transactionDest->ackReqNumRetransmissions = 0;
	transactionDest->transactionStopWhen = transactionLifetime;

	transactionDest->transactionStatus = ONGOING;

	transactionDest->myMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, srcAddress);
	transactionDest->peerMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, destAddress);

	transactionDest->currentMessage = NULL;
	transactionDest->currentRetransmissionTimer = NULL; //pointer to retransmission timer for future cancellation
	
	transactionDest->next = NULL;

	return transactionDest;
}

/**
FUNCTION   :: AddTransactionSourceToList
LAYER      :: IP
PURPOSE    :: Add an instance of a newly created transaction source machine to the list maintained at the node
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ mihf				: MihData*		: pointer to the MIHF structure
+ srcAddress		: NodeAddress   : transaction source node IP address
+ destAddress		: NodeAddress	: transaction destination node IP address
+ transactionId		: int		: identifier of the transaction
+ isMulticast		: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq		: BOOL		: TRUE if the acknowledgement service is started
RETURN     :: void
**/
void AddTransactionSourceToList (Node* node,
								 MihData* mihf,
								 NodeAddress srcAddress,
								 NodeAddress destAddress,
								 int transactionId,
								 BOOL isMulticast,
								 BOOL ackReq)
{
	TransactionSource* transactionSrc = CreateTransactionSrc(node,
															 srcAddress, 
															 destAddress, 
															 transactionId,
															 isMulticast,
															 ackReq);
	
	if (mihf->transactionSrcList == NULL)
	{
		TransactionSourceList* temp =  (TransactionSourceList *) MEM_malloc(sizeof(TransactionSourceList));
		//memset(temp, 0, sizeof(TransactionSourceDest));
		temp->first = transactionSrc;
		temp->last = transactionSrc;
		temp->count = 1;

		mihf->transactionSrcList = temp;
	}
	else
	{
		mihf->transactionSrcList->last->next = transactionSrc;
		mihf->transactionSrcList->last = transactionSrc;
		mihf->transactionSrcList->count++;
	}
}

/**
FUNCTION   :: AddTransactionDestToList
LAYER      :: IP
PURPOSE    :: Add an instance of a newly created transaction destination machine to the list maintained at the node
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ mihf			: MihData*	: pointer to the MIHF structure
+ srcAddress		: NodeAddress   : transaction source node IP address
+ destAddress		: NodeAddress	: transaction destination node IP address
+ transactionId		: int		: identifier of the transaction
+ isMulticast		: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq		: BOOL		: TRUE if the acknowledgement service is started
+ transactionLifetime	:clocktype	: remaining time during which the transaction is still active
RETURN     :: void
**/
void AddTransactionDestToList (Node* node,
							   MihData* mihf,
								 NodeAddress srcAddress,
								 NodeAddress destAddress,
								 int transactionId,
								 BOOL isMulticast,
								 BOOL ackReq,
								 clocktype transactionLifetime)
{
	TransactionDest* transactionDest = CreateTransactionDest(node, 
															 srcAddress, 
															 destAddress, 
															 transactionId,
															 isMulticast,
															 ackReq,
															 transactionLifetime);
	
	if (mihf->transactionDestList == NULL)
	{
		TransactionDestList* temp =  (TransactionDestList *) MEM_malloc(sizeof(TransactionDestList));
		//memset(temp, 0, sizeof(TransactionSourceDest));
		temp->first = transactionDest;
		temp->last = transactionDest;
		temp->count = 1;

		mihf->transactionDestList = temp;
	}
	else
	{
		mihf->transactionDestList->last->next = transactionDest;
		mihf->transactionDestList->last = transactionDest;
		mihf->transactionDestList->count++;
	}
}

/**
FUNCTION   :: RemoveTransactionSourceFromList
LAYER      :: IP
PURPOSE    :: Remove an instance of a transaction source machine from the list maintained at the node; called after a transaction has ended
PARAMETERS ::
+ node      		: Node*         	: pointer to node
+ listItem		: TransactionSource*	: pointer to the instance of the transaction source machine to be removed
RETURN     :: void
**/
void RemoveTransactionSourceFromList(Node* node,
									 TransactionSource *listItem)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionSource *item = NULL;
	TransactionSource *tempItem = NULL;
	BOOL found = FALSE;

    item = mihf->transactionSrcList->first;

	if (item == listItem) //it's the first node in the list
	{
		if (item->currentRetransmissionTimer != NULL) 
		{
			MESSAGE_CancelSelfMsg (node, item->currentRetransmissionTimer);
			item->currentRetransmissionTimer = NULL;
		}
		if (item->currentMessage != NULL)
				MESSAGE_Free(node, item->currentMessage);
		mihf->transactionSrcList->first = item->next;
		mihf->transactionSrcList->count--;
		if (mihf->transactionSrcList->first == NULL)
		{
			mihf->transactionSrcList->last = NULL;
			MEM_free(mihf->transactionSrcList);
			mihf->transactionSrcList = NULL;
		}
		MEM_free(item);
		found = TRUE;
	}
	else
	while (item!=NULL)
	{
		if (item->next == listItem)
		{
			tempItem = listItem;
			if (tempItem->currentRetransmissionTimer != NULL)
			{
				MESSAGE_CancelSelfMsg (node, tempItem->currentRetransmissionTimer);
				tempItem->currentRetransmissionTimer = NULL;
			}
			if (tempItem->currentMessage != NULL)
				MESSAGE_Free(node, tempItem->currentMessage);
			item->next=tempItem->next;
			if (item->next == NULL) //it's the last node of the list
				mihf->transactionSrcList->last = item;
			MEM_free(tempItem);
			found = TRUE;
			mihf->transactionSrcList->count--;
			break;
		}
		item = item->next;
	}

	ERROR_Assert(found == TRUE, "No matching transaction");
	

}

/**
FUNCTION   :: RemoveTransactionDestFromList
LAYER      :: IP
PURPOSE    :: Remove an instance of a transaction destination machine from the list maintained at the node; called after a transaction has ended
PARAMETERS ::
+ node      : Node*         	: pointer to node
+ listItem	: TransactionDest*	: pointer to the instance of the transaction destination machine to be removed
RETURN     :: void
**/
void RemoveTransactionDestFromList(Node* node,
								   TransactionDest *listItem)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionDest *item = NULL;
	TransactionDest *tempItem = NULL;
	BOOL found = FALSE;

    item = mihf->transactionDestList->first;

	if (item == listItem) //it's the first node in the list
	{
		if (item->currentRetransmissionTimer != NULL)
		{
			MESSAGE_CancelSelfMsg (node, item->currentRetransmissionTimer);
			item->currentRetransmissionTimer = NULL;
		}
		if (item->currentMessage != NULL)
				MESSAGE_Free(node, item->currentMessage);
		mihf->transactionDestList->first = item->next;
		mihf->transactionDestList->count--;
		if (mihf->transactionDestList->first == NULL)
		{
			mihf->transactionDestList->last = NULL;
			MEM_free(mihf->transactionDestList);
			mihf->transactionDestList = NULL;
		}
		mihf->ongoingTransactionId[item->peerMihfID] = FALSE;
		MEM_free(item);
		found = TRUE;
	}
	else
	while (item!=NULL)
	{
		if (item->next == listItem)
		{
			tempItem = listItem;
			if (tempItem->currentRetransmissionTimer != NULL)
			{
				MESSAGE_CancelSelfMsg (node, tempItem->currentRetransmissionTimer);
				tempItem->currentRetransmissionTimer = NULL;
			}
			if (tempItem->currentMessage != NULL)
				MESSAGE_Free(node, tempItem->currentMessage);
			item->next=tempItem->next;
			if (item->next == NULL) //it's the last node of the list
				mihf->transactionDestList->last = item;
			mihf->ongoingTransactionId[tempItem->peerMihfID] = FALSE;
			MEM_free(tempItem);
			found = TRUE;
			mihf->transactionDestList->count--;
			break;
		}
		item = item->next;
	}

	ERROR_Assert(found == TRUE, "No matching transaction");
	
}

/**
FUNCTION   :: FreeTransactionSrcList
LAYER      :: IP
PURPOSE    :: Remove transaction source machine list maintained at the node; called from MIH finalization function
PARAMETERS ::
+ node      		: Node*         	: pointer to node

RETURN     :: void
**/



void FreeTransactionSrcList(Node *node/*, BOOL isMsg*/)
{
    NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionSource *tempItem;
    TransactionSource *item;

    item = mihf->transactionSrcList->first;

    while (item)
    {
        tempItem = item;
        item = item->next;
        MEM_free(tempItem);
    }

    if (mihf->transactionSrcList != NULL)
    {
        MEM_free(mihf->transactionSrcList);
    }

    mihf->transactionSrcList = NULL;
}

/**
FUNCTION   :: FreeTransactionDestList
LAYER      :: IP
PURPOSE    :: Remove transaction destination machine list maintained at the node; called from MIH finalization function
PARAMETERS ::
+ node      		: Node*         	: pointer to node

RETURN     :: void
**/

void FreeTransactionDestList(Node *node/*, BOOL isMsg*/)
{
    NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionDest *tempItem;
    TransactionDest *item;

    item = mihf->transactionDestList->first;

    while (item)
    {
        tempItem = item;
        item = item->next;
/* //Here have to free message pointers now
        if (isMsg == FALSE)
        {
            MEM_free(tempItem->data);
        }
        else
        {
            MESSAGE_Free(node, (Message *) tempItem->data);
        }
*/
		
        MEM_free(tempItem);
    }

    if (mihf->transactionDestList != NULL)
    {
        MEM_free(mihf->transactionDestList);
    }

    mihf->transactionDestList = NULL;
}

/**
FUNCTION   :: MihSendPacketToIp
LAYER      :: IP
PURPOSE    :: Send MIH message to IP layer
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ mihMsg		: Message*	: pointer to the MIH Messaged being sent
+ sourceAddress		: NodeAddress   : MIH message source node IP address
+ destinationAddress	: NodeAddress	: MIH message destination node IP address
+ outgoingInterface	: int		: interface on which message is being sent
+ ttl			: int		: Time-To-Live
RETURN     :: void
**/


void MihSendPacketToIp(
    Node* node,
    Message* mihMsg,
    NodeAddress sourceAddress,
    NodeAddress destinationAddress,
    int outgoingInterface,
    int ttl)
{
    int priority = IPTOS_PREC_INTERNETCONTROL;
    unsigned char protocol = IPPROTO_MIH;

    // this is for adding IpHeader & Routing packet
    NetworkIpSendRawMessage(
        node,
        mihMsg,
        sourceAddress,
        destinationAddress,
        outgoingInterface,
        priority,
        protocol,
        ttl);
}

/**
FUNCTION   :: TransactionSrcInitTransaction
LAYER      :: IP
PURPOSE    :: Handle a new transaction originating at the source node
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ mihMsg		: Message*	: pointer to the MIH message corresponding to the transaction being initiated
+ srcAddress		: NodeAddress   : transaction source node IP address
+ destAddress		: NodeAddress	: transaction destination node IP address
+ transactionId		: int		: identifier of the transaction
+ isMulticast		: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq		: BOOL		: TRUE if the acknowledgement service has to be started
RETURN     :: void
**/

void TransactionSrcInitTransaction(Node* node,
				   Message* mihMsg,
				   NodeAddress srcAddress,
				   NodeAddress destAddress,
				   int transactionId,
				   BOOL isMulticast,
				   BOOL ackReq)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = ipLayer->mihFunctionStruct;

	AddTransactionSourceToList (node,
								mihf,
								 srcAddress,
								 destAddress,
								 transactionId,
								 isMulticast,
								 ackReq);
	TransactionSource* transactionSrc = mihf->transactionSrcList->last; //we added the source machine at the tail


	//schedule transaction lifetime msg
	Message *timerMsg;

	timerMsg = MESSAGE_Alloc(node, 
								NETWORK_LAYER, 
								MIH_PROTOCOL, 
								MSG_MIH_TransactionLifetimeAlarm); //Have to define this in messages/events enum
	
	MESSAGE_InfoAlloc(node, timerMsg, sizeof(TransactionLifetimeTimerInfo));
	TransactionLifetimeTimerInfo *timerInfo = (TransactionLifetimeTimerInfo *) MESSAGE_ReturnInfo(timerMsg);
	timerInfo->transactionId = transactionId;
	timerInfo->srcAddress = srcAddress;
	timerInfo->destAddress = destAddress;

	clocktype currentTime = getSimTime(node);
	clocktype delay = transactionSrc->transactionStopWhen - currentTime;
	if (delay>0)
		MESSAGE_Send(node, timerMsg, delay);

	if (transactionSrc->startAckRequestor)
	{
		//save a copy of the message
		transactionSrc->currentMessage = MESSAGE_Duplicate(node, mihMsg);
			//schedule timer message:
		Message *msg;
			
		msg = MESSAGE_Alloc(node, 
								NETWORK_LAYER, 
								MIH_PROTOCOL, 
								MSG_MIH_RetransmissionTimerAlarm); //Have to define this in messages/events enum
	
		MESSAGE_InfoAlloc(node, msg, sizeof(RetransmissionTimerInfo));
		RetransmissionTimerInfo *timerInfo = (RetransmissionTimerInfo *) MESSAGE_ReturnInfo(msg);
		timerInfo->transactionId = transactionId;
		timerInfo->srcAddress = srcAddress;
		timerInfo->destAddress = destAddress;

		transactionSrc->currentRetransmissionTimer = MESSAGE_Duplicate(node, msg);
	
		MESSAGE_Send(node, msg, RETRANSMISSION_INTERVAL); // or RetransmissionInterval

		transactionSrc->ackReqNumRetransmissions = 0;
		transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		mihf->stats->numRetransmissionTimerAlarmsSent++;
	}

}

/**
FUNCTION   :: TransactionDestInitTransaction
LAYER      :: IP
PURPOSE    :: Handle a new transaction originating at the destination node
PARAMETERS ::
+ node      		: Node*         : pointer to node
+ mihMsg		: Message*	: pointer to the MIH message corresponding to the transaction being initiated
+ srcAddress		: NodeAddress   : transaction source node IP address
+ destAddress		: NodeAddress	: transaction destination node IP address
+ transactionId		: int		: identifier of the transaction
+ isMulticast		: BOOL		: TRUE if the message sent by the source node is multicast
+ ackReq		: BOOL		: TRUE if the acknowledgement service has to be started
+ transactionLifetime	: clocktype	: time remaining until current transation expires
RETURN     :: void
**/
void TransactionDestInitTransaction(Node* node,
								   Message* mihMsg,
								   NodeAddress srcAddress,
								   NodeAddress destAddress,
								   int transactionId,
								   BOOL isMulticast,
								   BOOL ackReq,
								   clocktype transactionLifetime)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = ipLayer->mihFunctionStruct;

	AddTransactionDestToList (node,
								mihf,
								 srcAddress,
								 destAddress,
								 transactionId,
								 isMulticast,
								 ackReq,
								 transactionLifetime);
	TransactionDest* transactionDest = mihf->transactionDestList->last; //we added the source machine at the tail


	//schedule transaction lifetime msg
	Message *timerMsg;

	timerMsg = MESSAGE_Alloc(node, 
								NETWORK_LAYER, 
								MIH_PROTOCOL, 
								MSG_MIH_TransactionLifetimeAlarm); //Have to define this in messages/events enum
	
	MESSAGE_InfoAlloc(node, timerMsg, sizeof(TransactionLifetimeTimerInfo));
	TransactionLifetimeTimerInfo *timerInfo = (TransactionLifetimeTimerInfo *) MESSAGE_ReturnInfo(timerMsg);
	timerInfo->transactionId = transactionId;
	timerInfo->srcAddress = srcAddress;
	timerInfo->destAddress = destAddress;

	clocktype currentTime = getSimTime(node);
	clocktype delay = transactionDest->transactionStopWhen - currentTime;
	if (delay > 0)
		MESSAGE_Send(node, timerMsg, delay);

	if (transactionDest->startAckRequestor)
	{
		//save a copy of the message
		transactionDest->currentMessage = MESSAGE_Duplicate(node, mihMsg);
			//schedule timer message:
		Message *msg;
			
		msg = MESSAGE_Alloc(node, 
								NETWORK_LAYER, 
								MIH_PROTOCOL, 
								MSG_MIH_RetransmissionTimerAlarm); //Have to define this in messages/events enum
	
		MESSAGE_InfoAlloc(node, msg, sizeof(RetransmissionTimerInfo));
		RetransmissionTimerInfo *timerInfo = (RetransmissionTimerInfo *) MESSAGE_ReturnInfo(msg);
		timerInfo->transactionId = transactionId;
		timerInfo->srcAddress = srcAddress;
		timerInfo->destAddress = destAddress;

		transactionDest->currentRetransmissionTimer = MESSAGE_Duplicate(node, msg);
	
		MESSAGE_Send(node, msg, RETRANSMISSION_INTERVAL); // or RetransmissionInterval

		transactionDest->ackReqNumRetransmissions = 0;
		transactionDest->ackReqState = ACK_REQ_WAIT_ACK;
		mihf->stats->numRetransmissionTimerAlarmsSent++;
	}
}

/**
FUNCTION   :: CreateRemoteMihMessage
LAYER      :: IP
PURPOSE    :: Sends messages between peer MIHFs
PARAMETERS ::
+ node      			: Node*         	: pointer to node
+ mihf					: MihData*			: pointer to the MIHF structure
+ destAddress			: NodeAddress		: destination node IP address
+ payload				: void*				: pointer to the payload
+ payloadSize			: int				: payload size
+ serviceId				: ServiceIdentifier	: service identifier field
+ opCode				: OperationCode		: operation code field
+ actionId				: ActionIdentifier	: action identifier field
+ transactionId			: int				: identifier of the transaction
+ transactionInitiated	: clocktype			: time when the message is created	
+ isMulticast			: BOOL				: TRUE if the message sent by the source node is multicast
+ ackReq				: BOOL				: TRUE if the acknowledgement service has to be started
+ ackRsp				: BOOL				: TRUE if this message carries the ack-rsp bit
RETURN     :: void
**/

Message* CreateRemoteMihMessage(Node* node,
								   MihData* mihf,
								   NodeAddress destAddr, 
								   char* payload,
								   int payloadSize,
								   ServiceIdentifier serviceId,
								   OperationCode opCode,
								   ActionIdentifier actionId,
								   int transactionId,
								   clocktype transactionInitiated,
								   BOOL ackReq,
								   BOOL ackRsp)
{
    NodeAddress sourceAddr;
    Message* mihMsg;
	MihHeaderType* mihPkt;
    int interfaceIndex;
	BOOL found = FALSE;
		
	mihMsg = MESSAGE_Alloc(node,
                NETWORK_LAYER,
                NETWORK_PROTOCOL_IP, 
                MSG_NETWORK_FromTransportOrRoutingProtocol);


    MESSAGE_PacketAlloc(node,
                        mihMsg,
                        sizeof(MihHeaderType),
                        TRACE_MIH);

    mihPkt = (MihHeaderType *) MESSAGE_ReturnPacket(mihMsg);

    interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex); 

	mihPkt->ackReq = ackReq;
	mihPkt->ackRsp = ackRsp;
	mihPkt->sid = serviceId;
    mihPkt->aid = actionId;
	mihPkt->opcode = opCode;
	mihPkt->transactionId = transactionId; 
	mihPkt->timeSent = getSimTime(node);
	mihPkt->transactionInitiated = transactionInitiated;
	mihPkt->payloadSize = payloadSize;
	mihPkt->srcAddr = sourceAddr;
	mihPkt->destAddr = destAddr;
	mihPkt->payload = payload;
	mihPkt->payloadSize = payloadSize;
	
	
	//dummy udp header
    MESSAGE_AddHeader(node,
                      mihMsg,
                      sizeof(TransportUdpHeader),
                      TRACE_MIH);

	return mihMsg;

}




void SendRemoteMihLinkGetParametersRequest(Node *node,
									 MihData* mihf,
									 NodeAddress destAddr,
									 NodeAddress srcMihfID,
									 int transactionId,
									 UNSIGNED_INT_1 paramType[]//and other arguments
									 )
{
	NodeAddress sourceAddr;
	LINK_STATUS_REQ *getStatusReqSet =  (LINK_STATUS_REQ *) MEM_malloc(sizeof(LINK_STATUS_REQ));
	memset(getStatusReqSet, 0, sizeof(LINK_STATUS_REQ));

	getStatusReqSet->ldr = 0; //we want the number of classes of service supported
	getStatusReqSet->lsr = 0; //we want the op_mode


	
	L_LINK_PARAM_TYPE *LinkParametersRequest =  (L_LINK_PARAM_TYPE *) MEM_malloc(sizeof(L_LINK_PARAM_TYPE));
	memset(LinkParametersRequest, 0, sizeof(L_LINK_PARAM_TYPE));
	LINK_PARAM_TYPE *elem;
	//there should be a cycle here, just one for now
	for (int i =0; i<(sizeof(paramType)/sizeof(int)); i++)
	{
		if (LinkParametersRequest->elem == NULL)
		{
			LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
			memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
			ParamTypeValue value;
			value.lp_gen = paramType[i];
			paramTypeStr->value = value;
			paramTypeStr->selector = 1;
			paramTypeStr->next = NULL;
		
			LinkParametersRequest->length++;
			LinkParametersRequest->elem = paramTypeStr;
			elem = LinkParametersRequest->elem;
		}
		else
		{

			LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
			memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
			ParamTypeValue value;
			value.lp_gen = paramType[i];
			paramTypeStr->value = value;
			paramTypeStr->selector = 1;
			paramTypeStr->next = NULL;
			LinkParametersRequest->length++;
			elem->next = paramTypeStr;
			elem = elem->next;
		}
	}

	getStatusReqSet->lpt = LinkParametersRequest;
	link_parameters *param_req =  (link_parameters *) MEM_malloc(sizeof(link_parameters));
	memset(param_req, 0, sizeof(link_parameters));

	param_req->statusReq = getStatusReqSet;
	param_req->destMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, destAddr); //to revise //dest PoA address
	param_req->srcMihfID = srcMihfID; //mobile node address
	param_req->transactionId = transactionId;
	param_req->transactionInitiated = getSimTime(node); //have to modify

	//NodeAddress destAddr= MAPPING_GetDefaultInterfaceAddressFromNodeId(node, destMihfID);
	char *buff = new char[sizeof(link_parameters)];
	*((LINK_STATUS_REQ **)buff) = param_req->statusReq;
	*((NodeAddress *)(buff + sizeof(LINK_STATUS_REQ *))) = param_req->srcMihfID;
	*((NodeAddress *)(buff + sizeof(LINK_STATUS_REQ *) + sizeof(NodeAddress))) = param_req->destMihfID;
	*((int *)(buff + sizeof(LINK_STATUS_REQ *) + 2*sizeof(NodeAddress))) = param_req->transactionId;
	*((clocktype *)(buff + sizeof(LINK_STATUS_REQ *) + 2*sizeof(NodeAddress) + sizeof(int))) = param_req->transactionInitiated;


	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;

	BOOL ackReq = TRUE;
	//BOOL ackReq = FALSE;
	BOOL ackRsp = FALSE;

	Message* mihMsg = CreateRemoteMihMessage(node,
								   mihf,
								   destAddr, 
								   buff,
								   sizeof(link_parameters),
								   COMMAND_SERVICE,
								   REQUEST,
								   MIH_LINK_GET_PARAMETERS,
								   transactionId,
								   getSimTime(node),
								   ackReq,
								   ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	TransactionSrcInitTransaction(node,
								   mihMsg,
								   sourceAddr,
								   destAddr,
								   transactionId,
								   isMulticast,
								   ackReq);
	
	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);

	TransactionSource* transactionSrc = mihf->transactionSrcList->last;

	if (ackReq == FALSE)
		transactionSrc->transactionStatus = SUCCESS;
	else
		{
			transactionSrc->ackReqStatus = ONGOING;
			transactionSrc->state = STATE_WAIT_ACK;
			transactionSrc->ackReqNumRetransmissions = 0;
			transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		}
		
	mihf->stats->numMihLinkGetParamReqSent++;
}
void ReceiveMihMnHoCommitRequest(Node* node,
				Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    	MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	//Do not know the structure of the message that will be received
	Message* newMsg = NULL;
    Address* info = NULL;
	NodeAddress destAddr; //for compilation purpose
	MnHoCommitReqInfo *reqInfo = (MnHoCommitReqInfo *) MESSAGE_ReturnInfo(msg);
	//Have to extract parameters needed
	//Interested in the target network parameters, especially address
	LINK_TYPE lt = 0; //= something from the message
	TGT_NET_INFO* tgtNetInfo = NULL; //= something from the message
	switch (tgtNetInfo->selector)
	{
		case 0: //network Id
		{
		break;
		}
		case 1: //network Aux Id
		{
		break;
		}
		case 2: //link address
		{
		break;
		}
	}
	
	//allocate message for routing protocol
	
	newMsg = MESSAGE_Alloc(
                 node,
                 NETWORK_LAYER,
                 ROUTING_PROTOCOL_AODV,
                 MSG_NETWORK_CheckRouteTimeout);
                 
        MESSAGE_InfoAlloc(
			node,
			newMsg,
			sizeof(Address));
			
	info = (Address *) MESSAGE_ReturnInfo(newMsg);

    	memcpy(info, &destAddr, sizeof(Address));

    	// Schedule the timer after the specified delay
    	MESSAGE_Send(node, newMsg, 0);
}
void SendRemoteMihMnHoCommitRequest(Node *node,
									 MihData* mihf,
									 NodeAddress destAddr,
									 MnHoCommitReqInfo *mnHoCommitReq,
									 int transactionId
									 //and other arguments
									 )
{
	NodeAddress sourceAddr;
	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;

	BOOL ackReq = TRUE;
	BOOL ackRsp = FALSE;

	Message* mihMsg = CreateRemoteMihMessage(node,
								   mihf,
								   destAddr, 
								   /*(MnHoCommitReqInfo *) mnHoCommitReq*/0,
								   sizeof(link_parameters),
								   COMMAND_SERVICE,
								   REQUEST,
								   MIH_MN_HO_COMMIT,
								   transactionId,
								   getSimTime(node),
								   ackReq,
								   ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	TransactionSrcInitTransaction(node,
								   mihMsg,
								   sourceAddr,
								   destAddr,
								   transactionId,
								   isMulticast,
								   ackReq);
	
	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);

	TransactionSource* transactionSrc = mihf->transactionSrcList->last;

	if (ackReq == FALSE)
		transactionSrc->transactionStatus = SUCCESS;
	else
		{
			transactionSrc->ackReqStatus = ONGOING;
			transactionSrc->state = STATE_WAIT_ACK;
			transactionSrc->ackReqNumRetransmissions = 0;
			transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		}
		
	mihf->stats->numMihMnHoCommitReqSent++;
}
void RMSendMihMnHoCommitRequest (Node* node,
							   MihData* mihf,
							   NodeAddress tgtMihfId
							   //maybe some more params
							   )
{
	NbrMihData* nbrMihf;
	if (mihf->nbrMihfList->count!=0) //get info from nbr list
	{
		nbrMihf = mihf->nbrMihfList->first;
		while (nbrMihf!=NULL)
		{
			if (nbrMihf->mihfId == tgtMihfId)
				break;
			nbrMihf = nbrMihf->next;
		}
	}
	//maybe some error warning here
	switch (nbrMihf->macProtocol)
	{
	case MAC_PROTOCOL_DOT16:
		{
			UNSIGNED_INT_1 linkType = 27;
			break;
		}
	case MAC_PROTOCOL_DOT11:
		{
			UNSIGNED_INT_1 linkType = 19;
			break;
		}
	case MAC_PROTOCOL_GSM:
		{
			UNSIGNED_INT_1 linkType = 1;
			break;
		}
	case MAC_PROTOCOL_CELLULAR:
		{
			UNSIGNED_INT_1 linkType = 23;
			break;
		}

		//and more to be added
	case MAC_PROTOCOL_802_15_4:
	default:
		{
			UNSIGNED_INT_1 linkType = 18; //Wireless - other
			break;
		}
	};

	//build target network info

	TGT_NET_INFO *tgtNetInfo =  (TGT_NET_INFO *) MEM_malloc(sizeof(TGT_NET_INFO));
	memset(tgtNetInfo, 0, sizeof(TGT_NET_INFO));

	tgtNetInfo->selector = 2; //this means LINK_ADDR field from TgtNetInfoValue union
//need further revisiting
	MAC_ADDR macAddress;// = nbrMihf->interfaceAddress;

	LINK_ADDR linkAddress;
	linkAddress.selector = 0; //this means link address is of type (48 bit) mac address
	linkAddress.ma = macAddress;

	tgtNetInfo->value.lad =  linkAddress;
	tgtNetInfo->next = NULL;

	/* there is no such thing towards lower layers (mac)
	Mih_Mn_Ho_Commit_request (node,
							   NodeAddress destMihfId, //?
							   linkType,
							   tgtNetInfo);

*/ 
}

void MihHandleLinkGoingDownTimerAlarm (Node* node,
		Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
	MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	down_reason *timerInfo = (down_reason *) MESSAGE_ReturnInfo(msg);

	//RMTriggerHandover(node, mihf, timerInfo->srcMihfID);
}

void SendRemoteMihLinkGetParametersResponse(Node *node,
									 MihData* mihf,
									 link_params* confirm
									 //and other arguments
									 )
{

	NodeAddress sourceAddr;

	NodeAddress destAddr= MAPPING_GetDefaultInterfaceAddressFromNodeId(node, mihf->resourceManagerId);
	int transactionId = confirm->transactionId;
	//NodeAddress peerMihfID = confirm->srcMihfID;
	int transactionInitiated = confirm->transactionInitiated;

	char *buff = new char[sizeof(link_params)];
	*((STATUS *)buff) = confirm->status;
	*((l_LINK_PARAM **)(buff + sizeof(STATUS))) = confirm->linkParamStatusList;
	*((l_LINK_STATES_RSP **)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *))) = confirm->linkStatesRsp;
	*((l_LINK_DESC_RSP **)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *))) = confirm->linkDescRsp;
	*((NodeAddress *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *))) = confirm->destMihfID;
	*((NodeAddress *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + sizeof(NodeAddress))) = confirm->srcMihfID;
	*((int *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + 2*sizeof(NodeAddress))) = confirm->transactionId;
	*((clocktype *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + 2*sizeof(NodeAddress)+ sizeof(int))) = confirm->transactionInitiated;



	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;

	BOOL ackReq = FALSE;
	BOOL ackRsp = FALSE;

	TransactionDest *transactionDest = mihf->transactionDestList->first;

	while (transactionDest != NULL)
	{
		if ((transactionDest->transactionId == transactionId) &&
	(transactionDest->myMihfID == node->nodeId) &&
	(transactionDest->peerMihfID == mihf->resourceManagerId))
			break;
		transactionDest = transactionDest->next;
	}

	ERROR_Assert(transactionDest != NULL, "No matching transaction");

	if (transactionDest->startAckResponder == TRUE)
	{
		ackReq = FALSE;
		ackRsp = TRUE;
		transactionDest->ackRspState = PIGGYBACKING;
	}
	
	Message* mihMsg = CreateRemoteMihMessage(node,
								   mihf,
								   destAddr, 
								   buff,
								   sizeof(link_params),
								   COMMAND_SERVICE,
								   RESPONSE,
								   MIH_LINK_GET_PARAMETERS,
								   transactionId,
								   transactionInitiated,
								   ackReq,
								   ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);
//maybe there should be an if here
	transactionDest->state = SEND_RESPONSE;
	transactionDest->startAckRequestor = ackReq;
	transactionDest->ackReqStatus=ONGOING;

	mihf->stats->numMihLinkGetParamRspSent++;
}

void SendRemoteMihLinkActionsRequest(Node* node,
									 MihData* mihf,
									 NodeAddress destMihfID,
									 int transactionId,
									 L_LINK_ACTION_REQ* linkActionsList)
{
	NodeAddress destAddr= MAPPING_GetDefaultInterfaceAddressFromNodeId(node, destMihfID);

	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;

	BOOL ackReq = TRUE;
	BOOL ackRsp = FALSE;

	char *buff = new char[sizeof(L_LINK_ACTION_REQ)];
	*((L_LINK_ACTION_REQ**)buff) = linkActionsList;

	Message* mihMsg = CreateRemoteMihMessage(node,
								   mihf,
								   destAddr, 
								   buff,
								   sizeof(link_parameters),
								   COMMAND_SERVICE,
								   REQUEST,
								   MIH_LINK_ACTIONS,
								   transactionId,
								   getSimTime(node),
								   ackReq,
								   ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    NodeAddress sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	TransactionSrcInitTransaction(node,
								   mihMsg,
								   sourceAddr,
								   destAddr,
								   transactionId,
								   isMulticast,
								   ackReq);
	
	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);

	TransactionSource* transactionSrc = mihf->transactionSrcList->last;

	if (ackReq == FALSE)
		transactionSrc->transactionStatus = SUCCESS;
	else
		{
			transactionSrc->ackReqStatus = ONGOING;
			transactionSrc->state = STATE_WAIT_ACK;
			transactionSrc->ackReqNumRetransmissions = 0;
			transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		}
		
	mihf->stats->numMihLinkActionsReqSent++;
}


void SendRemoteMihLinkActionsResponse(Node* node,
									  MihData* mihf,
									  NodeAddress destMihfID,
									  int transactionId,
									  clocktype transactionInitiated,
									  STATUS status,
									  L_LINK_ACTION_RSP* linkActionsResultList
									  //and possibly other arguments
									  )
{
	NodeAddress destAddr= MAPPING_GetDefaultInterfaceAddressFromNodeId(node, destMihfID);

	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;

	BOOL ackReq = TRUE;
	BOOL ackRsp = FALSE;

	TransactionDest *transactionDest = mihf->transactionDestList->first;

	while (transactionDest != NULL)
	{
		if ((transactionDest->transactionId == transactionId) &&
	(transactionDest->myMihfID == node->nodeId) &&
	(transactionDest->peerMihfID == destMihfID))
			break;
		transactionDest = transactionDest->next;
	}

	ERROR_Assert(transactionDest != NULL, "No matching transaction");

	if (transactionDest->startAckResponder == TRUE)
	{
		ackReq = FALSE;
		ackRsp = TRUE;
		transactionDest->ackRspState = PIGGYBACKING;
	};

	MihLinkActionsRsp* mihLinkActRsp = (MihLinkActionsRsp *) MEM_malloc(sizeof(MihLinkActionsRsp));
	memset(mihLinkActRsp, 0, sizeof(MihLinkActionsRsp));

	mihLinkActRsp->status = status;
	mihLinkActRsp->linkActionsResultList = linkActionsResultList;

	char *buff = new char[sizeof(MihLinkActionsRsp)];
	*((STATUS *)buff) = mihLinkActRsp->status;
	*((L_LINK_ACTION_RSP**) (buff + sizeof(STATUS))) = mihLinkActRsp->linkActionsResultList;


	
	Message* mihMsg = CreateRemoteMihMessage(node,
								   mihf,
								   destAddr, 
								   buff,
								   sizeof(link_parameters),
								   COMMAND_SERVICE,
								   RESPONSE,
								   MIH_LINK_ACTIONS,
								   transactionId,
								   transactionInitiated,
								   ackReq,
								   ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    NodeAddress sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);
//maybe there should be an if here
	transactionDest->state = SEND_RESPONSE;
	transactionDest->startAckRequestor = ackReq;
	transactionDest->ackReqStatus=ONGOING;

	mihf->stats->numMihLinkActionsRspSent++;
}

void ReceiveLinkParametersReportIndication(Node* node,
										   Message* msg)
{
	int ttl = 1;
	int interfaceIndex;
	param_signal *param = (param_signal *) MESSAGE_ReturnInfo(msg);
	
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	mihf->stats->numLinkParametersReportIndReceived++;

	//NodeAddress destAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId (node, mihf->resourceManagerId);
	if (mihf->resourceManagerId == node->nodeId)
	{
		NodeAddress poaId;
		switch (param->linkId->lid.lt)
		{
		case 19:
		case 27:
		{
			//IEEE 802.11 and 802.16
			if (param->linkId->lid.la.ma.addr_type == 6) //6 - 802 address // should be true by default, doesn't hurt though to check
			{
				Mac802Address macAddr;
				memcpy(&macAddr.byte,
						param->linkId->lid.la.ma.octet_string,
						MAC_ADDRESS_LENGTH_IN_BYTE);
				MacHWAddress macHWAddr;
				Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
				int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
				poaId = MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr));
			}
			break;
		}
		case 23:
		{
			//UMTS
			poaId = (NodeAddress) (param->linkId->lid.la.g3.cid[0] / 16);
			break;
		}
		
		}
		RMProcessLinkParamReport(node,
							  mihf,
							  param->linkParamRptList,
							  param->dest,
							  poaId);
	}
	else
	{
		NodeAddress sourceAddr;

		BOOL isMulticast = FALSE;
		BOOL ackReq = TRUE;
		BOOL ackRsp = FALSE;

		int transactionId = mihf->transactionId++;

		switch(param->linkId->lid.lt)
		{
			case 19: //this is wi-fi
				{
				for (int i = 0; i < node->numberInterfaces; i++)
				{
					if(mihf->macProtocol[i] == MAC_PROTOCOL_DOT11)
						interfaceIndex = i;
				}
				break;
				}
			case 27:
				{
					for (int i = 0; i < node->numberInterfaces; i++)
				{
					if(mihf->macProtocol[i] == MAC_PROTOCOL_DOT16)
						interfaceIndex = i;
				}
				break;
				}
			case 23:
				{
					for (int i = 0; i < node->numberInterfaces; i++)
				{
					if(mihf->macProtocol[i] == MAC_PROTOCOL_CELLULAR)
						interfaceIndex = i;
				}
				break;
				}
			default:
				{
					printf("MIH Function: Node %u does not have link type %d. Switching to default interface\n",
				   node->nodeId, param->linkId->lid.lt);
					interfaceIndex = 0;
					break;
				}
		}


		NodeAddress destAddr = MAPPING_GetInterfaceAddressForInterface (node, mihf->resourceManagerId, interfaceIndex);


	//forward LinkParamReportInd to peer Mihf

		char *buff = new char[sizeof(param_signal)];
		*((NodeAddress*) (buff)) = param->dest;
		*((L_LINK_PARAM_RPT**) (buff+sizeof(NodeAddress))) = param->linkParamRptList;
		*((LINK_TUPLE_ID**) (buff + sizeof(NodeAddress) + sizeof(L_LINK_PARAM_RPT*))) = param->linkId;

		for (int j=0; j<5; j++)
			((unsigned char *) (buff + sizeof(L_LINK_PARAM_RPT*)+ sizeof(LINK_TUPLE_ID*)+sizeof(NodeAddress)))[j] = param->poaId[j];


		Message* mihMsg = CreateRemoteMihMessage(node,
												mihf,
												destAddr, 
												buff,
												sizeof(param_signal),
												EVENT_SERVICE,
												INDICATION,
												MIH_LINK_PARAMETERS_REPORT,
												transactionId, //if it is a new transaction, will use mihf->transactionId++
												getSimTime(node),
												ackReq,
												ackRsp);
	/*
		int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
					node,
					destAddr);
	*/
		sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

		TransactionSrcInitTransaction(node,
									   mihMsg,
									   sourceAddr,
									   destAddr,
									   transactionId,
									   isMulticast,
									   ackReq);

		MihSendPacketToIp(
			node,
			mihMsg,
			sourceAddr,
			destAddr, 
			interfaceIndex,
			ttl);
		mihf->stats->numMihLinkParametersReportIndSent++;
	}

}

void SendRemoteMihLinkGoingDownIndication (Node* node,
									 MihData* mihf,
									 down_reason* reason,
									 NodeAddress destAddr)
{
	NodeAddress sourceAddr;
	BOOL ackReq = TRUE;
	//BOOL ackReq = FALSE;
	BOOL ackRsp = FALSE;
	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;
	int transactionId = mihf->transactionId++;

	char *buff = new char[sizeof(down_reason)];
	*((LINK_TUPLE_ID**) buff) = reason->linkId;
	*((UNSIGNED_INT_2 *) (buff + sizeof(LINK_TUPLE_ID*))) = reason->timeInt;
	*((LINK_GD_REASON *)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(UNSIGNED_INT_2))) = reason->lgdReason;
	*((NodeAddress *)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(UNSIGNED_INT_2) + sizeof(LINK_GD_REASON))) = reason->srcMihfID;

	
	
	Message* mihMsg = CreateRemoteMihMessage(node,
													mihf,
													destAddr, 
													buff,
													sizeof(down_reason),
													EVENT_SERVICE,
													INDICATION,
													MIH_LINK_GOING_DOWN,
													transactionId, 
													getSimTime(node),
													ackReq,
													ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	TransactionSrcInitTransaction(node,
								   mihMsg,
								   sourceAddr,
								   destAddr,
								   transactionId,
								   isMulticast,
								   ackReq);
	
	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);

	TransactionSource* transactionSrc = mihf->transactionSrcList->last;

	if (ackReq == FALSE)
		transactionSrc->transactionStatus = SUCCESS;
	else
		{
			transactionSrc->ackReqStatus = ONGOING;
			transactionSrc->state = STATE_WAIT_ACK;
			transactionSrc->ackReqNumRetransmissions = 0;
			transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		}
		
	mihf->stats->numMihLinkGoingDownIndSent++;
}

void SendRemoteMihLinkUpIndication (Node* node,
									 MihData* mihf,
									 link_up_str* up,
									 NodeAddress destAddr)
{
	NodeAddress sourceAddr;
	BOOL ackReq = TRUE;
	//BOOL ackReq = FALSE;
	BOOL ackRsp = FALSE;
	BOOL isMulticast = FALSE;
	if (destAddr == ANY_DEST)
		isMulticast = TRUE;
	int transactionId = mihf->transactionId++;

	char *buff = new char[sizeof(link_up_str)];
	*((LINK_TUPLE_ID**) buff) = up->LinkIdentifier;
	*((LINK_ADDR**) (buff + sizeof(LINK_TUPLE_ID*))) = up->OldAccessRouter;
	*((LINK_ADDR**)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(LINK_ADDR*))) = up->NewAccessRouter;
	*((IP_RENEWAL_FLAG *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*))) = up->IpRenewalFlag;
	*((IP_MOB_MGMT *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*) + sizeof(IP_RENEWAL_FLAG))) = up->MobilityManagementSupport;
	*((NodeAddress *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*) + sizeof(IP_RENEWAL_FLAG) + sizeof(IP_MOB_MGMT))) = up->dest;

	
	
	Message* mihMsg = CreateRemoteMihMessage(node,
													mihf,
													destAddr, 
													buff,
													sizeof(link_up_str),
													EVENT_SERVICE,
													INDICATION,
													MIH_LINK_UP,
													transactionId, 
													getSimTime(node),
													ackReq,
													ackRsp);

	int interfaceIndex = NetworkGetInterfaceIndexForDestAddress(
                node,
                destAddr);

    sourceAddr = NetworkIpGetInterfaceAddress(node, interfaceIndex);

	TransactionSrcInitTransaction(node,
								   mihMsg,
								   sourceAddr,
								   destAddr,
								   transactionId,
								   isMulticast,
								   ackReq);
	
	MihSendPacketToIp(
        node,
        mihMsg,
        sourceAddr,
        destAddr, 
        ANY_INTERFACE,
        IPDEFTTL);

	TransactionSource* transactionSrc = mihf->transactionSrcList->last;

	if (ackReq == FALSE)
		transactionSrc->transactionStatus = SUCCESS;
	else
		{
			transactionSrc->ackReqStatus = ONGOING;
			transactionSrc->state = STATE_WAIT_ACK;
			transactionSrc->ackReqNumRetransmissions = 0;
			transactionSrc->ackReqState = ACK_REQ_WAIT_ACK;
		}
		
	mihf->stats->numMihLinkUpIndSent++;
}

void RMProcessLinkGoingDownInd (Node* node,
									MihData* mihf,
									down_reason* reason,
									clocktype timeSent,
									NodeAddress srcMihfID)
{			
	//mihf->numLinkGetParamsSent = 0;
	UNSIGNED_INT_1 paramTypeReq[1] = {2};
	/*
	if (node->nodeId != reason->srcMihfID) //the RM is not the current PoA
	{
		LINK_STATUS_REQ *getStatusReqSet =  (LINK_STATUS_REQ *) MEM_malloc(sizeof(LINK_STATUS_REQ));
		memset(getStatusReqSet, 0, sizeof(LINK_STATUS_REQ));

		getStatusReqSet->ldr = 0; //we want the number of classes of service supported
		getStatusReqSet->lsr = 0; //we want the op_mode


		
		L_LINK_PARAM_TYPE *LinkParametersRequest =  (L_LINK_PARAM_TYPE *) MEM_malloc(sizeof(L_LINK_PARAM_TYPE));
		memset(LinkParametersRequest, 0, sizeof(L_LINK_PARAM_TYPE));
		LINK_PARAM_TYPE *elem;
		//there should be a cycle here, just one for now
		for (int i =0; i<(sizeof(paramTypeReq)/sizeof(char)); i++)
		{
			if (LinkParametersRequest->elem == NULL)
			{
				LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
				memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
				ParamTypeValue value;
				value.lp_gen = paramTypeReq[i];
				paramTypeStr->value = value;
				paramTypeStr->selector = 1;
				paramTypeStr->next = NULL;
			
				LinkParametersRequest->length++;
				LinkParametersRequest->elem = paramTypeStr;
				elem = LinkParametersRequest->elem;
			}
			else
			{

				LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
				memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
				ParamTypeValue value;
				value.lp_gen = paramTypeReq[i];
				paramTypeStr->value = value;
				paramTypeStr->selector = 1;
				paramTypeStr->next = NULL;
				LinkParametersRequest->length++;
				elem->next = paramTypeStr;
				elem = elem->next;
			}
		}
		
		getStatusReqSet->lpt = LinkParametersRequest;
		//SendGenericLinkGetParametersReq(node, mihf, reason->srcMihfID, node->nodeId, getStatusReqSet, timeSent, getSimTime(node)); //local
		//mihf->isUpdatingParamList = TRUE;
		//mihf->numLinkGetParamsSent++;
	}
	 */
	Message* timerMsg;
	timerMsg = MESSAGE_Alloc(node,
			NETWORK_LAYER,
			MIH_PROTOCOL,
			MSG_MIH_LinkGoingDownTimerAlarm); //Have to define this in messages/events enum

	MESSAGE_InfoAlloc(node, timerMsg, sizeof(down_reason));

	down_reason *timerInfo = (down_reason *) MESSAGE_ReturnInfo(timerMsg);
	timerInfo->linkId = reason->linkId;
	timerInfo->lgdReason = reason->lgdReason;
	timerInfo->timeInt = reason->timeInt;
	timerInfo->srcMihfID = reason->srcMihfID;

	mihf->linkGoingDownTimer = MESSAGE_Duplicate(node, timerMsg);

	MESSAGE_Send(node, timerMsg, 1 * SECOND);

//RMTriggerHandover(node, mihf, srcMihfID);
/*
	if (mihf->nbrMihfList->count!=0)
	{
		NbrMihData* nbrMihf=mihf->nbrMihfList->first;
		while (nbrMihf!=NULL)
		{
			if (nbrMihf->mihfId != srcMihfID)
				SendRemoteMihLinkGetParametersRequest(node,
									 mihf,
									 nbrMihf->interfaceAddress,
									 reason->srcMihfID,
									 mihf->transactionId++,
									 paramTypeReq
									 //and maybe other arguments
									 );
			mihf->isUpdatingParamList = TRUE;
			mihf->numLinkGetParamsSent++;
			nbrMihf=nbrMihf->next;
		}
	}*/
}

void RMProcessLinkUpInd (Node* node,
						   MihData* mihf,
						   link_up_str *up,
						   clocktype timeSent,
						   NodeAddress srcMihfID)
{
	    NodeAddress poaId;
		BOOL isDifferentMacProtocol;

    switch (up->LinkIdentifier->lid.lt)
    {
    case 19: //Wi-Fi
    case 27: //WiMAX
    {
    	if (up->LinkIdentifier->lid.la.ma.addr_type == 6) //6 - 802 address // should be true by default, doesn't hurt though to check
    				{
    					Mac802Address macAddr;
    					memcpy(&macAddr.byte,
    							up->LinkIdentifier->lid.la.ma.octet_string,
    							MAC_ADDRESS_LENGTH_IN_BYTE);
    					MacHWAddress macHWAddr;
    					Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
    					int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
    					poaId = MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr));
    				}
		break;
    }
    case 23: //UMTS
    {
    	poaId = (NodeAddress) (up->LinkIdentifier->lid.la.g3.cid[0] / 16);
    	break;
    }
    default:
    	break;

    }

	if (mihf->currentPoaId == 0) //there is no current PoA
	{
		mihf->currentPoaId = poaId; //extend this for multiple managed MNs
		mihf->currentPoaIdMacProtocol = GetProtocolForPoaId(node,mihf,poaId);
	}
	else
	{
		mihf->prevPoaId = mihf->currentPoaId;
		mihf->prevPoaIdMacProtocol = mihf->currentPoaIdMacProtocol;
		mihf->currentPoaId = poaId;
		mihf->currentPoaIdMacProtocol = GetProtocolForPoaId(node,mihf,poaId);
		//TODO: send link actions request
		//MAC_PROTOCOL protocol = GetProtocolForPoaId(node,mihf,mihf->prevPoaId);
		
		if(mihf->prevPoaIdMacProtocol != mihf->currentPoaIdMacProtocol)
		{
		
		LINK_ADDR *linkAddr = (LINK_ADDR *) MEM_malloc(sizeof(LINK_ADDR));
		memset(linkAddr, 0, sizeof(LINK_ADDR));

		switch(mihf->prevPoaIdMacProtocol)
		{
		case MAC_PROTOCOL_DOT11:
		case MAC_PROTOCOL_DOT16:
			{
				linkAddr->selector = 0;
				linkAddr->ma.addr_type = 6;
				linkAddr->ma.octet_string[0] = mihf->prevPoaId;
				//for (int i = 1; i<6; i++)
				//	linkAddr.ma.octet_string[i] = 0;
				break;
			}
		case MAC_PROTOCOL_CELLULAR:
			{
				linkAddr->selector = 1;
				linkAddr->g3.cid[0] = mihf->prevPoaId * 16;
				//for (int i = 1; i<4; i++)
				//	linkAddr.g3.cid[i] = 0;
				break;
			}
		default:
			{
				linkAddr->selector = 0;
				linkAddr->ma.addr_type = 6;
				//for (int i = 0; i<6; i++)
				//	linkAddr.ma.octet_string[i] = 0;
				break;
			}
		}

		SendLinkActionsReq(node,
							mihf,
							node->nodeId,
							linkAddr,
							1, //LINK_DISCONNECT
							2); //DATA_FWD -- a bit difficutl to do

		if (NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_BELLMANFORD))
			ScheduleTriggeredUpdate(node);
	}

	printf("MIH Function: PoA Id %u replaced by new PoA Id %u\n",
			mihf->prevPoaId, mihf->currentPoaId);
		//printf("    from MIHF ID %u\n", reason->srcMihfID);
	}
}


void RMProcessLinkDownInd (Node* node,
						   MihData* mihf,
						   link_down_str *down,
						   clocktype timeSent,
						   NodeAddress srcMihfID)
{			
	//mihf->numLinkGetParamsSent = 0;
	UNSIGNED_INT_1 paramTypeReq[1] = {2};
	/*
	if (node->nodeId != reason->srcMihfID) //the RM is not the current PoA
	{
		LINK_STATUS_REQ *getStatusReqSet =  (LINK_STATUS_REQ *) MEM_malloc(sizeof(LINK_STATUS_REQ));
		memset(getStatusReqSet, 0, sizeof(LINK_STATUS_REQ));

		getStatusReqSet->ldr = 0; //we want the number of classes of service supported
		getStatusReqSet->lsr = 0; //we want the op_mode


		
		L_LINK_PARAM_TYPE *LinkParametersRequest =  (L_LINK_PARAM_TYPE *) MEM_malloc(sizeof(L_LINK_PARAM_TYPE));
		memset(LinkParametersRequest, 0, sizeof(L_LINK_PARAM_TYPE));
		LINK_PARAM_TYPE *elem;
		//there should be a cycle here, just one for now
		for (int i =0; i<(sizeof(paramTypeReq)/sizeof(char)); i++)
		{
			if (LinkParametersRequest->elem == NULL)
			{
				LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
				memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
				ParamTypeValue value;
				value.lp_gen = paramTypeReq[i];
				paramTypeStr->value = value;
				paramTypeStr->selector = 1;
				paramTypeStr->next = NULL;
			
				LinkParametersRequest->length++;
				LinkParametersRequest->elem = paramTypeStr;
				elem = LinkParametersRequest->elem;
			}
			else
			{

				LINK_PARAM_TYPE *paramTypeStr =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
				memset(paramTypeStr, 0, sizeof(LINK_PARAM_TYPE));
				ParamTypeValue value;
				value.lp_gen = paramTypeReq[i];
				paramTypeStr->value = value;
				paramTypeStr->selector = 1;
				paramTypeStr->next = NULL;
				LinkParametersRequest->length++;
				elem->next = paramTypeStr;
				elem = elem->next;
			}
		}
		
		getStatusReqSet->lpt = LinkParametersRequest;
		//SendGenericLinkGetParametersReq(node, mihf, reason->srcMihfID, node->nodeId, getStatusReqSet, timeSent, getSimTime(node)); //local
		//mihf->isUpdatingParamList = TRUE;
		//mihf->numLinkGetParamsSent++;
	}
	 *//*
	Message* timerMsg;
	timerMsg = MESSAGE_Alloc(node,
			NETWORK_LAYER,
			MIH_PROTOCOL,
			MSG_MIH_LinkGoingDownTimerAlarm); //Have to define this in messages/events enum

	MESSAGE_InfoAlloc(node, timerMsg, sizeof(down_reason));

	down_reason *timerInfo = (down_reason *) MESSAGE_ReturnInfo(timerMsg);
	timerInfo->linkId = reason->linkId;
	timerInfo->lgdReason = reason->lgdReason;
	timerInfo->timeInt = reason->timeInt;
	timerInfo->srcMihfID = reason->srcMihfID;

	MESSAGE_Send(node, timerMsg, 1 * SECOND);*/

//RMTriggerHandover(node, mihf, srcMihfID);

	if (mihf->nbrMihfList->count!=0)
	{
		NbrMihData* nbrMihf=mihf->nbrMihfList->first;
		while (nbrMihf!=NULL)
		{
			if (nbrMihf->mihfId != srcMihfID)
				SendRemoteMihLinkGetParametersRequest(node,
									 mihf,
									 nbrMihf->interfaceAddress,
									 down->srcMihfID,
									 mihf->transactionId++,
									 paramTypeReq
									 //and maybe other arguments
									 );
			mihf->isUpdatingParamList = TRUE;
			//mihf->numLinkGetParamsSent++;
			nbrMihf=nbrMihf->next;
		}
	}
}



void ReceiveLinkDownIndication(Node* node,
									Message* msg)
{
	link_down_str *reason = (link_down_str *) MESSAGE_ReturnInfo(msg);
	
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received Link_down.indication\n",
               node->nodeId);
        printf("    from MIHF ID %u\n", reason->srcMihfID);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */


	mihf->stats->numLinkDownIndReceived++;

	//cancel Link_Going_Down timer alarm

	if (mihf->linkGoingDownTimer!=NULL)
	MESSAGE_CancelSelfMsg(node,mihf->linkGoingDownTimer);

	if (mihf->resourceManagerId == node->nodeId) //the RM resides in this node
		RMProcessLinkDownInd (node, mihf, reason, getSimTime(node), reason->srcMihfID);
	else //forward to RM which is not located in this node
	{/*
		NodeAddress destAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId (node, mihf->resourceManagerId);

		SendRemoteMihLinkGoingDownIndication(node, mihf, reason, destAddr);
*/
	}
}

void ReceiveLinkGoingDownIndication(Node* node,
									Message* msg)
{
	down_reason *reason = (down_reason *) MESSAGE_ReturnInfo(msg);
	
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received Link_Going_down.indication\n",
               node->nodeId);
        printf("    from MIHF ID %u\n", reason->srcMihfID);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */


	mihf->stats->numLinkGoingDownIndReceived++;

	if (mihf->resourceManagerId == node->nodeId) //the RM resides in this node
		RMProcessLinkGoingDownInd (node, mihf, reason, getSimTime(node), reason->srcMihfID);
	else //forward to RM which is not located in this node
	{
		NodeAddress destAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId (node, mihf->resourceManagerId);

		SendRemoteMihLinkGoingDownIndication(node, mihf, reason, destAddr);

	}
}

void SendGenericLinkGetParametersReq(Node* node,
								  MihData* mihf,
								  NodeAddress srcMihfID,
								  NodeAddress destMihfID,
								  LINK_STATUS_REQ* getStatusReqSet,
								  int transactionId,
								  clocktype transactionInitiated
								  /*UNSIGNED_INT_1 paramType[]*/) //just one for now
{
	
	Link_Get_Parameters_request(node,
								getStatusReqSet->lpt,
								getStatusReqSet->lsr,
								getStatusReqSet->ldr,
								srcMihfID,
								transactionId,
								transactionInitiated);
	mihf->stats->numLinkGetParamReqSent++;
}

								
void SendLinkGetParametersReq(Node* node,
						   MihData* mihf,
						   NodeAddress mihfID,
						   int transactionId,
						   clocktype transactionInitiated)
{

	LINK_PARAM_TYPE *rssi =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
	memset(rssi, 0, sizeof(LINK_PARAM_TYPE));
	ParamTypeValue value;
	value.lp_gen = 2;
	rssi->value = value;
	rssi->selector = 1;
	rssi->next = NULL;

	L_LINK_PARAM_TYPE *LinkParametersRequest =  (L_LINK_PARAM_TYPE *) MEM_malloc(sizeof(L_LINK_PARAM_TYPE));
	memset(LinkParametersRequest, 0, sizeof(L_LINK_PARAM_TYPE));

	LinkParametersRequest->length = 1;
	LinkParametersRequest->elem = rssi;
	
	LINK_STATES_REQ LinkStatesRequest = 0; //We want the OP_MODE

	LINK_DESC_REQ LinkDescriptorsRequest = 0; //We want the Classes of Service supported

	
	Link_Get_Parameters_request(node,
								LinkParametersRequest,
								LinkStatesRequest,
								LinkDescriptorsRequest,
								mihfID,
								transactionId,
								transactionInitiated);
	mihf->stats->numLinkGetParamReqSent++;
}

void SendLinkActionsReq(Node* node,
						MihData* mihf,
						NodeAddress mihfID,
						LINK_ADDR* linkAddr,
						LINK_AC_TYPE linkActionType,
						LINK_AC_ATTR linkActionAttribute
						)
{
	LINK_ACTION *linkAction =  (LINK_ACTION *) MEM_malloc(sizeof(LINK_ACTION));
		memset(linkAction, 0, sizeof(LINK_ACTION));
		linkAction->attr = linkActionAttribute;
		linkAction->type = linkActionType;

	Link_Action_request(node,
						mihfID,
						linkAction,
						0,
						linkAddr);
}


void ReceiveLinkGetParametersConfirm(Node* node,
									 Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	link_params *param_conf = (link_params *)MESSAGE_ReturnInfo(msg);

	if (node->nodeId == mihf->resourceManagerId) //this is the Resource Manager
	{
		//mihf->numLinkGetParamsRcvd++;
		//update params list
		if (param_conf->status == 0) //success
		{
			LINK_PARAM *elem = param_conf->linkParamStatusList->elem;
			for (int i=1; i<=(param_conf->linkParamStatusList->length); i++) //while (elem  != NULL)
			{
				
				switch(elem->selector)
				{
					case 0: //link parameter value
					{
						switch (elem->lpt->selector)
						{
							case 0: //generic link parameter type
							{
								switch (elem->lpt->value.lp_gen)
								{
									case 0: //data rate
									{ //to be completed
										break;
									}
									case 1: //signal strength
									{ //to be completed
										break;
									}
									case 2: //CINR/SINR
									{ 
										mihf->myCinrValue[param_conf->srcMihfID] = elem->value.lpv;
										break;
									}
									case 3: //throughput
									{ //to be completed
										break;
									}
									case 4: //packet error rate
									{ //to be completed
										break;
									}
									default:
										break;
								}
								break;
							}
							case 1: //qos parameter threshold
							{ //to be completed
								break;
							}
							case 2: //link parameter for GSM and GPRS
							{ //to be completed
								break;
							}
							case 3: //link parameter for EDGE
							{ //to be completed
								break;
							}
							case 4: //link parameter for Ethernet
							{ //to be completed
								break;
							}
							case 5: //link parameter for 802.11
							{ //to be completed
								break;
							}
							case 6: //link parameter for 802.11
							{ //to be completed
								break;
							}
							case 7: //link parameter for CDMA 2000
							{ //to be completed
								break;
							}
							case 8: //link parameter for UMTS
							{ //to be completed
								break;
							}
							case 9: //link parameter for CDMA2000 HRPD
							{ //to be completed
								break;
							}
							case 10: //link parameter for 802.16
							{ //to be completed
								break;
							}
							case 11: //link parameter for 802.20
							{ //to be completed
								break;
							}
							case 12: //link parameter for 802.22
							{ //to be completed
								break;
							}
							default:
								break;
						}
					}
					case 1: //qos parameter threshold
					{ //to be completed
						break;
					}
					default:
						break;
				}
				elem = elem->next;
			}
		}
			/**/
		//if (mihf->numLinkGetParamsRcvd == mihf->numLinkGetParamsSent) 
		//{
		//	mihf->isUpdatingParamList = FALSE;
		//	mihf->numLinkGetParamsRcvd = 0;
		//	mihf->numLinkGetParamsSent = 0;
			//trigger handover
			//RMTriggerHandover(node, mihf, param_conf->srcMihfID);
		//}//this should be inside a function/**/
	}

	else
		SendRemoteMihLinkGetParametersResponse(node,
									 mihf,
									 param_conf);

	mihf->stats->numLinkGetParamRspReceived++;

}

void ReceiveLinkUpIndication (Node* node, Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
	MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	link_up_str *up = (link_up_str *) MESSAGE_ReturnInfo(msg);

	mihf->stats->numLinkUpInd++;

	if (mihf->resourceManagerId == node->nodeId) {
#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received Link_Up.indication\n",
               node->nodeId);
        printf("    from MIHF ID %u\n", up->dest);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */

	RMProcessLinkUpInd(node,
						mihf,
						up,
						getSimTime(node), //have to change to time sent, not received
						up->dest);

	}//the RM resides in this node
		//RMProcessLinkGoingDownInd (node, mihf, reason, getSimTime(node), reason->srcMihfID);
	else //forward to RM which is not located in this node
	{
		NodeAddress destAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId (node, mihf->resourceManagerId);

		SendRemoteMihLinkUpIndication(node, mihf, up, destAddr);

	}
	//MESSAGE_Free(node, msg);
}

void RMUpdateBestPoA(Node* node, MihData *mihf, NodeAddress srcMihfID)
{
	double maxRss = -1.7976931348623158e+308;
	NodeAddress targetMihfId = 0;
	MAC_PROTOCOL targetMihfIdMacProtocol = MAC_PROTOCOL_NONE;
	
	if (mihf->nbrMihfList->count!=0) //decide on the best PoA based on Rss
	{
		NbrMihData* nbrMihf = mihf->nbrMihfList->first;
		//double maxRss = -DBL_MAX;
				
		while (nbrMihf!=NULL)
		{
			double hoMargin = 2.0;
			if (nbrMihf->macProtocol == targetMihfId || nbrMihf->macProtocol != MAC_PROTOCOL_NONE)
			{
				if ((nbrMihf->rssValue[srcMihfID] - maxRss > hoMargin) && (nbrMihf->measTime[srcMihfID] + MEASUREMENT_VALID_TIME >= getSimTime(node))) //hysteresis
				{
					maxRss = nbrMihf->rssValue[srcMihfID];
					targetMihfId = nbrMihf->mihfId;
					targetMihfIdMacProtocol = nbrMihf->macProtocol;
				}
			}
			else if (nbrMihf->macProtocol != targetMihfId)
			{ //just for UMTS and WiFi for now
				if ((nbrMihf->macProtocol == MAC_PROTOCOL_DOT11) && (targetMihfIdMacProtocol == MAC_PROTOCOL_CELLULAR))
				{
					if ((nbrMihf->rssValue[srcMihfID] - maxRss - 27 > hoMargin) && (nbrMihf->measTime[srcMihfID] + MEASUREMENT_VALID_TIME >= getSimTime(node)))
					{
						maxRss = nbrMihf->rssValue[srcMihfID];
						targetMihfId = nbrMihf->mihfId;
						targetMihfIdMacProtocol = nbrMihf->macProtocol;
					}
				}
				else if	((nbrMihf->macProtocol == MAC_PROTOCOL_CELLULAR) && (targetMihfIdMacProtocol == MAC_PROTOCOL_DOT11))
				{
					if ((nbrMihf->rssValue[srcMihfID] - maxRss + 27 > hoMargin) && (nbrMihf->measTime[srcMihfID] + MEASUREMENT_VALID_TIME >= getSimTime(node)))
					{
						maxRss = nbrMihf->rssValue[srcMihfID];
						targetMihfId = nbrMihf->mihfId;
						targetMihfIdMacProtocol = nbrMihf->macProtocol;
					}
				}

			}

			nbrMihf=nbrMihf->next;
		}

	}

	if ((targetMihfId!=0) && (targetMihfIdMacProtocol!=MAC_PROTOCOL_NONE))
	{
		mihf->targetMihfId = targetMihfId;
		mihf->targetMihfMacProtocol = targetMihfIdMacProtocol;
		mihf->targetMihfRss = maxRss;

		if ((mihf->targetMihfId != mihf->currentPoaId) /*|| (mihf->prevPoaId==0)*/) //if 
			//TODO
			// send link actions request
			//set status to updating PoA

		for (int i = 0; i<node->numberInterfaces; i++)
		{
			Message* newMsg;
			if (node->macData[i]->macProtocol == MAC_PROTOCOL_CELLULAR) //send message to layer 3 of cellular, since that's where it is handled
			{
				
				newMsg = MESSAGE_Alloc(node,
						NETWORK_LAYER,
						NETWORK_PROTOCOL_CELLULAR,
						MSG_MIH_Report_Best_PoA);

				//MESSAGE_SetInstanceId(newMsg, i); //don't think it's necessary
			}
			else
			{

				newMsg = MESSAGE_Alloc(node,
						MAC_LAYER,
						node->macData[i]->macProtocol,
						MSG_MIH_Report_Best_PoA);

				MESSAGE_SetInstanceId(newMsg, i);

			}

			MESSAGE_InfoAlloc(node, newMsg, sizeof(MihReportBestPoa));
			MihReportBestPoa *info = (MihReportBestPoa *)MESSAGE_ReturnInfo(newMsg);

			info->bestPoaId  = targetMihfId;
			info->rssValue = maxRss;
			info->cinrValue = 0;
			info->mobileNodeId = srcMihfID;
			info->macProtocol = targetMihfIdMacProtocol;

			MESSAGE_Send(node, newMsg, 0);
		}
	}
}

void RMProcessLinkParamReport(Node* node,
							  MihData* mihf,
							  L_LINK_PARAM_RPT *linkParamRptList,
							  NodeAddress srcMihfID,
							  NodeAddress poaId)
{

	BOOL printMeasurements = TRUE;
	LINK_PARAM_RPT *elem = linkParamRptList->elem;


	for (int i=0; i<linkParamRptList->length; i++)
	{
		switch(elem->lp->selector)
			{
				case 0: //link parameter value
				{
					switch (elem->lp->lpt->selector)
					{
						case 0: //generic link parameter type
						{
							switch (elem->lp->lpt->value.lp_gen)
							{
								case 0: //data rate
								{ //to be completed
									break;
								}
								case 1: //signal strength
								{
									if (mihf->nbrMihfList->count!=0)
									{
										NbrMihData* nbrMihf=mihf->nbrMihfList->first;
										while (nbrMihf!=NULL)
										{
											if (nbrMihf->mihfId == poaId)
											{
												nbrMihf->rssValue[srcMihfID] =  ConvertIntToDouble(elem->lp->value.lpv);
												nbrMihf->measTime[srcMihfID] = getSimTime(node); //this is good only when RM is on same node, else there is a delay from the MIH Protocol transport


//												if (printMeasurements)
//												{
//													char filenameStr[MAX_STRING_LENGTH];
//													ctoa((Int64) nbrMihf->mihfId,filenameStr);
//													clocktype currentTime = getSimTime(node);
//													FILE *fp;
////													int i=1;
//													if (fp = fopen(filenameStr, "a"))
//													{
//														fprintf(fp, "%" TYPES_64BITFMT "d", currentTime);
//														fprintf(fp, "%u ", nbrMihf->mihfId);
//														fprintf(fp, "%u ", srcMihfID);
//														fprintf(fp, "%f ", nbrMihf->rssValue[srcMihfID]);
////#ifdef _WIN32
////														fprintf(fp, "%" TYPES_64BITFMT "d", currentTime);
////														fprintf(fp, "%I64d ", delay);
////														fprintf(fp, "%I64d ", serverPtr->actJitter);
////														fprintf(fp, "%I64d\n", instantThroughput);
////#else
////														fprintf(fp, "%lld ", currentTime);
////														fprintf(fp, "%lld ", delay);
////														fprintf(fp, "%lld ", serverPtr->actJitter);
////														fprintf(fp, "%lld\n", instantThroughput);
////#endif
//														fclose(fp);
////														i++;
//													}
//													else
//														printf("Error opening stats\n");
//												}

#if DEBUG
												{
													
													printf("RSS value of node %u from PoA ID %u is %f dBm\n",
														   srcMihfID, poaId, nbrMihf->rssValue[srcMihfID]);
												}
#endif
												break;
											}
											nbrMihf=nbrMihf->next;
										}
									}
									break;
								}
								case 2: //CINR/SINR
								{
									if (mihf->nbrMihfList->count!=0)
									{
										NbrMihData* nbrMihf=mihf->nbrMihfList->first;
										while (nbrMihf!=NULL)
										{
											if (nbrMihf->mihfId == poaId)
											{
												nbrMihf->cinrValue[srcMihfID] = ConvertIntToDouble(elem->lp->value.lpv);
												nbrMihf->measTime[srcMihfID] = getSimTime(node); //this is good only when RM is on same node, else there is a delay from the MIH Protocol transport

												if (printMeasurements)
												{
													char filenameStr[MAX_STRING_LENGTH];
													ctoa((Int64) nbrMihf->mihfId,filenameStr);
													clocktype currentTime = getSimTime(node);
													FILE *fp;
//													int i=1;
													if (fp = fopen(filenameStr, "a"))
													{
														fprintf(fp, "%" TYPES_64BITFMT "d", currentTime);
														fprintf(fp, "%u ", nbrMihf->mihfId);
														fprintf(fp, "%u ", srcMihfID);
														fprintf(fp, "%f ", nbrMihf->rssValue[srcMihfID]);
														fprintf(fp, "%f\n ", nbrMihf->cinrValue[srcMihfID]);
//#ifdef _WIN32
//														fprintf(fp, "%" TYPES_64BITFMT "d", currentTime);
//														fprintf(fp, "%I64d ", delay);
//														fprintf(fp, "%I64d ", serverPtr->actJitter);
//														fprintf(fp, "%I64d\n", instantThroughput);
//#else
//														fprintf(fp, "%lld ", currentTime);
//														fprintf(fp, "%lld ", delay);
//														fprintf(fp, "%lld ", serverPtr->actJitter);
//														fprintf(fp, "%lld\n", instantThroughput);
//#endif
														fclose(fp);
//														i++;
													}
													else
														printf("Error opening stats\n");
												}

#if DEBUG
												{
													
													printf("CINR value of node %u from PoA ID %u is %f\n",
														   srcMihfID, poaId, nbrMihf->cinrValue[srcMihfID]);
												}
#endif
												break;
											}
											nbrMihf=nbrMihf->next;
										}
									}
									break;
								}
								case 3: //throughput
								{ //to be completed
									break;
								}
								case 4: //packet error rate
								{ //to be completed
									break;
								}
								default:
									break;
							}
							break;
						}
						case 1: //qos parameter threshold
						{ //to be completed
							break;
						}
						case 2: //link parameter for GSM and GPRS
						{ //to be completed
							break;
						}
						case 3: //link parameter for EDGE
						{ //to be completed
							break;
						}
						case 4: //link parameter for Ethernet
						{ //to be completed
							break;
						}
						case 5: //link parameter for 802.11
						{ //to be completed
							break;
						}
						case 6: //link parameter for 802.11
						{ //to be completed
							break;
						}
						case 7: //link parameter for CDMA 2000
						{ //to be completed
							break;
						}
						case 8: //link parameter for UMTS
						{ //to be completed
							break;
						}
						case 9: //link parameter for CDMA2000 HRPD
						{ //to be completed
							break;
						}
						case 10: //link parameter for 802.16
						{ //to be completed
							break;
						}
						case 11: //link parameter for 802.20
						{ //to be completed
							break;
						}
						case 12: //link parameter for 802.22
						{ //to be completed
							break;
						}
						default:
							break;
					}
				}
				case 1: //qos parameter threshold
				{ //to be completed
					break;
				}
				default:
					break;
			}
			elem = elem->next;
	}

	RMUpdateBestPoA(node, mihf, srcMihfID);

}

void ReceiveRemoteMihLinkParametersReportIndication(Node* node,
											  Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

    //update stats
	mihf->stats->numMihLinkParametersReportIndReceived++;

	//retrieve header
	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	NodeAddress srcMihfId = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);

	param_signal *param_list =  (param_signal *) MEM_malloc(sizeof(param_signal));
	memset(param_list, 0, sizeof(param_signal));

	char *buff = hdr->payload;
	//parse payload
	param_list->dest = *((NodeAddress*) (buff));
	param_list->linkParamRptList = *((L_LINK_PARAM_RPT**) (buff+sizeof(NodeAddress)));
	param_list->linkId = *((LINK_TUPLE_ID**) (buff + +sizeof(NodeAddress) + sizeof(L_LINK_PARAM_RPT*)));
//	param_list->dest = *((NodeAddress*) (buff + sizeof(L_LINK_PARAM_RPT*)+ sizeof(LINK_TUPLE_ID*)));
	for (int j=0; j<=5;j++)
		param_list->poaId[j] = ((unsigned char *) (buff + sizeof(L_LINK_PARAM_RPT*)+ sizeof(LINK_TUPLE_ID*)+sizeof(NodeAddress)))[j];

	//LINK_PARAM_RPT *elem = param_list->linkParamRptList->elem;


	NodeAddress poaId;
	switch (param_list->linkId->lid.lt)
	{
	case 19:
	case 27:
	{
		//IEEE 802.11 802.16
		if (param_list->linkId->lid.la.ma.addr_type = 6) //should be true by default, doesn't hurt though to check
		{
			Mac802Address macAddr;
			memcpy(&macAddr.byte,
					param_list->linkId->lid.la.ma.octet_string,
					MAC_ADDRESS_LENGTH_IN_BYTE);
			MacHWAddress macHWAddr;
			Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
			int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
			poaId = MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr));
		}
		break;
	}
	case 23:
	{
		//UMTS
		poaId = (NodeAddress) (param_list->linkId->lid.la.g3.cid[0] / 16);
		break;
	}
	}


#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received Link_Parameters_Report.indication\n",
               node->nodeId);
		printf("    from MIHF ID %u\n", param_list->dest);
		printf("containing measurements from PoA ID %u\n", poaId);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */

	RMProcessLinkParamReport(node,
							  mihf,
							  param_list->linkParamRptList,
							  param_list->dest,
							  poaId);
	/*
	for (int i=0; i<param_list->linkParamRptList->length; i++)
	{
		switch(elem->lp->selector)
			{
				case 0: //link parameter value
				{
					switch (elem->lp->lpt->selector)
					{
						case 0: //generic link parameter type
						{
							switch (elem->lp->lpt->value.lp_gen)
							{
								case 0: //data rate
								{ //to be completed
									break;
								}
								case 1: //signal strength
								{
									if (mihf->nbrMihfList->count!=0)
									{
										NbrMihData* nbrMihf=mihf->nbrMihfList->first;
										while (nbrMihf!=NULL)
										{
											if (nbrMihf->mihfId == poaId)
											{
												nbrMihf->rssValue[param_list->dest] =  ConvertIntToDouble(elem->lp->value.lpv);
#if DEBUG
												{
													
													printf("RSS value of node %u from PoA ID %u is %f dBm\n",
														   param_list->dest, poaId, nbrMihf->rssValue[param_list->dest]);
												}
#endif
												break;
											}
											nbrMihf=nbrMihf->next;
										}
									}
									break;
								}
								case 2: //CINR/SINR
								{
									if (mihf->nbrMihfList->count!=0)
									{
										NbrMihData* nbrMihf=mihf->nbrMihfList->first;
										while (nbrMihf!=NULL)
										{
											if (nbrMihf->mihfId == poaId)
											{
												nbrMihf->cinrValue[param_list->dest] = ConvertIntToDouble(elem->lp->value.lpv);
#if DEBUG
												{
													
													printf("CINR value of node %u from PoA ID %u is %f\n",
														   param_list->dest, poaId, nbrMihf->cinrValue[param_list->dest]);
												}
#endif
												break;
											}
											nbrMihf=nbrMihf->next;
										}
									}
									break;
								}
								case 3: //throughput
								{ //to be completed
									break;
								}
								case 4: //packet error rate
								{ //to be completed
									break;
								}
								default:
									break;
							}
							break;
						}
						case 1: //qos parameter threshold
						{ //to be completed
							break;
						}
						case 2: //link parameter for GSM and GPRS
						{ //to be completed
							break;
						}
						case 3: //link parameter for EDGE
						{ //to be completed
							break;
						}
						case 4: //link parameter for Ethernet
						{ //to be completed
							break;
						}
						case 5: //link parameter for 802.11
						{ //to be completed
							break;
						}
						case 6: //link parameter for 802.11
						{ //to be completed
							break;
						}
						case 7: //link parameter for CDMA 2000
						{ //to be completed
							break;
						}
						case 8: //link parameter for UMTS
						{ //to be completed
							break;
						}
						case 9: //link parameter for CDMA2000 HRPD
						{ //to be completed
							break;
						}
						case 10: //link parameter for 802.16
						{ //to be completed
							break;
						}
						case 11: //link parameter for 802.20
						{ //to be completed
							break;
						}
						case 12: //link parameter for 802.22
						{ //to be completed
							break;
						}
						default:
							break;
					}
				}
				case 1: //qos parameter threshold
				{ //to be completed
					break;
				}
				default:
					break;
			}
			elem = elem->next;
	}
*/

	//MESSAGE_Free(node, msg);
}

void ReceiveRemoteMihLinkGoingDownIndication(Node* node,
											  Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	NodeAddress mihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);

	
	mihf->stats->numMihLinkGoingDownIndReceived++;

	down_reason *reason =  (down_reason *) MEM_malloc(sizeof(down_reason));
	memset(reason, 0, sizeof(down_reason));
	

	char *buff = hdr->payload;

	reason->linkId = *((LINK_TUPLE_ID**) buff);
	reason->timeInt = *((UNSIGNED_INT_2 *) (buff + sizeof(LINK_TUPLE_ID*)));
	reason->lgdReason = *((LINK_GD_REASON *)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(UNSIGNED_INT_2)));
	reason->srcMihfID = *((NodeAddress *)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(UNSIGNED_INT_2) + sizeof(LINK_GD_REASON)));
	
#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received MIH_Link_Going_down.indication\n",
               node->nodeId);
        printf("    from MIHF ID %u\n", reason->srcMihfID);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */

	if ((mihf->ongoingTransaction[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] == FALSE) ||
		(mihf->ongoingTransactionId[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] != hdr->transactionId))
	{
		RMProcessLinkGoingDownInd (node,
									mihf,
									reason,
									hdr->timeSent,
									mihfID);
		
		mihf->ongoingTransaction[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] = TRUE;
		mihf->ongoingTransactionId[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] = hdr->transactionId;
	}


	
	
	
	MESSAGE_Free(node, msg);
}

void ReceiveRemoteMihLinkUpIndication(Node* node,
										Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	NodeAddress mihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);

	
	mihf->stats->numMihLinkUpIndReceived++;

	link_up_str *up =  (link_up_str *) MEM_malloc(sizeof(link_up_str));
	memset(up, 0, sizeof(link_up_str));
	

	char *buff = hdr->payload;

	up->LinkIdentifier = *((LINK_TUPLE_ID**) buff);
	up->OldAccessRouter = *((LINK_ADDR**) (buff + sizeof(LINK_TUPLE_ID*)));
	up->NewAccessRouter = *((LINK_ADDR**)(buff + sizeof(LINK_TUPLE_ID*) + sizeof(LINK_ADDR*)));
	up->IpRenewalFlag = *((IP_RENEWAL_FLAG *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*)));
	up->MobilityManagementSupport = *((IP_MOB_MGMT *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*) + sizeof(IP_RENEWAL_FLAG)));
	up->dest = *((NodeAddress *)(buff + sizeof(LINK_TUPLE_ID*) + 2*sizeof(LINK_ADDR*) + sizeof(IP_RENEWAL_FLAG) + sizeof(IP_MOB_MGMT)));
	
	RMProcessLinkUpInd(node,
						mihf,
						up,
						hdr->timeSent,
						up->dest);
/*
	NodeAddress poaId;
	switch (up->LinkIdentifier->lid.lt)
    {
    case 19: //Wi-Fi
    case 27:
    {
    	if (up->LinkIdentifier->lid.la.ma.addr_type == 6) //6 - 802 address // should be true by default, doesn't hurt though to check
    				{
    					Mac802Address macAddr;
    					memcpy(&macAddr.byte,
    							up->LinkIdentifier->lid.la.ma.octet_string,
    							MAC_ADDRESS_LENGTH_IN_BYTE);
    					MacHWAddress macHWAddr;
    					Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
    					int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
    					poaId = MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr));
    				}
    				break;

    }
    case 23: //UMTS
    {
    	poaId = (NodeAddress) (up->LinkIdentifier->lid.la .g3.cid[0] / 16);
    	break;
    }
    default:
    	break;

    }

*/
	/*
	if ((mihf->ongoingTransaction[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] == FALSE) ||
		(mihf->ongoingTransactionId[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] != hdr->transactionId))
	{
		RMProcessLinkGoingDownInd (node,
									mihf,
									reason,
									hdr->timeSent,
									mihfID);
		
		mihf->ongoingTransaction[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] = TRUE;
		mihf->ongoingTransactionId[MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr)] = hdr->transactionId;
	}


	
	*/
	
	MESSAGE_Free(node, msg);
}

void ReceiveRemoteMihLinkGetParametersRequest(Node* node,
										Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	NodeAddress srcMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);
	//NodeAddress destMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->destAddr);

	link_parameters *param_req =  (link_parameters *) MEM_malloc(sizeof(link_parameters));
	memset(param_req, 0, sizeof(link_parameters));

	char *buff = hdr->payload;
	param_req->statusReq = *((LINK_STATUS_REQ **)buff);
	param_req->srcMihfID = *((NodeAddress *)(buff + sizeof(LINK_STATUS_REQ *)));
	param_req->destMihfID = *((NodeAddress *)(buff + sizeof(LINK_STATUS_REQ *) + sizeof(NodeAddress)));
	param_req->transactionId = *((int *)(buff + sizeof(LINK_STATUS_REQ *) + 2*sizeof(NodeAddress)));
	param_req->transactionInitiated = *((clocktype *)(buff + sizeof(LINK_STATUS_REQ *) + 2*sizeof(NodeAddress) + sizeof(int)));
	//have to send request message to MAC

#if DEBUG
    {
        char clockStr[24];
        printf("MIH Function: Node %u received MIH_Link_Get_Parameters.request\n",
               node->nodeId);
		printf("    from MIHF ID %u\n", param_req->destMihfID);
        TIME_PrintClockInSecond(getSimTime(node), clockStr);
        printf("time is %sS\n", clockStr);
    }
#endif /* DEBUG */

	SendGenericLinkGetParametersReq(node,
									mihf,
									param_req->srcMihfID,
									param_req->destMihfID,
									param_req->statusReq,
									param_req->transactionId,
									param_req->transactionInitiated);
	/*
	SendLinkGetParametersReq (node, 
							  mihf, 
							  destMihfID, 
							  hdr->transactionId,
							  hdr->transactionInitiated);
*/
	mihf->stats->numMihLinkGetParamReqReceived++;

}

void ReceiveRemoteMihLinkGetParametersResponse(Node* node, 
										 Message* msg)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;
	
	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	link_params *param_conf =  (link_params *) MEM_malloc(sizeof(link_params));
	memset(param_conf, 0, sizeof(link_params));

	char *buff = hdr->payload;
	param_conf->status = *((STATUS *)buff);
	param_conf->linkParamStatusList = *((l_LINK_PARAM **)(buff + sizeof(STATUS)));
	param_conf->linkStatesRsp = *((l_LINK_STATES_RSP **)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *)));
	param_conf->linkDescRsp = *((l_LINK_DESC_RSP **)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *)));
	param_conf->destMihfID = *((NodeAddress *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *)));
	param_conf->srcMihfID = *((NodeAddress *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + sizeof(NodeAddress)));
	param_conf->transactionId = *((int *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + 2*sizeof(NodeAddress)));
	param_conf->transactionInitiated = *((clocktype *)(buff + sizeof(STATUS) + sizeof(l_LINK_PARAM *) + sizeof(l_LINK_STATES_RSP *) + sizeof(l_LINK_DESC_RSP *) + 2*sizeof(NodeAddress)+ sizeof(int)));

	NodeAddress srcMihfId = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);

	//mihf->numLinkGetParamsRcvd++;
	//update params in list
	if (param_conf->status == 0) //success
		{
			LINK_PARAM *elem = param_conf->linkParamStatusList->elem;
			for (int i=1; i<=(param_conf->linkParamStatusList->length); i++) //while (elem  != NULL)
			{
				
				switch(elem->selector)
				{
					case 0: //link parameter value
					{
						switch (elem->lpt->selector)
						{
							case 0: //generic link parameter type
							{
								switch (elem->lpt->value.lp_gen)
								{
									case 0: //data rate
									{ //to be completed
										break;
									}
									case 1: //signal strength
									{ //to be completed
										break;
									}
									case 2: //CINR/SINR
									{
										if (mihf->nbrMihfList->count!=0)
										{
											NbrMihData* nbrMihf=mihf->nbrMihfList->first;
											while (nbrMihf!=NULL)
											{
												if (nbrMihf->mihfId == srcMihfId)
												{
													nbrMihf->cinrValue[param_conf->srcMihfID] = elem->value.lpv;
													break;
												}												
												nbrMihf=nbrMihf->next;
											}
										}
										break;
									}
									case 3: //throughput
									{ //to be completed
										break;
									}
									case 4: //packet error rate
									{ //to be completed
										break;
									}
									default:
										break;
								}
								break;
							}
							case 1: //qos parameter threshold
							{ //to be completed
								break;
							}
							case 2: //link parameter for GSM and GPRS
							{ //to be completed
								break;
							}
							case 3: //link parameter for EDGE
							{ //to be completed
								break;
							}
							case 4: //link parameter for Ethernet
							{ //to be completed
								break;
							}
							case 5: //link parameter for 802.11
							{ //to be completed
								break;
							}
							case 6: //link parameter for 802.11
							{ //to be completed
								break;
							}
							case 7: //link parameter for CDMA 2000
							{ //to be completed
								break;
							}
							case 8: //link parameter for UMTS
							{ //to be completed
								break;
							}
							case 9: //link parameter for CDMA2000 HRPD
							{ //to be completed
								break;
							}
							case 10: //link parameter for 802.16
							{ //to be completed
								break;
							}
							case 11: //link parameter for 802.20
							{ //to be completed
								break;
							}
							case 12: //link parameter for 802.22
							{ //to be completed
								break;
							}
							default:
								break;
						}
					}
					case 1: //qos parameter threshold
					{ //to be completed
						break;
					}
					default:
						break;
				}
				elem = elem->next;
			}
		}
	//if (mihf->numLinkGetParamsRcvd == mihf->numLinkGetParamsSent) 
	//{
	//	mihf->isUpdatingParamList = FALSE;
	//	mihf->numLinkGetParamsRcvd = 0;
	//	mihf->numLinkGetParamsSent = 0;
		//trigger handover
	//RMTriggerHandover(node, mihf, param_conf->srcMihfID);
	//}
	mihf->stats->numMihLinkGetParamRspReceived++;

	MESSAGE_Free(node, msg);
}
/*void RMTriggerHandover(Node* node,
					   MihData* mihf,  //for now
					   NodeAddress destMihfId
					   )
{
	Address destAddr;
	NodeAddress targetMihfId;
	NbrMihData* nbrMihf;
	UNSIGNED_INT_1 maxCinr;
	Address* info;

	if (mihf->nbrMihfList->count!=0)
	{
		nbrMihf = mihf->nbrMihfList->first;
		maxCinr = 0;
		
		while (nbrMihf!=NULL)
		{
			if (nbrMihf->cinrValue[destMihfId] > maxCinr)
			{
				maxCinr = nbrMihf->cinrValue[destMihfId];
				targetMihfId = nbrMihf->mihfId;
			}
			nbrMihf=nbrMihf->next;
		}

	}*/
	/*
	// try routing

	Message* newMsg = MESSAGE_Alloc(
                 node,
                 NETWORK_LAYER,
                 ROUTING_PROTOCOL_AODV,
                 MSG_NETWORK_CheckRouteTimeout);
                 
    MESSAGE_InfoAlloc(
			node,
			newMsg,
			sizeof(Address));
			
	info = (Address *) MESSAGE_ReturnInfo(newMsg);

	destAddr.networkType = NETWORK_IPV4;
	destAddr.interfaceAddr.ipv4 = MAPPING_GetDefaultInterfaceAddressFromNodeId (node, destMihfId); //could replace targetMihfId with destMihfId, see what happens

	memcpy(info, &destAddr, sizeof(Address));

    	// Schedule the timer after the specified delay
	MESSAGE_Send(node, newMsg, 0);

}
*/



void ReceiveRemoteMihMessage (Node* node,
							  Message* msg) //And possibly other arguments
{
	if (node->isMihfEnabled == FALSE)
	{
		MESSAGE_Free(node, msg);
		return;
	}

	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	BOOL found = FALSE;
	BOOL isMulticast = FALSE;

	MihHeaderType* hdr = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));

	int transactionId = hdr->transactionId;
	NodeAddress myMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->destAddr);
	NodeAddress peerMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, hdr->srcAddr);
	clocktype currentTime = getSimTime(node);
	if (hdr->destAddr == ANY_DEST)
		isMulticast = TRUE;

	if (((hdr->opcode == REQUEST) || (hdr->opcode == INDICATION)) && (hdr->payloadSize == 0) && (hdr->ackRsp == TRUE))
	{ //it's an empty acknowledgement message, there's a source machine
		TransactionSource *transactionSrc = mihf->transactionSrcList->first;

		while (transactionSrc != NULL)
		{
			if ((transactionSrc->transactionId == transactionId) &&
			(transactionSrc->myMihfID == myMihfID) &&
			(transactionSrc->peerMihfID == peerMihfID))
				break;
			transactionSrc = transactionSrc->next;
		}
/////We'll think about it
		if (transactionSrc == NULL)
		ERROR_ReportWarning("transaction source state machine already stopped");
		else if (transactionSrc->ackReqState == ACK_REQ_WAIT_ACK)
		{
				transactionSrc->ackReqState = ACK_REQ_SUCCESS;
				transactionSrc->ackReqStatus = SUCCESS;
		} 
		mihf->stats->numAckRspReceived++;
	}
	else
		if ((hdr->opcode == RESPONSE) && (hdr->payloadSize == 0) && (hdr->ackRsp == TRUE))
		{ //it's an empty acknowledgement message, there's a dest machine
			TransactionDest *transactionDest = mihf->transactionDestList->first;

			while (transactionDest != NULL)
			{
				if ((transactionDest->transactionId == transactionId) &&
			(transactionDest->myMihfID == myMihfID) &&
			(transactionDest->peerMihfID == peerMihfID))
					break;
				transactionDest = transactionDest->next;
			}

			ERROR_Assert(transactionDest != NULL, "No matching transaction");

			if(transactionDest->state=SEND_RESPONSE)
			{
				transactionDest->state = STATE_SUCCESS;
				transactionDest->transactionStatus = SUCCESS;
			}
		}
		else
			if ((hdr->opcode == REQUEST) || (hdr->opcode == INDICATION))
			{ //have to create dest machine
				TransactionDestInitTransaction(node,
												msg,
												hdr->destAddr,
												hdr->srcAddr,
												transactionId,
												isMulticast,
												hdr->ackReq,
												TRANSACTION_LIFETIME + hdr->transactionInitiated);

				TransactionDest* transactionDest = mihf->transactionDestList->last;

				if (transactionDest->startAckResponder == TRUE)
				{
					//build empty acknowledgement message??
					Message* mihMsg = CreateRemoteMihMessage(node,
											mihf,
											hdr->srcAddr, 
											0,
											0,
											hdr->sid,
											hdr->opcode,
											hdr->aid,
											hdr->transactionId,
											hdr->transactionInitiated,
											FALSE,
											TRUE);
					MihSendPacketToIp(node,
									  mihMsg,
									  hdr->destAddr,
									  hdr->srcAddr,
									  ANY_INTERFACE,
									  IPDEFTTL);
					transactionDest->ackRspState = RETURN_ACK;
				}
				if (hdr->opcode == INDICATION)
				{
					transactionDest->state = STATE_SUCCESS;
					transactionDest->transactionStatus = SUCCESS;
				}
			}
			else
				if (hdr->opcode == RESPONSE)
				{ //it's a source machine
					TransactionSource *transactionSrc = mihf->transactionSrcList->first;

					while (transactionSrc != NULL)
					{
						if ((transactionSrc->transactionId == transactionId) &&
							(transactionSrc->myMihfID == myMihfID) &&
							(transactionSrc->peerMihfID == peerMihfID))
							break;
						transactionSrc = transactionSrc->next;
					}
					ERROR_Assert(transactionSrc != NULL, "No matching transaction");

					if (transactionSrc->state == WAIT_RESPONSE_MSG)
					{
						if (hdr->ackReq == TRUE)
							transactionSrc->startAckResponder = TRUE;
						if (hdr->ackRsp == TRUE)
							{
							transactionSrc->ackReqStatus = SUCCESS;
							transactionSrc->ackReqState = ACK_REQ_SUCCESS;
						}
						transactionSrc->responseReceived = TRUE;
						transactionSrc->state = STATE_SUCCESS;
						transactionSrc->transactionStatus = SUCCESS; //probably will have to free memory here also for this transaction
						MESSAGE_Free(node, transactionSrc->currentMessage); //release the saved copy of the message since it is no longer needed
						MESSAGE_CancelSelfMsg(node, transactionSrc->currentRetransmissionTimer);
						transactionSrc->currentRetransmissionTimer = NULL;
						mihf->stats->numRetransmissionTimersCancelled++;
					}
				}
					
	if (hdr->payloadSize != 0)	//Process message
	switch (hdr->opcode)
	{
		case REQUEST:
			{
				switch (hdr->aid)
				{
					case MIH_REGISTER:
						{
							//ReceiveRemoteMihRegisterRequest(node, msg);
							break;
						}
					case MIH_DEREGISTER:
						{
							//ReceiveRemoteMihDeRegisterRequest(node, msg);
							break;
						}
					case MIH_EVENT_SUBSCRIBE:
						{
							//ReceiveRemoteMihEventSubscribeRequest(node, msg);
							break;
						}
					case MIH_EVENT_UNSUBSCRIBE:
						{
							//ReceiveRemoteMihEventUnsubscribeRequest(node, msg);
							break;
						}
					case MIH_LINK_GET_PARAMETERS:
						{
							ReceiveRemoteMihLinkGetParametersRequest(node, msg);
							break;
						}
					case MIH_CAPABILITY_DISCOVER:
						{
							//ReceiveRemoteMihCapabilityDiscoverRequest(node, msg);
							break;
						}
					case MIH_LINK_CONFIGURE_THRESHOLDS:
						{
							//ReceiveRemoteMihLinkConfigureThresholdsRequest(node, msg);
							break;
						}
					case MIH_LINK_ACTIONS:
						{
							//ReceiveRemoteMihLinkActionRequest(node, msg);
							break;
						}
					case MIH_NET_HO_CANDIDATE_QUERY:
						{
							//ReceiveRemoteMihNetHoCandidateQueryRequest(node, msg);
							break;
						}
					case MIH_MN_HO_CANDIDATE_QUERY:
						{
							//ReceiveRemoteMihMnHoCandidateQueryRequest(node, msg);
							break;
						}
					case MIH_N2N_HO_CANDIDATE_QUERY_RESOURCES:
						{
							//ReceiveRemoteMihN2nHoCandidateQueryResourcesRequest(node, msg);
							break;
						}
					case MIH_MN_HO_COMMIT:
						{
							//ReceiveRemoteMihMnHoCommitRequest(node, msg);
							break;
						}
					case MIH_NET_HO_COMMIT:
						{
							//ReceiveRemoteMihNetHoCommitRequest(node, msg);
							break;
						}
					case MIH_N2N_HO_COMMIT:
						{
							//ReceiveRemoteMihN2nHoCommitRequest(node, msg);
							break;
						}
					case MIH_MN_HO_COMPLETE:
						{
							//ReceiveRemoteMihMnHoCompleteRequest(node, msg);
							break;
						}
					case MIH_N2N_HO_COMPLETE:
						{
							//ReceiveRemoteMihN2nHoCompleteRequest(node, msg);
							break;
						}
					case MIH_GET_INFORMATION:
						{
							//ReceiveRemoteMihGetInformationRequest(node, msg);
							break;
						}
				}
				break;
			}
		case RESPONSE:
			{
				switch (hdr->aid)
				{
					case MIH_REGISTER:
						{
							//ReceiveRemoteMihRegisterResponse(node, msg);
							break;
						}
					case MIH_DEREGISTER:
						{
							//ReceiveRemoteMihDeRegisterResponse(node, msg);
							break;
						}
					case MIH_EVENT_SUBSCRIBE:
						{
							//ReceiveRemoteMihEventSubscribeResponse(node, msg);
							break;
						}
					case MIH_EVENT_UNSUBSCRIBE:
						{
							//ReceiveRemoteMihEventUnsubscribeResponse(node, msg);
							break;
						}
					case MIH_LINK_GET_PARAMETERS:
						{
							ReceiveRemoteMihLinkGetParametersResponse(node, msg);
							break;
						}
					case MIH_CAPABILITY_DISCOVER:
						{
							//ReceiveRemoteMihCapabilityDiscoverResponse(node, msg);
							break;
						}
					case MIH_LINK_CONFIGURE_THRESHOLDS:
						{
							//ReceiveRemoteMihLinkConfigureThresholdsResponse(node, msg);
							break;
						}
					case MIH_LINK_ACTIONS:
						{
							//ReceiveRemoteMihLinkActionResponse(node, msg);
							break;
						}
					case MIH_NET_HO_CANDIDATE_QUERY:
						{
							//ReceiveRemoteMihNetHoCandidateQueryResponse(node, msg);
							break;
						}
					case MIH_MN_HO_CANDIDATE_QUERY:
						{
							//ReceiveRemoteMihMnHoCandidateQueryResponse(node, msg);
							break;
						}
					case MIH_N2N_HO_CANDIDATE_QUERY_RESOURCES:
						{
							//ReceiveRemoteMihN2nHoCandidateQueryResourcesResponse(node, msg);
							break;
						}
					case MIH_MN_HO_COMMIT:
						{
							//ReceiveRemoteMihMnHoCommitResponse(node, msg);
							break;
						}
					case MIH_NET_HO_COMMIT:
						{
							//ReceiveRemoteMihNetHoCommitResponse(node, msg);
							break;
						}
					case MIH_N2N_HO_COMMIT:
						{
							//ReceiveRemoteMihN2nHoCommitResponse(node, msg);
							break;
						}
					case MIH_MN_HO_COMPLETE:
						{
							//ReceiveRemoteMihMnHoCompleteResponse(node, msg);
							break;
						}
					case MIH_N2N_HO_COMPLETE:
						{
							//ReceiveRemoteMihN2nHoCompleteResponse(node, msg);
							break;
						}
					case MIH_GET_INFORMATION:
						{
							//ReceiveRemoteMihGetInformationResponse(node, msg);
							break;
						}
				}
				break;
			}
		case INDICATION:
			{
				switch (hdr->aid)
				{
					case MIH_LINK_DETECTED:
						{
							//ReceiveRemoteMihLinkDetectedIndication(node, msg);
							break;
						}
					case MIH_LINK_UP:
						{
							ReceiveRemoteMihLinkUpIndication(node, msg);
							break;
						}
					case MIH_LINK_DOWN:
						{
							//ReceiveRemoteMihLinkDownIndication(node, msg);
							break;
						}
					case MIH_LINK_PARAMETERS_REPORT:
						{
							ReceiveRemoteMihLinkParametersReportIndication(node, msg);
							break;
						}
					case MIH_LINK_GOING_DOWN:
						{
							ReceiveRemoteMihLinkGoingDownIndication(node, msg);
							break;
						}
					case MIH_LINK_HANDOVER_IMMINENT:
						{
							//ReceiveRemoteMihLinkHandoverImminentIndication(node, msg);
							break;
						}
					case MIH_LINK_HANDOVER_COMPLETE:
						{
							//ReceiveRemoteMihLinkHandoverCompleteIndication(node, msg);
							break;
						}
					case MIH_PUSH_INFORMATION:
						{
							//ReceiveRemoteMihPushInformation(node, msg);
							break;
						}
				}
				break;
			}
	};
}

void MihHandleRetransmissionTimerAlarm (Node* node,
										Message* timerMsg)
{
	int interfaceIndex;
	//int ttl = 1;
	NodeAddress sourceAddress;

	RetransmissionTimerInfo *timerInfo = (RetransmissionTimerInfo *) MESSAGE_ReturnInfo(timerMsg);
	int transactionId = timerInfo->transactionId;
	NodeAddress myMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, timerInfo->srcAddress);
	NodeAddress peerMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, timerInfo->destAddress);
	BOOL found = FALSE;

	clocktype currentTime = getSimTime(node);

	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionSource* transactionSrc;
	TransactionDest* transactionDest;

	if (mihf->transactionSrcList != NULL)
		transactionSrc = mihf->transactionSrcList->first;
	if (mihf->transactionDestList != NULL)
		transactionDest = mihf->transactionDestList->first;

	while (transactionSrc != NULL) //search first in source machine list
	{
		if ((transactionSrc->transactionId == transactionId) &&
			(transactionSrc->myMihfID == myMihfID) &&
			(transactionSrc->peerMihfID == peerMihfID))
		{
			found = TRUE;
			break;
		}
		transactionSrc = transactionSrc->next;
	}
	if (found)
	{
		switch(transactionSrc->ackReqState)
		{
			case ACK_REQ_WAIT_ACK:
				{
				if(transactionSrc->ackReqNumRetransmissions < MAX_RETRANSMISSION_NUMBER)
				{
				//retransmit message

				Message* msg = MESSAGE_Duplicate(node, transactionSrc->currentMessage);

				interfaceIndex = NetworkGetInterfaceIndexForDestAddress(node, timerInfo->destAddress);

				sourceAddress = NetworkIpGetInterfaceAddress(node, interfaceIndex); 

				MihSendPacketToIp(
								node,
								msg,
								sourceAddress,
								timerInfo->destAddress, 
								ANY_INTERFACE,
								IPDEFTTL);

				//
				transactionSrc->ackReqNumRetransmissions++;
				//schedule again retransmission timer

				//schedule timer message:
				if (transactionSrc->currentRetransmissionTimer != NULL)
				{
					Message* reTxMsg = MESSAGE_Duplicate(node, transactionSrc->currentRetransmissionTimer);
					MESSAGE_Send(node, reTxMsg, RETRANSMISSION_INTERVAL); //or RetransmissionInterval
				}
				}
				else
				{
					transactionSrc->ackReqState = ACK_REQ_FAILURE;
					transactionSrc->ackReqStatus = FAILURE;
					if(transactionSrc->state = STATE_WAIT_ACK)
					{
						transactionSrc->state = STATE_FAILURE;
						transactionSrc->transactionStatus = FAILURE; // Here we should free memory for this transaction
					}
				}
				}
			default:
				break; //we could free the message here, or it could be done outside this function
		}
	}
	else
	{ //search in destination machine list
		while (transactionDest != NULL) //search first in source machine list
		{
			if ((transactionDest->transactionId == transactionId) &&
				(transactionDest->myMihfID == myMihfID) &&
				(transactionDest->peerMihfID == peerMihfID))
			{
				found = TRUE;
				break;
			}
		transactionDest = transactionDest->next;
		}

		ERROR_Assert(found == TRUE, "No matching transaction");
		switch(transactionDest->ackReqState)
		{
			case ACK_REQ_WAIT_ACK:
				{
				if(transactionDest->ackReqNumRetransmissions < MAX_RETRANSMISSION_NUMBER)
				{
				//retransmit message

				Message* msg = MESSAGE_Duplicate(node, transactionDest->currentMessage);

				interfaceIndex = NetworkGetInterfaceIndexForDestAddress(node, timerInfo->destAddress);

				sourceAddress = NetworkIpGetInterfaceAddress(node, interfaceIndex); 

				MihSendPacketToIp(
								node,
								msg,
								sourceAddress,
								timerInfo->destAddress, 
								ANY_INTERFACE,
								IPDEFTTL);

				//
				transactionDest->ackReqNumRetransmissions++;
				//schedule again retransmission timer

				//schedule timer message:
				if (transactionDest->currentRetransmissionTimer != NULL)
				{
					Message* reTxMsg = MESSAGE_Duplicate(node, transactionDest->currentRetransmissionTimer);
					MESSAGE_Send(node, reTxMsg, RETRANSMISSION_INTERVAL); //or RetransmissionInterval
				}
				}
				else
				{
					transactionSrc->ackReqState = ACK_REQ_FAILURE;
					transactionSrc->ackReqStatus = FAILURE;
					if(transactionSrc->state = SEND_RESPONSE)
					{
						transactionSrc->state = STATE_FAILURE;
						transactionSrc->transactionStatus = FAILURE; // Here we should free memory for this transaction
					}
				}
				}
			default:
				break; //we could free the message here, or it could be done outside this function
		}
		}

	mihf->stats->numRetransmissionTimerAlarmsReceived++;
}
	

void MihHandleTransactionLifetimeAlarm(Node* node,
									   Message* timerMsg)
{
	TransactionLifetimeTimerInfo *timerInfo = (TransactionLifetimeTimerInfo *) MESSAGE_ReturnInfo(timerMsg);
	int transactionId = timerInfo->transactionId;
	NodeAddress myMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, timerInfo->srcAddress);
	NodeAddress peerMihfID = MAPPING_GetNodeIdFromInterfaceAddress(node, timerInfo->destAddress);

	BOOL found = FALSE;

	clocktype currentTime = getSimTime(node);

	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	TransactionSource* transactionSrc = NULL;
	TransactionDest* transactionDest = NULL;

	if (mihf->transactionSrcList != NULL)
		transactionSrc = mihf->transactionSrcList->first;
	if (mihf->transactionDestList != NULL)
		transactionDest = mihf->transactionDestList->first;
	
	while (transactionSrc != NULL) //search first in source machine list
	{
		if ((transactionSrc->transactionId == transactionId) &&
			(transactionSrc->myMihfID == myMihfID) &&
			(transactionSrc->peerMihfID == peerMihfID))
		{
			found = TRUE;
			break;
		}
		transactionSrc = transactionSrc->next;
	}
	if (found)
	{
		switch(transactionSrc->state)
		{
		case WAIT_RESPONSE_MSG:
			{
				if(transactionSrc->responseReceived == TRUE)
				{
					transactionSrc->state = STATE_SUCCESS;
					transactionSrc->transactionStatus = SUCCESS;
				}
				else
				{
					transactionSrc->state = STATE_FAILURE;
					transactionSrc->transactionStatus = FAILURE;
				}
				
			}
		default: 
			break;
		}
		RemoveTransactionSourceFromList(node, transactionSrc);
	}
	else
	{ //search in destination machine list
		while (transactionDest != NULL) 
		{
			if ((transactionDest->transactionId == transactionId) &&
				(transactionDest->myMihfID == myMihfID) &&
				(transactionDest->peerMihfID == peerMihfID))
			{
				found = TRUE;
				break;
			}
		transactionDest = transactionDest->next;
		}

	ERROR_Assert(found == TRUE, "No matching transaction");
	switch(transactionDest->state)
		{
		case WAIT_RESPONSE_MSG:
			{
				transactionDest->state = STATE_FAILURE;
				transactionDest->transactionStatus = FAILURE; //report this to upper entity, or update stat
				//
			}
		default:
			break;
		}

	RemoveTransactionDestFromList(node, transactionDest);
	}
	mihf->stats->numTransactionLifetimeAlarmsReceived++;

	
}

void MihHandleProtocolPacket(Node* node,
							 Message* msg,
							 NodeAddress sourceAddr,
							 NodeAddress destAddr,
							 int interfaceIndex)
{
	//MihHeaderType* mihPacket = (MihHeaderType*) (MESSAGE_ReturnPacket(msg) + sizeof(TransportUdpHeader));//?

	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	//switch(msg->eventType)
	//{
	//case MSG_NETWORK_FromTransportOrRoutingProtocol:
		//{
			ReceiveRemoteMihMessage (node, msg);
		//	break;
	//	}
	
	

		//..................................
	//}
}

void MihLayer(Node* node,
			  Message* msg)
{
	if (node->isMihfEnabled == FALSE)
	{
#if DEBUG
    {
        //char clockStr[24];
        printf("MIH Function: Node %u received MIH message but MIHF not enabled\n",
               node->nodeId);
		printf("    discarding message\n");
    }
#endif /* DEBUG */
		MESSAGE_Free(node, msg);
		return;
	}
	switch (MESSAGE_GetEvent(msg))
	{

	case MSG_MAC_Link_Going_Down_Indication:
		{
			ReceiveLinkGoingDownIndication(node, msg);
			break;
		}
	case MSG_MAC_Link_Parameters_Report_indication:
		{
			ReceiveLinkParametersReportIndication(node, msg);
			break;
		}
	case MSG_MAC_Get_Parameters_confirm:
		{
			ReceiveLinkGetParametersConfirm(node, msg);
			break;
		}
	case MSG_MAC_Link_Up_indication:
	{
		ReceiveLinkUpIndication(node, msg);
		break;
	}
	case MSG_MIH_RetransmissionTimerAlarm:
	{
		MihHandleRetransmissionTimerAlarm (node, msg);
		break;
	}
	case MSG_MIH_TransactionLifetimeAlarm:
	{
		MihHandleTransactionLifetimeAlarm(node, msg);
		break;
	}
	case MSG_MIH_LinkGoingDownTimerAlarm:
	{
		MihHandleLinkGoingDownTimerAlarm(node, msg);
		break;
	}
	case MSG_MAC_Link_Down_indication:
		{
		ReceiveLinkDownIndication(node,msg);
			break;
		}
		//...................................
	}
	MESSAGE_Free(node, msg);
}

void MihInitNbrMihfInfo(Node* node,
						const NodeInput* nodeInput,
						MihData* mihf)
{
    char buf[MAX_STRING_LENGTH];
    char errorString[MAX_STRING_LENGTH];
    BOOL retVal;
    int i;

	//first init list

	NbrMihfDataList* temp =  (NbrMihfDataList *) MEM_malloc(sizeof(NbrMihfDataList));
	temp->first = NULL;
	temp->last = NULL;
	temp->count = 0;

	mihf->nbrMihfList = temp;

	//read from config file

    IO_ReadString(
        node,
        node->nodeId,
        nodeInput,
        "MIHF-NEIGHBOR",
        &retVal,
        buf);

    if (retVal == FALSE)
    {
        return;
    }
    else
    {
        // For IO_GetDelimitedToken
        char* next;
        char* token;
        char* p;
        char nbrMihfString[MAX_STRING_LENGTH];
        char* delims = "{,} \n";
        char iotoken[MAX_STRING_LENGTH];
        char macProtocolName[MAX_STRING_LENGTH];
        BOOL wasFound;


        strcpy(nbrMihfString, buf);
        p = nbrMihfString;
        p = strchr(p, '{');
        BOOL minNodeIdSpecified = FALSE; 
        NodeAddress minNodeId = 0;

        if (p == NULL)
        {
            char errorBuf[MAX_STRING_LENGTH];
            sprintf(
                errorBuf, "Could not find '{' character:\n in %s\n",
                nbrMihfString);
            ERROR_Assert(FALSE, errorBuf);
        }
        else
        {
            token = IO_GetDelimitedToken(iotoken, p, delims, &next);

            if (!token)
            {
                char errorBuf[MAX_STRING_LENGTH];
                sprintf(errorBuf, "Can't find mihf list, e.g. "
                    "{ 1, 2, ... }:in \n  %s\n", nbrMihfString);
                ERROR_Assert(FALSE, errorBuf);
            }
            else
            {
                while (token)
                {
                    NodeAddress mihfId;

                    if (strncmp("thru", token, 4) == 0)
                    {
                        if (minNodeIdSpecified)
                        {
                            NodeAddress maxNodeId;

                            token = IO_GetDelimitedToken(iotoken,
                                                 next,
                                                 delims,
                                                 &next);

                            maxNodeId = (NodeAddress)atoi(token);

                            ERROR_Assert(maxNodeId > minNodeId,
                                "in { M Thru N}, N has to be"
                                "greater than M");

                            ERROR_Assert((mihf->nbrMihfList->count + maxNodeId -
                                     minNodeId) <=
                                     MIH_DEFAULT_MAX_NBR_MIHF,
                                     "Too many neighbor MIHFs, try to "
                                     "increase MIH_DEFAULT_MAX_NBR_MIHF");
							
                            mihfId = minNodeId + 1;
                            while (mihfId <= maxNodeId)
                            { //allocate memory for neighbor
								NbrMihData *nbrMihf =  (NbrMihData *) MEM_malloc(sizeof(NbrMihData));
								memset(nbrMihf, 0, sizeof(NbrMihData));

								nbrMihf->mihfId = mihfId;
								nbrMihf->interfaceAddress =
								    MAPPING_GetDefaultInterfaceAddressFromNodeId
								        (node,
								         mihfId);
								int interfaceIndex = MAPPING_GetInterfaceIdForDestAddress(node,
																							node->nodeId,
																							nbrMihf->interfaceAddress); 
								for (int i = 0; i< 20; i++)
								{
									nbrMihf->rssValue[i] = -1.7976931348623158e+308;
									nbrMihf->cinrValue[i] = -1.7976931348623158e+308;
									nbrMihf->measTime[i] = 0;
								}

								IO_ReadString(node,
												mihfId,
												ANY_IP,
												nodeInput,
												"MIHF-MAC-PROTOCOL",
												&wasFound,
												macProtocolName);
								if (!wasFound)
								{
								    sprintf(errorString,
									   "No MAC-PROTOCOL specified for interface %d of Node %d\n",
									    interfaceIndex,
									   mihfId);
								    ERROR_ReportError(errorString);
								}

								if (strcmp(macProtocolName, "MAC802.16") == 0)
									nbrMihf->macProtocol = MAC_PROTOCOL_DOT16;
								else if (strcmp(macProtocolName, "MACDOT11") == 0)
									nbrMihf->macProtocol = MAC_PROTOCOL_DOT11;
								else if (strcmp(macProtocolName, "CELLULAR-MAC") == 0)
									nbrMihf->macProtocol = MAC_PROTOCOL_CELLULAR;
							
								nbrMihf->next = NULL;

								if (mihf->nbrMihfList->count == 0)
								{
									mihf->nbrMihfList->first = nbrMihf;
									mihf->nbrMihfList->last = nbrMihf;
								}
								else
								{
									mihf->nbrMihfList->last->next = nbrMihf;
									mihf->nbrMihfList->last = nbrMihf;
								}
                                mihf->nbrMihfList->count++;
                                mihfId++;
							}

                            minNodeIdSpecified = FALSE;
                        }
                        else
                        {
                            char errorBuf[MAX_STRING_LENGTH];
                            sprintf(errorBuf, "Can't min node M, e.g. "
                                "when using { M thru n... }:in \n  %s\n",
                                 nbrMihfString);
                            ERROR_Assert(FALSE, errorBuf);
                        }
                    }
                    else
                    {
                        mihfId = (NodeAddress)atoi(token);

                        NbrMihData *nbrMihf =  (NbrMihData *) MEM_malloc(sizeof(NbrMihData));
								memset(nbrMihf, 0, sizeof(NbrMihData));

                        nbrMihf->mihfId = mihfId;
                        nbrMihf->interfaceAddress =
                            MAPPING_GetDefaultInterfaceAddressFromNodeId
                                (node,
                                 mihfId);
                       int interfaceIndex = MAPPING_GetInterfaceIdForDestAddress(node,
																				node->nodeId,
																				nbrMihf->interfaceAddress); 
						for (int i = 0; i<20; i++)
						{
							nbrMihf->rssValue[i] = -1.7976931348623158e+308;
							nbrMihf->cinrValue[i] = -1.7976931348623158e+308;
							nbrMihf->measTime[i] = 0;
						}

						IO_ReadString(node,
										mihfId,
										ANY_IP,
										nodeInput,
										"MIHF-MAC-PROTOCOL",
										&wasFound,
										macProtocolName);
						if (!wasFound)
						{
							sprintf(errorString,
							   "No MAC-PROTOCOL specified for interface %d of Node %d\n",
								interfaceIndex,
								mihfId);
							ERROR_ReportError(errorString);
						}
						if (strcmp(macProtocolName, "MAC802.16") == 0)
							nbrMihf->macProtocol = MAC_PROTOCOL_DOT16;
						else if (strcmp(macProtocolName, "MACDOT11") == 0)
							nbrMihf->macProtocol = MAC_PROTOCOL_DOT11;
						else if (strcmp(macProtocolName, "CELLULAR-MAC") == 0)
							nbrMihf->macProtocol = MAC_PROTOCOL_CELLULAR;
				
						nbrMihf->next = NULL;

						if (mihf->nbrMihfList->count == 0)
						{
							mihf->nbrMihfList->first = nbrMihf;
							mihf->nbrMihfList->last = nbrMihf;
						}
						else
						{
							mihf->nbrMihfList->last->next = nbrMihf;
							mihf->nbrMihfList->last = nbrMihf;
						}
                        mihf->nbrMihfList->count++;


                        if (mihf->nbrMihfList->count > MIH_DEFAULT_MAX_NBR_MIHF)
                        {
                            ERROR_Assert(FALSE, "Too many neighbor MIHFs, "
                                "try to increase"
                                "MIH_DEFAULT_MAX_NBR_MIHF");
                        }

                        minNodeId = mihfId;
                        minNodeIdSpecified = TRUE;
                    }

                    token = IO_GetDelimitedToken(iotoken,
                                                 next,
                                                 delims,
                                                 &next);
                } // while (token)
            } // if (token)
        } // if (p)
    } // if (retVal)

} // 

void FreeNbrMihfList(Node *node)
{
    NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

	NbrMihData *tempItem;
    NbrMihData *item;

    item = mihf->nbrMihfList->first;

    while (item)
    {
        tempItem = item;
        item = item->next;
        MEM_free(tempItem);
    }

    if (mihf->nbrMihfList != NULL)
    {
        MEM_free(mihf->nbrMihfList);
    }

    mihf->nbrMihfList = NULL;
}

void MihFunctionInit(Node* node,
					 const NodeInput* nodeInput)
{
	MihData* mihf;
	BOOL wasFound = FALSE;
	char stringVal[MAX_STRING_LENGTH] = {0, 0};
	char buff[MAX_STRING_LENGTH] = {0, 0};
	int intVal = 0;

	IO_ReadString(node,
                  node->nodeId,
                  nodeInput,
                  "MIH-ENABLED",
                  &wasFound,
                  stringVal);

    if (wasFound)
    {
        if (strcmp(stringVal, "YES") == 0)
        {
            node->isMihfEnabled = TRUE;
        }
        else if (strcmp(stringVal, "NO") != 0)
        {
            sprintf(buff,
                "Node Id: %d, MIH-ENABLED "
                    "shoudle be YES or NO. Use default value as NO.\n",
                    node->nodeId);
            ERROR_ReportWarning(buff);
        }
    }
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
	mihf = (MihData*) MEM_malloc (sizeof(MihData));
	memset(mihf, 0, sizeof(MihData));
	

	ipLayer->mihFunctionStruct = mihf;

	//mihf->numTransactionSrcDest = 0;
	//mihf->firstTransactionSrcDest = NULL;
	mihf->transactionSrcList = NULL;
	mihf->transactionDestList = NULL;
	mihf->transactionId = 0;
	mihf->nbrMihfList = NULL;
	mihf->isUpdatingParamList = FALSE;
	//mihf->numLinkGetParamsRcvd = 0;
	//mihf->numLinkGetParamsSent = 0;
	mihf->nodeType = MIH_OTHER;
	mihf->resourceManagerId = 0;
	mihf->targetMihfId = 0;
	mihf->targetMihfMacProtocol = MAC_PROTOCOL_NONE;
	mihf->targetMihfRss = -1.7976931348623158e+308;
	mihf->linkGoingDownTimer = NULL;
	mihf->currentPoaId = 0;
	mihf->currentPoaIdMacProtocol = MAC_PROTOCOL_NONE;
	mihf->prevPoaId = 0;
	mihf->prevPoaIdMacProtocol = MAC_PROTOCOL_NONE;

	for (int i=0; i<20; i++)
	{
		mihf->ongoingTransaction[i] = FALSE;
		mihf->ongoingTransactionId[i] = -1;
		mihf->myCinrValue[i] = -1.7976931348623158e+308;
		mihf->myRssValue[i] = -1.7976931348623158e+308;
		if(i<5)
			mihf->macProtocol[i] = MAC_PROTOCOL_NONE;
	}
	//and possibly other variables will follow

	IO_ReadString(node,
                  node->nodeId,
                  nodeInput,
                  "MIHF-NODE-TYPE",
                  &wasFound,
                  stringVal);

	 if (wasFound)
    {
        if (strcmp(stringVal, "MN") == 0)
        {
            mihf->nodeType = MIH_MOBILE_NODE;
			
			IO_ReadInt(
				node->nodeId,
				ANY_INTERFACE,
				nodeInput,
				"DUMMY-NUM-RADIO-INTERFACE",
				&wasFound,
				&intVal);

			for (int i = 0; i < intVal; i++)
			{
				IO_ReadStringInstance(
					node,
					node->nodeId,
					ANY_INTERFACE,
					nodeInput,
					"MIHF-MAC-PROTOCOL",
					i,                 // parameterInstanceNumber
					TRUE,              // fallbackIfNoInstanceMatch
					&wasFound,
					stringVal);

				for (int j = 0; j < node->numberInterfaces; j++)
				{
					if (((strcmp(stringVal, "MACDOT11") == 0) && (node->macData[j]->macProtocol == MAC_PROTOCOL_DOT11)) ||
						(((strcmp(stringVal, "MAC802.16") == 0)) && (node->macData[j]->macProtocol == MAC_PROTOCOL_DOT16)) ||
						(((strcmp(stringVal, "MAC802.15.4") == 0)) && (node->macData[j]->macProtocol == MAC_PROTOCOL_802_15_4)) ||
						(((strcmp(stringVal, "GSM") == 0)) && (node->macData[j]->macProtocol == MAC_PROTOCOL_GSM)) ||
						(((strcmp(stringVal, "CELLULAR-MAC") == 0)) && (node->macData[j]->macProtocol == MAC_PROTOCOL_CELLULAR)))
						
						mihf->macProtocol[i] = node->macData[j]->macProtocol;

				}
				
			}
			IO_ReadString(node,
                  node->nodeId,
                  nodeInput,
                  "IS-RESOURCE-MANAGER",
                  &wasFound,
                  stringVal);


			if (wasFound)
			{
				if (strcmp(stringVal, "YES") == 0)
				{
					MihInitNbrMihfInfo(node, nodeInput, mihf);
				}
				else if (strcmp(stringVal, "NO") != 0)
				{
					sprintf(buff,
						"Node Id: %d, IS-RESOURCE-MANAGER "
							"shoudle be YES or NO. Use default value as NO.\n",
							node->nodeId);
					ERROR_ReportWarning(buff);
				}
			}

        }
        else if (strcmp(stringVal, "POA") == 0)
        {
			mihf->nodeType = MIH_POA;
        }
		else
		{
			sprintf(buff,
                "Node Id: %d, MIH_NODE_TYPE "
                    "shoudle be MN or POA. Use default value as POA.\n",
                    node->nodeId);
            ERROR_ReportWarning(buff);
			mihf->nodeType = MIH_POA;
		}
    }
	
	IO_ReadString(node->nodeId,
				  ANY_ADDRESS,
				  nodeInput,
                  "RESOURCE-MANAGER",
                  &wasFound,
                  stringVal);

    if (wasFound)
    {
		mihf->resourceManagerId = (NodeAddress)atoi(stringVal);
		ERROR_Assert(atoi(stringVal)!=0, "RESOURCE-MANAGER shoud be a valid integer number"); //to be revised
	}

	MihInitNbrMihfInfo(node, nodeInput, mihf);
	MihInitStats (node, nodeInput);
	
}

void MihInitStats (Node* node, const NodeInput* nodeInput)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

    MihStats* mihStatistics = (MihStats*) MEM_malloc (sizeof (MihStats));
    memset(mihStatistics, 0, sizeof(MihStats));

    mihf->stats = mihStatistics;

	
}

void MihPrintStats (Node* node,
					MihData* mihf)
{
	
	//NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
   // MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;

    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "Number of Link_Parameters_Report indications received = %d",
            mihf->stats->numLinkParametersReportIndReceived);
    IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);
	sprintf(buf, "Number of MIH_Link_Parameters_Report indications sent = %d",
            mihf->stats->numMihLinkParametersReportIndSent);
    IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Parameters_Report indications received = %d",
            mihf->stats->numMihLinkParametersReportIndReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of Vertical Handovers performed = %d",
            mihf->stats->numVerticalHandovers);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of Link_Going_Down indications received = %d",
            mihf->stats->numLinkGoingDownIndReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of Link_Get_Parameters requests sent = %d",
            mihf->stats->numLinkGetParamReqSent);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of Link_Get_Parameters confirms received = %d",
            mihf->stats->numLinkGetParamRspReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);
	
	sprintf(buf, "Number of Link_Up indications received = %d",
	            mihf->stats->numLinkUpInd);
		IO_PrintStat(node,
	                 "Network",
	                 "802.21",
	                 ANY_DEST,
	                 -1,
	                 buf);


	sprintf(buf, "Number of MIH_Link_Going_Down indications received = %d",
            mihf->stats->numMihLinkGoingDownIndReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	
	sprintf(buf, "Number of MIH_Link_Going_Down indications sent = %d",
            mihf->stats->numMihLinkGoingDownIndSent);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Up indications sent = %d",
            mihf->stats->numMihLinkUpIndSent);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Up indications received = %d",
            mihf->stats->numMihLinkUpIndReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Get_Parameters requests sent = %d",
            mihf->stats->numMihLinkGetParamReqSent);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Get_Parameters responses sent = %d",
            mihf->stats->numMihLinkGetParamRspSent);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Get_Parameters requests received = %d",
            mihf->stats->numMihLinkGetParamReqReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	sprintf(buf, "Number of MIH_Link_Get_Parameters responses received = %d",
            mihf->stats->numMihLinkGetParamRspReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);
	
	sprintf(buf, "Number of RetransmissionTimer Alarms = %d",
            mihf->stats->numRetransmissionTimerAlarmsReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

	
	sprintf(buf, "Number of TransactionLifetime Alarms = %d",
            mihf->stats->numTransactionLifetimeAlarmsReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);


	sprintf(buf, "Number of Retransmission Timers cancelled = %d",
            mihf->stats->numRetransmissionTimersCancelled);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);
	
	sprintf(buf, "Number of ACK response messages received = %d",
            mihf->stats->numAckRspReceived);
	IO_PrintStat(node,
                 "Network",
                 "802.21",
                 ANY_DEST,
                 -1,
                 buf);

}

void MihFunctionFinalize (Node* node)
{
	NetworkDataIp* ipLayer = (NetworkDataIp*) node->networkData.networkVar;
    MihData* mihf = /*(MihData*)*/ ipLayer->mihFunctionStruct;
    if (node->isMihfEnabled == TRUE)
    	MihPrintStats(node, mihf);

	if (mihf->transactionSrcList != NULL)
		FreeTransactionSrcList(node);
	if (mihf->transactionDestList != NULL)
		FreeTransactionDestList(node);
	if (mihf->nbrMihfList != NULL)
		FreeNbrMihfList(node);

	MEM_free(mihf->stats);

	MEM_free(mihf); //is it correct?
}







			






	


