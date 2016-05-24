#ifndef MIH_H
#define MIH_H

#include "main.h"
#include "MIH_LINK_SAP.h"

#define RETRANSMISSION_INTERVAL	(1 * SECOND)		//have to decide on proper values for these
#define MAX_RETRANSMISSION_NUMBER 3 //or something like that
#define TRANSACTION_LIFETIME (10 * SECOND)
#define MIH_DEFAULT_MAX_NBR_MIHF 15
#define MEASUREMENT_VALID_TIME (6 * SECOND)

typedef enum {
	SERVICE_MANAGEMENT,
	EVENT_SERVICE,
	COMMAND_SERVICE,
	INFORMATION_SERVICE
} ServiceIdentifier;

typedef enum {
	REQUEST = 1,
	RESPONSE = 2,
	INDICATION = 3
} OperationCode;

typedef enum {
	MIH_CAPABILITY_DISCOVER = 11,
	MIH_REGISTER = 12,
	MIH_DEREGISTER = 13,
	MIH_EVENT_SUBSCRIBE = 14,
	MIH_EVENT_UNSUBSCRIBE = 15,
	MIH_LINK_DETECTED = 21,
	MIH_LINK_UP = 22,
	MIH_LINK_DOWN = 23,
	MIH_LINK_PARAMETERS_REPORT = 25,
	MIH_LINK_GOING_DOWN = 26,
	MIH_LINK_HANDOVER_IMMINENT = 27,
	MIH_LINK_HANDOVER_COMPLETE = 28,
	MIH_LINK_GET_PARAMETERS = 31,
	MIH_LINK_CONFIGURE_THRESHOLDS = 32,
	MIH_LINK_ACTIONS = 33,
	MIH_NET_HO_CANDIDATE_QUERY = 34,
	MIH_MN_HO_CANDIDATE_QUERY = 35,
	MIH_N2N_HO_CANDIDATE_QUERY_RESOURCES = 36,
	MIH_MN_HO_COMMIT = 37,
	MIH_NET_HO_COMMIT = 38,
	MIH_N2N_HO_COMMIT = 39,
	MIH_MN_HO_COMPLETE = 310,
	MIH_N2N_HO_COMPLETE = 311,
	MIH_GET_INFORMATION = 41,
	MIH_PUSH_INFORMATION = 42
}ActionIdentifier;

typedef struct mih_msg_id {
	ServiceIdentifier sid;	//Service Identifier
	OperationCode opcode;	//Operation Code
	ActionIdentifier aid;	//Action Identifier
} MihMessageId;

typedef enum mih_node_type {
	MIH_MOBILE_NODE,
	MIH_POA,
	MIH_OTHER
} MihNodeType;


typedef struct mih_header_str {
	//unsigned short version = 1;  //This is the first version of MIH protocol; it's a const for now
	BOOL ackRsp; //ackRsp bit
	BOOL ackReq;
	//BOOL uir = 0;	// For now we can't make MIIS request in unauthenticated state
	//BOOL moreFrag;
	//int fragNumber;
	//MihMessageId messageId;
	ServiceIdentifier sid;	//Service Identifier
	OperationCode opcode;	//Operation Code
	ActionIdentifier aid;	//Action Identifier
	int transactionId;
	clocktype timeSent; //time the message was sent
	clocktype transactionInitiated;
	NodeAddress srcAddr; 
	NodeAddress destAddr;
	char* payload; //pointer to the information carried by the mih message
	int payloadSize;
} MihHeaderType;

typedef enum {	//Transaction status for acknowledgment service
	NOT_ACTIVE,
	ONGOING,
	SUCCESS,
	FAILURE
} Status;

typedef enum ack_req_state_str {
	ACK_REQ_NOT_STARTED,
	ACK_REQ_WAIT_ACK,
	RETRANSMIT,
	ACK_REQ_SUCCESS,
	ACK_REQ_FAILURE
} AckRequestorState;

typedef enum ack_rsp_state_str {
	ACK_RSP_NOT_STARTED,
	RETURN_ACK,
	PIGGYBACKING,
	RETURN_DUPLICATE
} AckResponderState;

typedef enum transaction_source_state_str {
	STATE_INIT,
	STATE_WAIT_ACK,
	WAIT_RESPONSE_MSG,
	PROCESS_MSG,
	SEND_RESPONSE,
	STATE_FAILURE,
	STATE_SUCCESS
} TransactionState;

