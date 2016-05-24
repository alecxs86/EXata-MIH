/*
 * mac_dot11-mih.cpp
 *
 *  Created on: 07.12.2010
 *      Author: alecxs
 */
#include "mac_dot11-mih.h"
#include "mac_dot11-mgmt.h"


void  MacDot11Sta_Link_Up_indication(Node* node, MacDataDot11* dot11)
{
	// DOT11_ApInfo* dot11Sta = (DOT11_ApInfo*) dot11->associatedAP;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
    memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof(LINK_ADDR));
    memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

    OldAccessRouter->selector=0;

	DOT11_ManagementVars* mngmtVars =
        (DOT11_ManagementVars *) dot11->mngmtVars;


    for (int j=0; j<5; j++)
    	OldAccessRouter->ma.octet_string[j]= mngmtVars->prevApAddr.byte[j];

    OldAccessRouter->ma.addr_type = 6; //IEEE 802 address type


	//DOT11_ApInfo* apInfo=(DOT11_ApInfo*)MEM_malloc(sizeof( DOT11_ApInfo));
    //memset(apInfo, 0, sizeof(DOT11_ApInfo));

	LinkIdentifier->lid.lt=19;
if (dot11->stationAssociated)
	for (int j=0; j<5; j++)
	{
		LinkIdentifier->lid.la.ma.octet_string[j]= dot11->associatedAP->bssAddr.byte[j];
		LinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];
	}

	LinkIdentifier->lid.la.ma.addr_type = 6;
	LinkIdentifier->lad.ma.addr_type = 6;

	//apInfo = MacDot11ManagementFindBestAp(node, dot11);


    NewAccessRouter->selector=0;

	if (dot11->stationAssociated)
		for (int j=0; j<5; j++)
			NewAccessRouter->ma.octet_string[j]= dot11->associatedAP->bssAddr.byte[j];

    NewAccessRouter->ma.addr_type = 6;

	IP_MOB_MGMT MobilityManagementSupport=1; //Mobile IPv4 supported

	Link_Up_indication( node, LinkIdentifier,OldAccessRouter,NewAccessRouter,FALSE,MobilityManagementSupport);
    dot11->numLink_Up_indication++;

}

void  MacDot11Ap_Link_Up_indication(Node* node, MacDataDot11* dot11)
{
	// DOT11_ApInfo* dot11Sta = (DOT11_ApInfo*) dot11->associatedAP;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
    memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof(LINK_ADDR));
    memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

    OldAccessRouter->selector=0;

    if (dot11->apVars->apStationList->first != NULL)
    {
    for (int j=0; j<5; j++)
    	OldAccessRouter->ma.octet_string[j]= dot11->apVars->apStationList->first->data->macAddr.byte[j];
    }

    OldAccessRouter->ma.addr_type = 6; //IEEE 802 address type


	DOT11_ApInfo* apInfo=(DOT11_ApInfo*)MEM_malloc(sizeof( DOT11_ApInfo));
    memset(apInfo, 0, sizeof(DOT11_ApInfo));

	LinkIdentifier->lid.lt=19;

	for (int j=0; j<5; j++)
	{
		LinkIdentifier->lid.la.ma.octet_string[j]= dot11->bssAddr.byte[j];
		LinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];
	}

	OldAccessRouter->ma.addr_type = 6;
	LinkIdentifier->lid.la.ma.addr_type = 6;
	LinkIdentifier->lad.ma.addr_type = 6;

//	apInfo = MacDot11ManagementFindBestAp(node, dot11);


    NewAccessRouter->selector=0;

    for (int j=0; j<5; j++)
    	NewAccessRouter->ma.octet_string[j]= dot11->bssAddr.byte[j];

    NewAccessRouter->ma.addr_type = 6;

	IP_MOB_MGMT MobilityManagementSupport=1; //Mobile IPv4 supported

	Link_Up_indication( node, LinkIdentifier,OldAccessRouter,NewAccessRouter,FALSE,MobilityManagementSupport);
    dot11->numLink_Up_indication++;


}

