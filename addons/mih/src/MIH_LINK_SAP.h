#ifndef MIH_LINK_SAP_H
#define MIH_LINK_SAP_H

#include "main.h"
#include "api.h"
#include "MIH_Data_types.h"

    // ************************************************************************
    //
    //  
    //
    // ************************************************************************
typedef struct parameters_signal_structure{
	NodeAddress 		dest;
	L_LINK_PARAM_RPT* 	linkParamRptList;
	LINK_TUPLE_ID*    	linkId;
	unsigned char		poaId[6];
}param_signal;

typedef struct new_network_parameters {
	OCTET_STRING network_id;
	SIG_STRENGTH signal_strength;
	NodeAddress dest;
}new_network;

typedef struct link_up_structure {
	    LINK_TUPLE_ID*		LinkIdentifier;
		LINK_ADDR*			OldAccessRouter;
		LINK_ADDR *			NewAccessRouter;
		IP_RENEWAL_FLAG		IpRenewalFlag;
		IP_MOB_MGMT			MobilityManagementSupport;
		NodeAddress			dest;
}link_up_str;

typedef struct link_down_structure{
	
	LINK_TUPLE_ID*    LinkIdentifier;
	LINK_ADDR*        OldAccessRouter;
	LINK_DN_REASON   ReasonCode;
	NodeAddress		srcMihfID;
}link_down_str;

//Parameters cinr and rss

typedef struct CinrAndRss
{
   unsigned char MacAddress[6];
   double rss;
   double cinr;
   CinrAndRss* next;
} CinrAndRss_str;

typedef struct CinrAndRss_list
{
   CinrAndRss_str* first;
   CinrAndRss_str* last;
   int count;
} CinrAndRss_list_str;

typedef struct link_PDU_tr_stat_structure{
	BOOL succes;
	BOOL failure;
	NodeAddress dest;
}PDU_status;

typedef struct link_capability_disc_structure{
	UNSIGNED_INT_1 status;
	BITMAP_32 l_ev_list;
	BITMAP_32 l_cmd_list;
	NodeAddress dest;
}link_list;

typedef struct link_event_subscribe_structure{
	UNSIGNED_INT_1 status;
	BITMAP_32 l_ev_list;
	NodeAddress dest;
}ev_list;

typedef struct link_get_param_structure{
// list_param;
	BITMAP_16 list_stat;
	BITMAP_16 list_descr;
}ink_param;

typedef struct link_going_down_structure{
	LINK_TUPLE_ID* linkId;
	UNSIGNED_INT_2 timeInt;
	LINK_GD_REASON lgdReason;
	NodeAddress srcMihfID;
}down_reason;


typedef struct link_handover_im_structure{
	LINK_TUPLE_ID*    OldLinkIdentifier;
	LINK_TUPLE_ID*    NewLinkIdentifier;
    LINK_ADDR*        OldAccessRouter;
	LINK_ADDR*        NewAccessRouter;
}link_charc_im;


typedef struct link_handover_comp_structure{
	

	STATUS           LinkHandoverStatus;
	LINK_TUPLE_ID*    OldLinkIdentifier;
	LINK_TUPLE_ID*    NewLinkIdentifier;
    LINK_ADDR*        OldAccessRouter;
	LINK_ADDR*        NewAccessRouter;
	
}link_charc_comp;

typedef struct parameters_unsubscribe_confirm{
	unsigned char status;
	NodeAddress dest;
	unsigned int events;
}link_unsub;


typedef struct parameters_get_confirm{
	STATUS status;
	l_LINK_PARAM* linkParamStatusList;
	l_LINK_STATES_RSP* linkStatesRsp;
	l_LINK_DESC_RSP* linkDescRsp;
	NodeAddress destMihfID;
	NodeAddress srcMihfID;
	int transactionId;
	clocktype transactionInitiated;
}link_params;

typedef struct link_parameters_str {
	LINK_STATUS_REQ *statusReq;
	NodeAddress srcMihfID;
	NodeAddress destMihfID;
	int transactionId;
	clocktype transactionInitiated;
} link_parameters;

typedef struct local_link_parameters_str {
	L_LINK_PARAM_TYPE* linkParamReq;
	LINK_STATES_REQ linkStatesReq;
	LINK_DESC_REQ linkDescReq;
	NodeAddress srcMihfID;
	NodeAddress destMihfID;
	int transactionId;
	clocktype transactionInitiated;
} local_link_parameters;


typedef struct parameters_configure_threshold{
	STATUS Status;
	L_LINK_CFG_STATUS* LinkConfigureStatusList; 
}config_thresh;
	

typedef struct parameters_action_confirm{
	STATUS                Status;
	L_LINK_SCAN_RSP*       ScanResponseSet;
	LINK_AC_RESULT        LinkActionResult;
}action_conf;	


typedef struct link_event_subscribe_str{
	LINK_EVENT_LIST*  list;
}subscribe_event;


typedef struct link_event_unsubscribe_structure{
	LINK_EVENT_LIST*  list;
}unsubscribe_event;