typedef struct mn_ho_commit_info_str {
	LINK_TYPE linkType;
	TGT_NET_INFO *tgtNetInfo;
} MnHoCommitReqInfo;

typedef struct retransmission_timer_msg_str {
	int transactionId;
	NodeAddress srcAddress;
	NodeAddress destAddress;
}RetransmissionTimerInfo;

typedef struct transaction_lifetime_timer_msg_str {
	int transactionId;
	NodeAddress srcAddress;
	NodeAddress destAddress;
} TransactionLifetimeTimerInfo;

typedef struct mih_link_act_rsp_str {
	STATUS status;
	L_LINK_ACTION_RSP* linkActionsResultList;
} MihLinkActionsRsp;

typedef struct mih_report_best_poa_str {
	NodeAddress bestPoaId;
	double rssValue;
	double cinrValue;
	MAC_PROTOCOL macProtocol;
	NodeAddress mobileNodeId;
} MihReportBestPoa;

typedef struct transaction_src_str {
	BOOL responseReceived;
	BOOL startAckRequestor;
	BOOL startAckResponder;
	BOOL isBroadcast;
	int transactionId;
	Status ackReqStatus;
	AckRequestorState ackReqState;
	AckResponderState ackRspState;
	int ackReqNumRetransmissions;
	Status transactionStatus;
	TransactionState state;
	clocktype transactionStopWhen;
	Message* currentMessage; //saves a copy of the current MIH message for ack service
	Message* currentRetransmissionTimer; //pointer to retransmission timer for future cancellation

	NodeAddress myMihfID;
	NodeAddress peerMihfID;
	
	//transaction_src_dest_str *prevTransactionSrcDest;
	transaction_src_str *next;
} TransactionSource;

typedef struct transaction_dest_str {
	BOOL startAckRequestor;
	BOOL startAckResponder;
	BOOL isBroadcast;
	int transactionId;
	Status ackReqStatus;
	AckRequestorState ackReqState;
	AckResponderState ackRspState;
	int ackReqNumRetransmissions;
	Status transactionStatus;
	TransactionState state;
	clocktype transactionStopWhen;
	Message* currentMessage; //saves a copy of the current MIH message for ack service
	Message* currentRetransmissionTimer; //pointer to retransmission timer for future cancellation

	NodeAddress myMihfID;
	NodeAddress peerMihfID;
	
	//transaction_src_dest_str *prevTransactionSrcDest;
	transaction_dest_str *next;
} TransactionDest;

typedef struct transaction_src_list_str {
	TransactionSource *first;
	TransactionSource *last;
	int count;
} TransactionSourceList;

typedef struct transaction_dest_list_str {
	TransactionDest *first;
	TransactionDest *last;
	int count;
} TransactionDestList;

typedef struct nbr_mihf_data_str {
	NodeAddress mihfId;
	NodeAddress interfaceAddress;
	nbr_mihf_data_str *next;
	//this is where we have to add link parameters like RSS etc. for neighbouring nodes if this node is RM
	double rssValue[20];
	double cinrValue[20];
	clocktype measTime[20];
	//just these for now
	MAC_PROTOCOL macProtocol;
} NbrMihData;

typedef struct nbr_mihf_data_list_str {
	NbrMihData *first;
	NbrMihData *last;
	int count;
} NbrMihfDataList;

typedef struct mih_stats_str {
	int numHorizHandovers;
	int numVerticalHandovers;
	//...........
	int numLinkParametersReportIndReceived;
	int numMihLinkParametersReportIndSent; 
	int numMihLinkParametersReportIndReceived;
	int numRetransmissionTimerAlarmsReceived;
	int numRetransmissionTimerAlarmsSent;
	int numTransactionLifetimeAlarmsReceived;
	int numMihLinkGoingDownIndSent;
	int numMihLinkGoingDownIndReceived;
	int numMihLinkUpIndSent;
	int numMihLinkUpIndReceived;
	int numLinkGoingDownIndReceived;
	int numLinkDownIndReceived;
	int numRetransmissionTimersCancelled;
	int numAckRspReceived;
	int numLinkGetParamReqSent;
	int numLinkGetParamRspReceived;
	int numMihLinkGetParamReqSent;
	int numMihLinkGetParamRspSent;
	int numMihLinkGetParamReqReceived;
	int numMihLinkGetParamRspReceived;
	int numMihMnHoCommitReqSent;
	int numMihMnHoCommitReqReceived;
	int numMihMnHoCommitRspSent;
	int numMihMnHoCommitRspReceived;
	int numMihLinkActionsReqSent;
	int numMihLinkActionsRspSent;
	int numLinkUpInd;

} MihStats;