void MacDot11Sta_Send_LinkHandover_IminentIndication(Node* node, MacDataDot11* dot11)
{
	//MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;

	LINK_TUPLE_ID*    OldLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(OldLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));


	LINK_TUPLE_ID*    NewLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(NewLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	LINK_ADDR* OldAccessRouter= (LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	OldAccessRouter->selector=1;

	OldLinkIdentifier->lid.lt=19;
	for (int j=0; j<5; j++)
	{
		OldAccessRouter->ma.octet_string[j]= dot11->bssAddr.byte[j];
		OldLinkIdentifier->lid.la.ma.octet_string[j]= dot11->bssAddr.byte[j];
		OldLinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];
	}

	DOT11_ApInfo* apInfo=(DOT11_ApInfo*)MEM_malloc(sizeof( DOT11_ApInfo));
		memset(apInfo, 0, sizeof(DOT11_ApInfo));

	apInfo = MacDot11ManagementFindBestAp(node, dot11);

	NewAccessRouter->selector=1;

	NewLinkIdentifier->lid.lt=19;
	for (int j=0; j<5; j++)
	{
		NewAccessRouter->ma.octet_string[j]= apInfo->bssAddr.byte[j];
		NewLinkIdentifier->lid.la.ma.octet_string[j]= apInfo->bssAddr.byte[j];
		NewLinkIdentifier->lad.ma.octet_string[j]= apInfo->bssAddr.byte[j];
	}

	Link_Handover_Iminent_indication(node, OldLinkIdentifier,NewLinkIdentifier,OldAccessRouter,NewAccessRouter);
	dot11->numLinkHandoverIminent++;


}

void MacDot11Sta_Send_LinkHandover_CompleteIndication(Node* node, MacDataDot11* dot11)
{
	//MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;

	LINK_TUPLE_ID*    OldLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(OldLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));


	LINK_TUPLE_ID*    NewLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(NewLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	LINK_ADDR* OldAccessRouter= (LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	STATUS LinkHandoverStatus;

	OldAccessRouter->selector=1;

	OldLinkIdentifier->lid.lt=19;
	for (int j=0; j<5; j++)
	{
		OldAccessRouter->ma.octet_string[j]= dot11->bssAddr.byte[j];
		OldLinkIdentifier->lid.la.ma.octet_string[j]= dot11->bssAddr.byte[j];
		OldLinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];

	}

	DOT11_ApInfo* apInfo=(DOT11_ApInfo*)MEM_malloc(sizeof( DOT11_ApInfo));
	memset(apInfo, 0, sizeof(DOT11_ApInfo));

	apInfo = MacDot11ManagementFindBestAp(node, dot11);

	NewAccessRouter->selector=1;

	NewLinkIdentifier->lid.lt=19;
	for (int j=0; j<5; j++)
	{
		NewAccessRouter->ma.octet_string[j]= apInfo->bssAddr.byte[j];
		NewLinkIdentifier->lid.la.ma.octet_string[j]= apInfo->bssAddr.byte[j];
		NewLinkIdentifier->lad.ma.octet_string[j]= apInfo->bssAddr.byte[j];
	}


	LinkHandoverStatus=0;//equals success

	Link_Handover_Complete_indication(node, OldLinkIdentifier,NewLinkIdentifier,OldAccessRouter,NewAccessRouter, LinkHandoverStatus);
	dot11->numLinkHandoverComplete++;


}


void MacDot11_Send_Link_Going_Down_Indication(Node* node, MacDataDot11* dot11)
{
	LINK_TUPLE_ID* LinkIdentifier= (LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

    //DOT11_ApInfo* apInfo=(DOT11_ApInfo*)MEM_malloc(sizeof( DOT11_ApInfo));
    //memset(apInfo, 0, sizeof(DOT11_ApInfo));

	LinkIdentifier->lid.lt=19; //this is WiFi
	for (int j=0; j<5; j++)
	{
		LinkIdentifier->lid.la.ma.octet_string[j]= dot11->bssAddr.byte[j];
		LinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];

	}

	UNSIGNED_INT_2	TimeInterval=0; //unknown

	LINK_GD_REASON  LinkGoingDownReason = 3;

	Link_Going_Down_indication(node, LinkIdentifier, TimeInterval, LinkGoingDownReason);
	dot11->numLinkGoingDownIndSent++;
}