typedef struct parameters_action_req{
		NodeAddress mobileNodeId;
	    LINK_ACTION* LinkAction;
		UNSIGNED_INT_2 ExecutionDelay;
		LINK_ADDR*  PoALinkAddress ;
}action_req;
    
    void Link_Detected_indication (
Node*            node,
       LINK_DET_INFO* LinkDetectedInfo);

	void Link_Up_indication(
		Node*				node,
		LINK_TUPLE_ID*		LinkIdentifier,
		LINK_ADDR*			OldAccessRouter,
		LINK_ADDR *			NewAccessRouter,
		IP_RENEWAL_FLAG		IpRenewalFlag,
		IP_MOB_MGMT			MobilityManagementSupport/*,
		NodeAddress			destAddress*/);

	void Link_Down_indication(Node*            node,
		LINK_TUPLE_ID*    LinkIdentifier,
		LINK_ADDR*        OldAccessRouter,
		LINK_DN_REASON   ReasonCode /*,
		NodeAddress destinationAddress */);

	void Link_Parameters_Report_indication(Node*            node,
		 LINK_TUPLE_ID*    LinkIdentifier,
		 L_LINK_PARAM_RPT* LinkParametersReportList,
		 NodeAddress		destinationAddress,
		 unsigned char*				poaId);

	void Link_Going_Down_indication(
		Node*            node,
		LINK_TUPLE_ID*   LinkIdentifier,
		UNSIGNED_INT_2  TimeIinterval, 
		LINK_GD_REASON  LinkGoingDownReason /*,
		NodeAddress destinationAddress*/);

	void Link_Handover_Iminent_indication(
Node*            node,
		LINK_TUPLE_ID*    OldLinkIdentifier,
		LINK_TUPLE_ID*    NewLinkIdentifier,
		LINK_ADDR*        OldAccessRouter,
		LINK_ADDR*        NewAccessRouter /*,
		NodeAddress destinationAddress*/);

    void Link_Handover_Complete_indication(
Node*            node,
		LINK_TUPLE_ID*    OldLinkIdentifier,
		LINK_TUPLE_ID*    NewLinkIdentifier,
		LINK_ADDR*        OldAccessRouter,
		LINK_ADDR*        NewAccessRouter ,
		STATUS           LinkHandoverStatus /*,
		NodeAddress destinationAddress */);

	void Link_PDU_Transmit_Status_indication(
Node*            node,
		LINK_TUPLE_ID*   LinkIdentifier,
		UNSIGNED_INT_2  PacketIdentifier, 
		BOOL         TransmissionStatus,
		NodeAddress destinationAddress);

	void Link_Capability_Discover_confirm (
Node*            node,
		LINK_EVENT_LIST       SupportedLinkEventList,
		LINK_CMD_LIST    SupportedLinkCommandList,
		NodeAddress destinationAddress);
	
	void Link_Event_Subscribe_confirm(
Node*            node,
		STATUS             Status,
		LINK_EVENT_LIST       ResponseLinkEventList,
		NodeAddress destinationAddress);

	void Link_Event_Unsubscribe_confirm(
Node*            node,
		STATUS             Status,
		LINK_EVENT_LIST       ResponseLinkEventList,
		NodeAddress destinationAddress);

	void Link_Get_Parameters_confirm(Node* node,
									 STATUS Status,
									 l_LINK_PARAM* LinkParametersStatusList,
									 l_LINK_STATES_RSP* LinkStatesResponse,
									 l_LINK_DESC_RSP* LinkDescriptorsResponse,
									 NodeAddress destMihfID,
									 NodeAddress srcMihfID,
									 int transactionId,
									 clocktype transactionInitiated);


	void Link_Configure_Thresholds_confirm(
        Node*            node,
		STATUS           Status,
		L_LINK_CFG_STATUS*     LinkConfigureStatusList/*,
		NodeAddress destinationAddress*/);
	
	
	void Link_Action_confirm(
Node*            node,
		STATUS                Status,
		L_LINK_SCAN_RSP*       ScanResponseSet,
		LINK_AC_RESULT        LinkActionResult/*, 
		NodeAddress destinationAddress */);

    void Link_Capability_Discover_request (
Node*            node,
NodeAddress destinationAddress);
	
	void Link_Event_Subscribe_request(
Node*            node,
NodeAddress destinationAddress,
		LINK_EVENT_LIST* RequestedLinkEventList);

	void Link_Event_Unsubscribe_request(
Node*            node,
NodeAddress destinationAddress,
		LINK_EVENT_LIST* RequestedLinkEventList);

	void Link_Get_Parameters_request(Node *node,
		L_LINK_PARAM_TYPE*     LinkParametersRequest,
		LINK_STATES_REQ       LinkStatesRequest,
		LINK_DESC_REQ         LinkDescriptorsRequest,
		NodeAddress mihfID,
		int transactionId,
		clocktype transactionInitiated);

	void Link_Configure_Thresholds_request(
Node*            node,
L_LINK_CFG_STATUS*     LinkConfigureStatusList/*,
NodeAddress destinationAddress*/);
	
	

	void Link_Action_request(
Node*            node,
NodeAddress destinationAddress,
		LINK_ACTION*          LinkAction,
		UNSIGNED_INT_2       ExecutionDelay,
		LINK_ADDR *           PoALinkAddress );

#endif