typedef struct mih_function_str {

	//TransactionSourceDest *firstTransactionSrcDest; //pointer to the first transaction source or dest maintained at the node
	TransactionSourceList *transactionSrcList; //pointer to list of transaction source or dest maintained at the node
	TransactionDestList *transactionDestList; //pointer to list of transaction source or dest maintained at the node
	NbrMihfDataList *nbrMihfList;
	NodeAddress resourceManagerId;
	
	NodeAddress currentPoaId;
	MAC_PROTOCOL currentPoaIdMacProtocol;

	NodeAddress prevPoaId;
	MAC_PROTOCOL prevPoaIdMacProtocol;

	BOOL isUpdatingParamList;
	//int numLinkGetParamsSent;
	//int numLinkGetParamsRcvd;
	double myRssValue[20];
	double myCinrValue[20];
	BOOL ongoingTransaction[20];
	int ongoingTransactionId[20];
	Message *linkGoingDownTimer;

	MihNodeType nodeType;
	MAC_PROTOCOL macProtocol[5];

	NodeAddress targetMihfId;
	MAC_PROTOCOL targetMihfMacProtocol;
	double targetMihfRss;
	
	int transactionId; //will be 0 at first, then will increment once a new transaction is created
	MihStats *stats;

} MihData;

MAC_PROTOCOL MihGetCurrentPoaMacProtocol(Node* node);

TransactionSource* CreateTransactionSrc (Node* node,
										 NodeAddress srcAddress,
										 NodeAddress destAddress,
										 int transactionId,
										 BOOL isMulticast,
										 BOOL ackReq);

TransactionDest* CreateTransactionDest (Node* node,
										 NodeAddress srcAddress,
										 NodeAddress destAddress,
										 int transactionId,
										 BOOL isMulticast,
										 BOOL ackReq,
										 clocktype transactionLifetime);

void AddTransactionSourceToList (Node* node,
								 MihData* mihf,
								 NodeAddress srcAddress,
								 NodeAddress destAddress,
								 int transactionId,
								 BOOL isMulticast,
								 BOOL ackReq);

void AddTransactionDestToList (Node* node,
							   MihData* mihf,
								 NodeAddress srcAddress,
								 NodeAddress destAddress,
								 int transactionId,
								 BOOL isMulticast,
								 BOOL ackReq,
								 clocktype transactionLifetime);

void RemoveTransactionSourceFromList(Node* node,
									 TransactionSource *listItem);

void RemoveTransactionDestFromList(Node* node,
								   TransactionDest *listItem);

void FreeTransactionSrcList(Node *node);

void FreeTransactionDestList(Node *node);

void MihSendPacketToIp(Node* node,
					   Message* mihMsg,
					   NodeAddress sourceAddress,
					   NodeAddress destinationAddress,
					   int outgoingInterface,
					   int ttl);

void TransactionSrcInitTransaction(Node* node,
								   Message* mihMsg,
								   NodeAddress srcAddress,
								   NodeAddress destAddress,
								   int transactionId,
								   BOOL isMulticast,
								   BOOL ackReq);

void TransactionDestInitTransaction(Node* node,
								   Message* mihMsg,
								   NodeAddress srcAddress,
								   NodeAddress destAddress,
								   int transactionId,
								   BOOL isMulticast,
								   BOOL ackReq,
								   clocktype transactionLifetime);

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
								   BOOL ackRsp);

void ReceiveLinkParametersReportIndication(Node* node,
										   Message* msg);

void ReceiveLinkGoingDownIndication(Node* node,
										   Message* msg);

void SendGenericLinkGetParametersReq(Node* node,
								  MihData* mihf,
								  NodeAddress srcMihfID,
								  NodeAddress destMihfID,
								  LINK_STATUS_REQ* getStatusReqSet,
								  int transactionId,
								  clocktype transactionInitiated);