void  MacDot11sta_Link_Down_indication(Node* node,
  MacDataDot11* dot11)
{
	LINK_DN_REASON   ReasonCode=2;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
		memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

		OldAccessRouter->selector=1;


	LinkIdentifier->lid.lt=19;
	for (int j=0; j<5; j++)
	{
		OldAccessRouter->ma.octet_string[j]= dot11->bssAddr.byte[j];
		LinkIdentifier->lid.la.ma.octet_string[j]= dot11->bssAddr.byte[j];
		LinkIdentifier->lad.ma.octet_string[j]= dot11->bssAddr.byte[j];
	}


	Link_Down_indication(node,LinkIdentifier,OldAccessRouter,ReasonCode);
		dot11->numLink_Down_indication++;
}

void MacDot11StaSendLinkParametersReportIndication (Node* node,
													MacDataDot11* dot11)
{

	

	DOT11_ApInfo* apInfo;
    DOT11_ManagementVars * mgmtVars =
                (DOT11_ManagementVars*) dot11->mngmtVars;

    apInfo = mgmtVars->channelInfo->headAPList;

	while (apInfo != NULL)
    {
        //if(apInfo->lastMeaTime >= mgmtVars->channelInfo->scanStartTime)
        //{
        L_LINK_PARAM_RPT* LinkParametersReportList = (L_LINK_PARAM_RPT *) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
		LINK_TUPLE_ID* LinkIdentifier = (LINK_TUPLE_ID *) MEM_malloc(sizeof(LINK_TUPLE_ID));
		LINK_PARAM_RPT * temp_report = (LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT));
		LINK_PARAM_TYPE* temp_lpt =( LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
		LINK_PARAM * temp = (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));

		memset(temp_lpt, 0, sizeof(LINK_PARAM_TYPE));
		memset(temp, 0, sizeof(LINK_PARAM));
		memset(temp_report, 0, sizeof(LINK_PARAM_RPT));
		memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));
		memset(LinkParametersReportList, 0, sizeof(L_LINK_PARAM_RPT));

		ParamTypeValue value;
		value.lp_gen = 1; //Received Signal Strength
		temp_lpt->value = value;
		temp_lpt->selector= 0;
		temp_lpt->next=NULL;
		//build first element of report
		temp->lpt = temp_lpt;
		LinkParamValue ParamValue;
		ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(apInfo->rssMean);
		temp->selector = 0;
		temp->value = ParamValue;

		temp_report->lp=temp;
		temp_report->threshold=NULL ;
		temp_report->next=NULL ;
		//add it to list
		LinkParametersReportList->elem = temp_report;
		LinkParametersReportList->length++;

		//next item in list

		LINK_PARAM_RPT * temp_report2=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
		LINK_PARAM_TYPE* temp_lpt2=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
		LINK_PARAM* temp2=(LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
		memset(temp_lpt2, 0, sizeof( LINK_PARAM_TYPE));
		memset(temp_report2, 0, sizeof( LINK_PARAM_RPT));
		memset(temp2, 0, sizeof( LINK_PARAM));

		value.lp_gen=2; //SINR

		temp_lpt2->value = value;
		temp_lpt2->selector= 0;
		temp_lpt2->next=NULL;
		//build second element of report

		temp2->lpt = temp_lpt2;
		ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(apInfo->cinrMean);
		temp2->selector = 0;
		temp2->value = ParamValue;
		temp_report2->lp=temp2;
		temp_report2->threshold=NULL ;
		temp_report2->next=NULL ;
		//add it to list

		LinkParametersReportList->elem->next = temp_report2;
		LinkParametersReportList->length++;

		LinkIdentifier->lid.lt = 19; //IEEE 802.11 - type of link
		LinkIdentifier->lid.la.selector = 0; //select mac address from the union
		LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
//		LinkIdentifier->lad.ma.octet_string = (char *) dot16->macAddress; //MAC address of (serving?) Base Station
		for (int i=0; i<6; i++)
		{
			LinkIdentifier->lid.la.ma.octet_string[i] = apInfo->bssAddr.byte[i];
		}

		double x = node->mobilityData->current->position.cartesian.x;
		double y = node->mobilityData->current->position.cartesian.y;

		double APIdX = 716.73672018469995;
		double APIdY = 414.14815622480000;

		double distance;
		double speed;

		//if (scCode / 16 == 4) //there is only one access point in this scenario, have to modify for more general case
		//{
			distance = sqrt(pow((APIdX - x),2) + pow((APIdY - y),2));
			speed = node->mobilityData->current->speed;
			BOOL printStats = TRUE;
					if (printStats)
					{
						clocktype currentTime = getSimTime(node);
						FILE *fp;
						int i=1;
						if (fp = fopen("distancenodeAP2", "a"))
						{
							fprintf(fp, "%d ", i);
							fprintf(fp, "%u ", node->nodeId);
							fprintf(fp, "12 ");
#ifdef _WIN32
							fprintf(fp, "%I64d ", currentTime);
#else
							fprintf(fp, "%lld ", currentTime);
#endif
							fprintf(fp, "%f ", distance);
							fprintf(fp, "%f ", speed);
							fprintf(fp, "%f ", apInfo->rssMean);
							fprintf(fp, "%f\n", apInfo->cinrMean);
							fclose(fp);
							i++;
						}
						else
						printf("Error opening stats\n");
					}

		
	//}


		//extract nodeId from STA address
		/*
		Mac802Address macAddr;
		memcpy(&macAddr.byte,
		ssInfo->macAddress,
		MAC_ADDRESS_LENGTH_IN_BYTE);
		*/
		MacHWAddress macHWAddr;
		Convert802AddressToVariableHWAddress(node, &macHWAddr, &(apInfo->bssAddr));
		int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

		//Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), (unsigned char *) apInfo->bssAddr.byte);
		Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, node->nodeId, (unsigned char *) apInfo->bssAddr.byte);

        //}
        apInfo = apInfo->next;
    }


}

void MacDot11StaSendLinkParametersReportIndication (Node* node,
													MacDataDot11* dot11,
													DOT11_SignalMeasurementInfo* measInfo,
													Mac802Address bssAddr)
{

    if (measInfo!=NULL)
    {


        L_LINK_PARAM_RPT* LinkParametersReportList = (L_LINK_PARAM_RPT *) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
		LINK_TUPLE_ID* LinkIdentifier = (LINK_TUPLE_ID *) MEM_malloc(sizeof(LINK_TUPLE_ID));
		LINK_PARAM_RPT * temp_report = (LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT));
		LINK_PARAM_TYPE* temp_lpt =( LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
		LINK_PARAM * temp = (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));

		memset(temp_lpt, 0, sizeof(LINK_PARAM_TYPE));
		memset(temp, 0, sizeof(LINK_PARAM));
		memset(temp_report, 0, sizeof(LINK_PARAM_RPT));
		memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));
		memset(LinkParametersReportList, 0, sizeof(L_LINK_PARAM_RPT));

		ParamTypeValue value;
		value.lp_gen = 1; //Received Signal Strength
		temp_lpt->value = value;
		temp_lpt->selector= 0;
		temp_lpt->next=NULL;
		//build first element of report
		temp->lpt = temp_lpt;
		LinkParamValue ParamValue;
		ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(measInfo->rssi);
		temp->selector = 0;
		temp->value = ParamValue;

		temp_report->lp=temp;
		temp_report->threshold=NULL ;
		temp_report->next=NULL ;
		//add it to list
		LinkParametersReportList->elem = temp_report;
		LinkParametersReportList->length++;

		//next item in list

		LINK_PARAM_RPT * temp_report2=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
		LINK_PARAM_TYPE* temp_lpt2=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
		LINK_PARAM* temp2=(LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
		memset(temp_lpt2, 0, sizeof( LINK_PARAM_TYPE));
		memset(temp_report2, 0, sizeof( LINK_PARAM_RPT));
		memset(temp2, 0, sizeof( LINK_PARAM));

		value.lp_gen=2; //SINR

		temp_lpt2->value = value;
		temp_lpt2->selector= 0;
		temp_lpt2->next=NULL;
		//build second element of report

		temp2->lpt = temp_lpt2;
		ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(measInfo->cinr);
		temp2->selector = 0;
		temp2->value = ParamValue;
		temp_report2->lp=temp2;
		temp_report2->threshold=NULL ;
		temp_report2->next=NULL ;
		//add it to list

		LinkParametersReportList->elem->next = temp_report2;
		LinkParametersReportList->length++;

		
		double x = node->mobilityData->current->position.cartesian.x;
		double y = node->mobilityData->current->position.cartesian.y;

		double APIdX = 716.73672018469995;
		double APIdY = 414.14815622480000;

		double distance;
		double speed;

		//if (scCode / 16 == 4) //there is only one access point in this scenario, have to modify for more general case
		//{
			distance = sqrt(pow((APIdX - x),2) + pow((APIdY - y),2));
			speed = node->mobilityData->current->speed;
			BOOL printStats = TRUE;
					if (printStats)
					{
						clocktype currentTime = getSimTime(node);
						FILE *fp;
						int i=1;
						if (fp = fopen("distancenodeAP2", "a"))
						{
							fprintf(fp, "%d ", i);
							fprintf(fp, "%u ", node->nodeId);
							fprintf(fp, "12 ");
#ifdef _WIN32
							fprintf(fp, "%I64d ", currentTime);
#else
							fprintf(fp, "%lld ", currentTime);
#endif
							fprintf(fp, "%f ", distance);
							fprintf(fp, "%f ", speed);
							fprintf(fp, "%f ", measInfo->rssi);
							fprintf(fp, "%f\n", measInfo->cinr);
							fclose(fp);
							i++;
						}
						else
						printf("Error opening stats\n");
					}

		
	//}
		
		
		//extract nodeId from STA address
		/*
		Mac802Address macAddr;
		memcpy(&macAddr.byte,
		ssInfo->macAddress,
		MAC_ADDRESS_LENGTH_IN_BYTE);
		*/
		MacHWAddress macHWAddr;
		Convert802AddressToVariableHWAddress(node, &macHWAddr, &(dot11->bssAddr));
		int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

		MacHWAddress macHWAddr2;
		Convert802AddressToVariableHWAddress(node, &macHWAddr2, &bssAddr);
		int interfaceIndex2 = MacGetInterfaceIndexFromMacAddress(node, macHWAddr2);
		/*LinkIdentifier->lid.la =  MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr2));*/
		LinkIdentifier->lid.lt = 19; //it is WiFi

//		char buff[6] = "";
//		NodeAddress bssAddress =
//		sprintf(buff,"%d",MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr2)));
		LinkIdentifier->lid.la.ma.addr_type = 6; //IEEE link type

		for (int j=0; j<5; j++)
		{
			//LinkIdentifier->lid.la.ma.octet_string[j] = ((char *) MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr2)))[j];
//					LinkIdentifier->lad.ma.octet_string[j] = bssAddr.byte[j];
//			LinkIdentifier->lid.la.ma.octet_string[j] = buff[j];
			LinkIdentifier->lid.la.ma.octet_string[j]=bssAddr.byte[j];

		}

		Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, node->nodeId, (unsigned char *) bssAddr.byte);
		dot11->numLink_Parameters_Report_indication++;
    }

}