/*
void RMTriggerHandover(Node* node,
					   MihData* mihf,  //for now
					   NodeAddress destMihfId
					   );
*/
void RMUpdateBestPoA(Node* node, MihData *mihf, NodeAddress srcMihfID);

void RMProcessLinkParamReport(Node* node,
							  MihData* mihf,
							  L_LINK_PARAM_RPT *linkParamRptList,
							  NodeAddress srcMihfID,
							  NodeAddress poaId);

void RMProcessLinkUpInd (Node* node,
						   MihData* mihf,
						   link_up_str *up,
						   clocktype timeSent,
						   NodeAddress srcMihfID);

void SendLinkGetParametersReq(Node* node,
						   MihData* mihf,
						   NodeAddress mihfID,
						   int transactionId,
						   clocktype transactionInitiated);

void ReceiveLinkGetParametersConfirm(Node* node,
									 Message* msg);

void SendLinkActionsReq(Node* node,
						MihData* mihf,
						NodeAddress MihfID,
						LINK_ADDR* linkAddr,
						LINK_AC_TYPE linkActionType,
						LINK_AC_ATTR linkActionAttribute
						);


void ReceiveRemoteMihLinkParametersReportIndication(Node* node,
											  Message* msg);

void ReceiveRemoteMihLinkGoingDownIndication(Node* node,
											  Message* msg);

void ReceiveRemoteMihLinkUpIndication(Node* node,
										Message* msg);

void ReceiveRemoteMihLinkGetParametersRequest(Node* node,
										Message* msg);

void ReceiveRemoteMihLinkGetParametersResponse(Node* node, 
										 Message* msg);

void SendRemoteMihLinkGoingDownIndication (Node* node,
									 MihData* mihf,
									 down_reason* reason,
									 NodeAddress destAddr);

void SendRemoteMihLinkUpIndication (Node* node,
									 MihData* mihf,
									 link_up_str* up,
									 NodeAddress destAddr);

void SendRemoteMihLinkGetParametersResponse(Node *node,
									 MihData* mihf,
									 link_params* confirm
									 //and other arguments
									 );

void SendRemoteMihLinkGetParametersRequest(Node *node,
									 MihData* mihf,
									 NodeAddress destAddr,
									 NodeAddress srcMihfID,
									 int transactionId,
									 UNSIGNED_INT_1 paramType[]//and other arguments
									 );

void SendRemoteMihLinkActionsRequest(Node* node,
									 MihData* mihf,
									 NodeAddress destMihfID,
									 int transactionId,
									 L_LINK_ACTION_REQ* linkActionsList);

void SendRemoteMihLinkActionsResponse(Node* node,
									  MihData* mihf,
									  NodeAddress destMihfID,
									  int transactionId,
									  clocktype transactionInitiated,
									  STATUS status,
									  L_LINK_ACTION_RSP* linkActionsResultList
									  //and possibly other arguments
									  );
void ReceiveMihMnHoCommitRequest(Node* node,
				Message* msg);

void SendRemoteMihMnHoCommitRequest(Node *node,
									 MihData* mihf,
									 NodeAddress destAddr,
									 MnHoCommitReqInfo *mnHoCommitReq,
									 int transactionId
									 //and other arguments
									 );
void RMSendMihMnHoCommitRequest (Node* node,
							   MihData* mihf,
							   NodeAddress tgtMihfId
							   //maybe some more params
							   );

void MihHandleLinkGoingDownTimerAlarm (Node* node,
		Message* msg);

void RMProcessLinkGoingDownInd (Node* node,
									MihData* mihf,
									down_reason* reason,
									clocktype timeSent,
									NodeAddress srcMihfID);

void ReceiveRemoteMihMessage (Node* node,
							  Message* msg);

void MihHandleRetransmissionTimerAlarm (Node* node,
										Message* timerMsg);

void MihHandleTransactionLifetimeAlarm(Node* node,
									   Message* timerMsg);


void MihLayer(Node* node,
			  Message* msg);

void MihHandleProtocolPacket(Node* node,
							 Message* msg,
							 NodeAddress sourceAddr,
							 NodeAddress destAddr,
							 int interfaceIndex);

void MihFunctionInit(Node* node,
					 const NodeInput* nodeInput);

void MihFunctionFinalize (Node* node);

void MihInitStats (Node* node, const NodeInput* nodeInput);

void MihPrintStats (Node* node,	MihData* mihf);


#endif


	 