void MacDot11StationGetMeasurementInfoAndSendLinkParametersReportIndication(Node* node,
		MacDataDot11* dot11,
		DOT11_ApInfo* apInfo)
{
	/*
	 PhySignalMeasurement* currMsgMea;
	    PhySignalMeasurement signalMeasVal;
	    Message* newmsg = (Message*)msg;
	    DOT11_SignalMeasurementInfo measInfo;
	    currMsgMea = (PhySignalMeasurement*) MESSAGE_ReturnInfo(newmsg);

	    if (currMsgMea == NULL)
	    {
	         no measurement data, do nothing
	        return;
	    }

	         update the measurement history and return the measurement info
	        MacDot11StationUpdateInstantaneousMeasurement(node,
	              dot11,
	              &measInfo,
	              currMsgMea);

	DOT11_ManagementVars* mngmtVars =
	        (DOT11_ManagementVars *) dot11->mngmtVars;
	DOT11_ApInfo* apInfo = mgmtVars->channelInfo->headAPList;*/

	while (apInfo!=NULL)
	{
		DOT11_SignalMeasurementInfo measInfo;
		measInfo.cinr = apInfo->cinrMean;
		measInfo.rssi = apInfo->rssMean;
		measInfo.measTime = apInfo->lastMeaTime;
		MacDot11StaSendLinkParametersReportIndication(node, dot11, &measInfo, apInfo->bssAddr);
		apInfo = apInfo->next;
	}

}






