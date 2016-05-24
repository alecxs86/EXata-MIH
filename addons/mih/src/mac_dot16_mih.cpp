/*
 * mac_dot16_mih.cpp
 *
 *  Created on: 01.02.2011
 *      Author: alecxs
 */
//#include "main.h"
#include "mac_dot16_mih.h"
#include "mac_dot16_bs.h"
#include "mac_dot16_ss.h"

static
unsigned short ConvertDoubleToInt(double x)
{
    unsigned short a = 0;
	int x_int;
    double x_frac;
    BOOL isNegative = FALSE;
    char intPart_reversed[8]="", conversion[16]="";
    int charCount = 0;

    x_int = floor (x);
    if (x_int<0)
    	x_int +=1;
    x_frac = x - (double)x_int;


    if (x<0)
    {
    	isNegative = TRUE;
    	conversion[0] = 1;
    	x_int = abs(x_int);
    	x_frac = fabs(x_frac);
    }



    while (x_int > 0) //Convert integer part, if any
       {
    	intPart_reversed[charCount++] = (int) (x_int - 2*floor((double)x_int/2));
    	x_int = floor((double)x_int/2);
//        intPart_reversed[charCount++] = (int)(2*x_int);
//        x_int = floor(x_int/10);
       }

    for (int i=1; i<9; i++) //Reverse the integer part, if any
       conversion[i] = intPart_reversed[8-i];

    charCount = 9;
//    conversion[charCount++] = '.'; //Decimal point

     while ((x_frac > 0) && (charCount < 16)) //Convert fractional part, if any
       {
        x_frac*=2;
        x_int = floor(x_frac);
        x_frac = x_frac - (double)x_int;
//        x_frac = modf(x_frac,&x_int);
        conversion[charCount++] = (int)x_int;
       }
     unsigned int p = 1;
     for (int j=0; j<16; j++)
     {
    	 a += p*conversion[15-j];
    	 p *= 2;
     }

    return a;
}

void MacDot16BsSendLinkParametersReportIndication(Node* node,
		MacDataDot16* dot16,
		MacDot16BsSsInfo* ssInfo)
{
	if (ssInfo->isRegistered)
	{
		L_LINK_PARAM_RPT* LinkParametersReportList=(L_LINK_PARAM_RPT*) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
		LINK_TUPLE_ID*    LinkIdentifier = ( LINK_TUPLE_ID*) MEM_malloc(sizeof(LINK_TUPLE_ID));
		LINK_PARAM_RPT * temp_report=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
		LINK_PARAM_TYPE* temp_lpt=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
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
		ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(ssInfo->ulRssMean);
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
		ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(ssInfo->ulCinrMean);
		temp2->selector = 0;
		temp2->value = ParamValue;
		temp_report2->lp=temp2;
		temp_report2->threshold=NULL ;
		temp_report2->next=NULL ;
		//add it to list

		LinkParametersReportList->elem->next = temp_report2;
		LinkParametersReportList->length++;

		//extract LinkIdentifier
		//MacDot16Bs* dot16Bs = (MacDot16Bs*) dot16->bsData;
		LinkIdentifier->lid.lt = 27; //IEEE 802.16 - type of link
		LinkIdentifier->lid.la.selector = 0; //select mac address from the union
		LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
//		LinkIdentifier->lad.ma.octet_string = (char *) dot16->macAddress; //MAC address of (serving?) Base Station
		for (int i=0; i<6; i++)
		{
			LinkIdentifier->lid.la.ma.octet_string[i] = dot16->macAddress[i];
		}

		Mac802Address macAddr;
		memcpy(&macAddr.byte,
				ssInfo->macAddress,
				MAC_ADDRESS_LENGTH_IN_BYTE);
		MacHWAddress macHWAddr;
		Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
		int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

		Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), (unsigned char *) dot16->macAddress);

		MacDot16Bs* bs = (MacDot16Bs*) dot16->bsData;
		bs->stats.numLinkParametersReportInd++;

		//now we should build report for neighbor BS in SS nbr list

		for (int i=0; i<DOT16e_DEFAULT_MAX_NBR_BS; i++)
		{
			if (ssInfo->nbrBsSignalMeas[i].bsValid)
			{
				L_LINK_PARAM_RPT* LinkParametersReportList=(L_LINK_PARAM_RPT*) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
				LINK_TUPLE_ID*    LinkIdentifier = ( LINK_TUPLE_ID*) MEM_malloc(sizeof(LINK_TUPLE_ID));
				LINK_PARAM_RPT * temp_report=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
				LINK_PARAM_TYPE* temp_lpt=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
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
				ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(ssInfo->nbrBsSignalMeas[i].dlMeanMeas.rssi);
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
				ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(ssInfo->nbrBsSignalMeas[i].dlMeanMeas.cinr);
				temp2->selector = 0;
				temp2->value = ParamValue;
				temp_report2->lp=temp2;
				temp_report2->threshold=NULL ;
				temp_report2->next=NULL ;
				//add it to list

				LinkParametersReportList->elem->next = temp_report2;
				LinkParametersReportList->length++;

				//extract LinkIdentifier
				//MacDot16Bs* dot16Bs = (MacDot16Bs*) dot16->bsData;
				LinkIdentifier->lid.lt = 27; //IEEE 802.16 - type of link
				LinkIdentifier->lid.la.selector = 0; //select mac address from the union
				LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned

				for (int j=0; j<6; j++)
					LinkIdentifier->lid.la.ma.octet_string[j] = ssInfo->nbrBsSignalMeas[i].bsId[j];
//
//				memcpy(LinkIdentifier->lad.ma.octet_string,
//						ssInfo->nbrBsSignalMeas[i].bsId,
//						MAC_ADDRESS_LENGTH_IN_BYTE);
//				LinkIdentifier->lad.ma.octet_string = (char *) ssInfo->nbrBsSignalMeas[i].bsId; //MAC address of Base Station

				//extract nodeId from SS address
				Mac802Address macAddr;
				memcpy(&macAddr.byte,
						ssInfo->macAddress,
						MAC_ADDRESS_LENGTH_IN_BYTE);
				MacHWAddress macHWAddr;
				Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
				int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

				Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), (unsigned char *) ssInfo->nbrBsSignalMeas[i].bsId);

				bs->stats.numLinkParametersReportInd++;

			}
		}

	}
}


//
CinrAndRss_list_str* mac_dot16_ss_CinrAndRss_parameteres(Node* node,
		MacDot16Ss* dot16Ss)
{
	MacDot16SsBsInfo* bsInfo;
	bsInfo = dot16Ss->nbrBsList;

	CinrAndRss_str* list_params = (CinrAndRss_str *) MEM_malloc(sizeof(CinrAndRss_str));

	CinrAndRss_list_str* temp =  (CinrAndRss_list_str *) MEM_malloc(sizeof(CinrAndRss_list_str));
	temp->count=0;
	
	clocktype currentTime = getSimTime(node);

	if (dot16Ss->servingBs != NULL)
	{
		if (dot16Ss->servingBs->lastMeaTime > 0 &&
            dot16Ss->servingBs->lastMeaTime + dot16Ss->para.nbrMeaLifetime >=
            currentTime)
		{
			CinrAndRss_str* list_params = (CinrAndRss_str *) MEM_malloc(sizeof(CinrAndRss_str));
			
			for (int j=0; j<=5;j++)
				list_params->MacAddress[j]=dot16Ss->servingBs->bsId[j];

			list_params->rss=dot16Ss->servingBs->rssMean;
			list_params->cinr=dot16Ss->servingBs->cinrMean;
			list_params->next=NULL;
			temp->first=list_params;
			temp->last=list_params;
			temp->count++;
		}
	}
	if (bsInfo==NULL) printf ("ai trecut de zona WiMAX, baiete\n");


	while (bsInfo != NULL)
	{
		if (bsInfo->lastMeaTime > 0 &&
            bsInfo->lastMeaTime + dot16Ss->para.nbrMeaLifetime >=
            currentTime)
		{
			CinrAndRss_str* list_params2 = (CinrAndRss_str *) MEM_malloc(sizeof(CinrAndRss_str));

			for (int j=0; j<=5;j++)
				list_params2->MacAddress[j]=bsInfo->bsId[j];

			list_params2->rss= bsInfo->rssMean;
			list_params2->cinr=bsInfo->cinrMean;
			list_params2->next = NULL;


			if (temp == NULL)
			{
				temp->first = list_params2;
				temp->last = list_params2;
				temp->count++;
			}
			else
			{
				temp->last->next =list_params2;
				temp->last = list_params2;
				//temp->last->next=NULL;
				temp->count++;
			}
		}
		bsInfo = bsInfo->next;
	}

	return temp;
}

void MacDot16Ss_Link_Parameters_Report_indication(Node* node,
		MacDataDot16* dot16)
{
	MacDot16Ss* dot16Ss = (MacDot16Ss *) dot16->ssData;
	CinrAndRss_list_str* aux_temp = mac_dot16_ss_CinrAndRss_parameteres(node, dot16Ss );
/*
    L_LINK_PARAM_RPT* LinkParametersReportList=( L_LINK_PARAM_RPT*) MEM_malloc(sizeof( L_LINK_PARAM_RPT)) ;

	LINK_TUPLE_ID*    LinkIdentifier= (  LINK_TUPLE_ID*) MEM_malloc(sizeof(  LINK_TUPLE_ID));
	LINK_PARAM_RPT * temp_report=(  LINK_PARAM_RPT *) MEM_malloc(sizeof(  LINK_PARAM_RPT ));
	LINK_PARAM_RPT * aux_temp_report=(  LINK_PARAM_RPT *) MEM_malloc(sizeof(  LINK_PARAM_RPT ));
	LINK_PARAM_TYPE* temp_lpt=(  LINK_PARAM_TYPE *) MEM_malloc(sizeof(  LINK_PARAM_TYPE ));

    memset(temp_lpt, 0, sizeof( LINK_PARAM_TYPE));
	memset(temp_report, 0, sizeof( LINK_PARAM_RPT));
	memset(aux_temp_report, 0, sizeof( LINK_PARAM_RPT));

	memset(LinkIdentifier, 0, sizeof( LINK_TUPLE_ID));
    memset(LinkParametersReportList, 0, sizeof(L_LINK_PARAM_RPT));



	 temp_lpt->value.lp_gen =1;
     temp_lpt->selector=0;
     temp_lpt->next=NULL;

     temp_report->lp.lpt=temp_lpt;
	 temp_report->lp.selector=0 ;
     temp_report->lp.value.qpv.selector=0 ;
	 temp_report->lp.next=NULL;

    temp_report->threshold.t_val=12 ;//cinr threshold
	temp_report->threshold.t_dir= 1;//below threshold
	temp_report->next=aux_temp_report ;

	 aux_temp_report->lp.lpt=temp_lpt;
	 aux_temp_report->lp.selector=1 ;
     aux_temp_report->lp.value.qpv.selector=1 ;
	 aux_temp_report->lp.next=NULL;

    aux_temp_report->threshold.t_val=12 ;//cinr threshold
	aux_temp_report->threshold.t_dir= 1;//below threshold
	aux_temp_report->next=NULL ;

	LinkParametersReportList->elem=temp_report;
    LinkParametersReportList->length=10;
*/
	if (aux_temp != NULL)
	{
		CinrAndRss* temp_str = aux_temp->first;
		for (int i=0; i<aux_temp->count; i++)
		{
			L_LINK_PARAM_RPT* LinkParametersReportList=(L_LINK_PARAM_RPT*) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
			LINK_TUPLE_ID*    LinkIdentifier = ( LINK_TUPLE_ID*) MEM_malloc(sizeof(LINK_TUPLE_ID));
			LINK_PARAM_RPT * temp_report=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
			LINK_PARAM_TYPE* temp_lpt=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(temp_str->rss);
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(temp_str->cinr);
			temp2->selector = 0;
			temp2->value = ParamValue;
			temp_report2->lp=temp2;
			temp_report2->threshold=NULL ;
			temp_report2->next=NULL ;
			//add it to list

			LinkParametersReportList->elem->next = temp_report2;
			LinkParametersReportList->length++;

			//extract LinkIdentifier
			//MacDot16Bs* dot16Bs = (MacDot16Bs*) dot16->bsData;
			LinkIdentifier->lid.lt = 27; //IEEE 802.16 - type of link
			LinkIdentifier->lid.la.selector = 0; //select mac address from the union
			LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
			memcpy(LinkIdentifier->lid.la.ma.octet_string,
					temp_str->MacAddress,
					MAC_ADDRESS_LENGTH_IN_BYTE);
//			LinkIdentifier->lad.ma.octet_string = (char *) temp_str->MacAddress; //MAC address of Base Station

			//extract nodeId from SS address
			Mac802Address macAddr;
			memcpy(&macAddr.byte,
					dot16->macAddress,
					MAC_ADDRESS_LENGTH_IN_BYTE);
			MacHWAddress macHWAddr;
			Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
			int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

			Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), (unsigned char* ) temp_str->MacAddress);

			dot16Ss->stats.numLink_Parameters_Report_indication++;

			temp_str = temp_str->next ;

		}
	}
	/*
	LinkIdentifier->lid.lt=27;
    while((temp!=NULL) && (temp->count!=0))
	     { temp->count--;
		  LinkIdentifier->lad.ma.octet_string=(char* )temp->first->next->MacAddress;
		  //octet_string-char;MacAddress-unsigned char

		  if (temp->first->next->rss!=0)
			   {
	            LinkParametersReportList->elem->lp.lpt->value.lp_gen =1;
			    LinkParametersReportList->elem->lp.value.lpv=(unsigned short)temp->first->next->rss;

				if (LinkParametersReportList->elem->lp.value.lpv< -76)
					{
					aux_temp_report->threshold.t_val=76 ;
					aux_temp_report->threshold.t_dir= 1;//below threshold
					aux_temp_report->next=aux_temp_report ;
					//temp_report->next=aux_temp_report;
					}
				else
					{
					aux_temp_report->threshold.t_val=76 ;
					aux_temp_report->threshold.t_dir= 0;//above threshold
					aux_temp_report->next=aux_temp_report ;
					//temp_report->next=aux_temp_report;
					}

			   }
		  else
			   {
				LinkParametersReportList->elem->lp.lpt->value.lp_gen =2;
	            LinkParametersReportList->elem->lp.value.lpv=(unsigned short)temp->first->next->cinr;
				if (LinkParametersReportList->elem->lp.value.lpv< 12)
					{
					aux_temp_report->threshold.t_val=12 ;//cinr threshold
					aux_temp_report->threshold.t_dir= 1;//below threshold
					aux_temp_report->next=aux_temp_report;
                   // temp_report->next=aux_temp_report;
					}
				else
					{
					aux_temp_report->threshold.t_val=12 ;//cinr threshold
					aux_temp_report->threshold.t_dir= 0;//above threshold
					aux_temp_report->next=aux_temp_report ;
                    //temp_report->next=aux_temp_report;
					}

		        }
		temp->first=temp->first->next;
		LinkParametersReportList->elem->next=LinkParametersReportList->elem ;
	}
	 temp_report->next=aux_temp_report;
 	 Link_Parameters_Report_indication(node,LinkIdentifier,LinkParametersReportList,0);

	 dot16Ss->stats.numLink_Parameters_Report_indication++;
	 */

}

void MacDot16Ss_Link_Parameters_Report_indication(Node* node,
		MacDataDot16* dot16,
		PhySignalMeasurement* signalMea,
		unsigned char* bsId)
{
	MacDot16Ss* dot16Ss = (MacDot16Ss *) dot16->ssData;

	L_LINK_PARAM_RPT* LinkParametersReportList=(L_LINK_PARAM_RPT*) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
	LINK_TUPLE_ID*    LinkIdentifier = ( LINK_TUPLE_ID*) MEM_malloc(sizeof(LINK_TUPLE_ID));
	LINK_PARAM_RPT * temp_report=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
	LINK_PARAM_TYPE* temp_lpt=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
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
	ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(signalMea->rss);
//	ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(signalMea->rss);
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
	ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(signalMea->cinr);
//	ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(signalMea->cinr);
	temp2->selector = 0;
	temp2->value = ParamValue;
	temp_report2->lp=temp2;
	temp_report2->threshold=NULL ;
	temp_report2->next=NULL ;
	//add it to list

	LinkParametersReportList->elem->next = temp_report2;
	LinkParametersReportList->length++;

	//extract LinkIdentifier
	//MacDot16Bs* dot16Bs = (MacDot16Bs*) dot16->bsData;
	LinkIdentifier->lid.lt = 27; //IEEE 802.16 - type of link
	LinkIdentifier->lid.la.selector = 0; //select mac address from the union
	LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
//	memcpy(LinkIdentifier->lid.la.ma.octet_string,
//			bsId,
//			MAC_ADDRESS_LENGTH_IN_BYTE);

	for (int j=0; j<=5;j++)
	{
		LinkIdentifier->lid.la.ma.octet_string[j] = bsId[j]; //(unsigned char *) bsId; //MAC address of Base Station
	}

	//extract nodeId from SS address
	Mac802Address macAddr;
	memcpy(&macAddr.byte,
			dot16->macAddress,
			MAC_ADDRESS_LENGTH_IN_BYTE);
	MacHWAddress macHWAddr;
	Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
	int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

	Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), bsId);

	dot16Ss->stats.numLink_Parameters_Report_indication++;

	MacDot16SsBsInfo* nbrInfo = dot16Ss->nbrBsList;

	while (nbrInfo != NULL)
	{

		L_LINK_PARAM_RPT* LinkParametersReportList=(L_LINK_PARAM_RPT*) MEM_malloc(sizeof(L_LINK_PARAM_RPT)) ;
		LINK_TUPLE_ID*    LinkIdentifier = ( LINK_TUPLE_ID*) MEM_malloc(sizeof(LINK_TUPLE_ID));
		LINK_PARAM_RPT * temp_report=(LINK_PARAM_RPT *) MEM_malloc(sizeof(LINK_PARAM_RPT ));
		LINK_PARAM_TYPE* temp_lpt=(LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE ));
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
		ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(nbrInfo->rssMean);
//		ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(nbrInfo->rssMean);
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
		ParamValue.lpv = (UNSIGNED_INT_2) RoundToInt(nbrInfo->cinrMean);
		temp2->selector = 0;
		temp2->value = ParamValue;
		temp_report2->lp=temp2;
		temp_report2->threshold=NULL ;
		temp_report2->next=NULL ;
		//add it to list

		LinkParametersReportList->elem->next = temp_report2;
		LinkParametersReportList->length++;

		//extract LinkIdentifier
		//MacDot16Bs* dot16Bs = (MacDot16Bs*) dot16->bsData;
		LinkIdentifier->lid.lt = 27; //IEEE 802.16 - type of link
		LinkIdentifier->lid.la.selector = 0; //select mac address from the union
		LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
		//	memcpy(LinkIdentifier->lid.la.ma.octet_string,
		//			bsId,
		//			MAC_ADDRESS_LENGTH_IN_BYTE);

		for (int j=0; j<=5;j++)
		{
			LinkIdentifier->lid.la.ma.octet_string[j] = nbrInfo->bsId[j]; //(unsigned char *) bsId; //MAC address of Base Station
		}

		//extract nodeId from SS address
		Mac802Address macAddr;
		memcpy(&macAddr.byte,
				dot16->macAddress,
				MAC_ADDRESS_LENGTH_IN_BYTE);
		MacHWAddress macHWAddr;
		Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
		int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);

		Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, MAPPING_GetNodeIdFromInterfaceAddress(node, MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr)), nbrInfo->bsId);

		dot16Ss->stats.numLink_Parameters_Report_indication++;

		nbrInfo = nbrInfo->next;

	}

	//temp_str->next = temp_str;

}


void  MacDot16Ss_Link_Up_indication(Node* node,
		MacDataDot16* dot16)
{   
	MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof(LINK_ADDR));
	memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	OldAccessRouter->selector = 0;
	OldAccessRouter->ma.addr_type = 6;
	for (int j=0; j<=5;j++)
		{
			OldAccessRouter->ma.octet_string[j] = dot16Ss->prevServBsId[j]; //MAC address of old Base Station
		}
	

/*
	MacDot16SsBsInfo* bsInfo=( MacDot16SsBsInfo*)MEM_malloc(sizeof( MacDot16SsBsInfo));
	memset(bsInfo, 0, sizeof(MacDot16SsBsInfo));
*/
	LinkIdentifier->lid.lt=27;
	LinkIdentifier->lid.la.selector = 0; //select mac address from the union
	LinkIdentifier->lid.la.ma.addr_type = 6; //802 address IANA assigned
	/*memcpy(LinkIdentifier->lid.la.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
	memcpy(LinkIdentifier->lad.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);*/

	for (int j=0; j<=5;j++)
		{
			LinkIdentifier->lid.la.ma.octet_string[j] = dot16Ss->servingBs->bsId[j]; //MAC address of new Base Station
		}
//	LinkIdentifier->lid.la.ma.octet_string=(char*)dot16Ss->servingBs->bsId;
//	LinkIdentifier->lad.ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	//MacDot16SsBsInfo* bsInfo = MacDot16eSsSelectBestNbrBs(node,dot16);

	//dot16Ss->servingBs=bsInfo;

	NewAccessRouter->selector=0;
	NewAccessRouter->ma.addr_type = 6;
	for (int j=0; j<=5;j++)
		{
			NewAccessRouter->ma.octet_string[j] = dot16Ss->servingBs->bsId[j]; //MAC address of old Base Station
		}
	/*memcpy(NewAccessRouter->ma.octet_string,
			bsInfo->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);*/
//	NewAccessRouter->ma.octet_string=(char*)bsInfo->bsId;

	IP_MOB_MGMT MobilityManagementSupport=10/*00000000000000*/; //?

	
	Link_Up_indication( node, LinkIdentifier,OldAccessRouter,NewAccessRouter,false,MobilityManagementSupport);
	dot16Ss->stats.numLink_Up_indication++;

}

void MacDot16Ss_Link_Down_indication(Node* node,
		MacDataDot16* dot16)
{
	MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;


	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));

	LINK_TUPLE_ID*    LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));

	LINK_DN_REASON   ReasonCode;

	LinkIdentifier->lid.lt=27;
	memcpy(LinkIdentifier->lad.ma.octet_string,
				dot16Ss->servingBs->bsId,
				MAC_ADDRESS_LENGTH_IN_BYTE);
	memcpy(LinkIdentifier->lid.la.ma.octet_string,
				dot16Ss->servingBs->bsId,
				MAC_ADDRESS_LENGTH_IN_BYTE);
//	LinkIdentifier->lid.la.ma.octet_string=(char*)dot16Ss->servingBs->bsId;
//	LinkIdentifier->lad.ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	MacDot16SsBsInfo* bestBs=( MacDot16SsBsInfo*)MEM_malloc(sizeof( MacDot16SsBsInfo));
	//memset(bestBs, 0, sizeof(MacDot16SsBsInfo));
	bestBs = MacDot16eSsSelectBestNbrBs(node,dot16);

	if (dot16Ss->servingBs!=bestBs )
	{
		OldAccessRouter->selector=0;
		memcpy(OldAccessRouter->ma.octet_string,
						dot16Ss->servingBs->bsId,
						MAC_ADDRESS_LENGTH_IN_BYTE);
//		OldAccessRouter->ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	}
	//if (dot16Ss->servingBs->rssMean < -78)
	//{
	ReasonCode=0;//explicit disconnect

	Link_Down_indication(node,LinkIdentifier,OldAccessRouter,ReasonCode);
	//}


	dot16Ss->stats.numLink_Down_indication++;

}
/* The function is called whenever a decision of handover has been taken by the MIH user
 */

void MacDot16Ss_Send_LinkHandover_IminentIndication(Node* node,
		MacDataDot16* dot16)
{
	MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;

	LINK_TUPLE_ID*    OldLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	//memset(OldLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));


	LINK_TUPLE_ID*    NewLinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	//memset(NewLinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	LINK_ADDR* OldAccessRouter= (LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	//memset(OldAccessRouter, 0, sizeof(LINK_ADDR));


	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	//memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	OldAccessRouter->selector=0;
	memcpy(OldAccessRouter->ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	OldAccessRouter->ma.octet_string=(char*)dot16Ss->servingBs->bsId;
	OldLinkIdentifier->lid.lt=27;
	memcpy(OldLinkIdentifier->lid.la.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
	memcpy(OldLinkIdentifier->lad.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	OldLinkIdentifier->lid.la.ma.octet_string=(char*)dot16Ss->servingBs->bsId;
//	OldLinkIdentifier->lad.ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	MacDot16SsBsInfo* bsInfo=( MacDot16SsBsInfo*)MEM_malloc(sizeof( MacDot16SsBsInfo));
	//memset(bsInfo, 0, sizeof(MacDot16SsBsInfo));
	bsInfo = MacDot16eSsSelectBestNbrBs(node,dot16);

	NewAccessRouter->selector=0;
	memcpy(NewAccessRouter->ma.octet_string,
			bsInfo->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewAccessRouter->ma.octet_string=(char*)bsInfo->bsId;
	NewLinkIdentifier->lid.lt=27;
	memcpy(NewLinkIdentifier->lid.la.ma.octet_string,
			bsInfo->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewLinkIdentifier->lid.la.ma.octet_string=(char*)bsInfo->bsId;
	memcpy(NewLinkIdentifier->lad.ma.octet_string,
			bsInfo->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewLinkIdentifier->lad.ma.octet_string=(char*)bsInfo->bsId;


	Link_Handover_Iminent_indication(node, OldLinkIdentifier,NewLinkIdentifier,OldAccessRouter,NewAccessRouter);
	dot16Ss->stats.numLinkHandoverIminent++;


}

void MacDot16Ss_Send_LinkHandover_CompleteIndication(Node* node,
		MacDataDot16* dot16)
{

	LINK_TUPLE_ID*    OldLinkIdentifier= (LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	LINK_TUPLE_ID*    NewLinkIdentifier= (LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	LINK_ADDR*        OldAccessRouter=(LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	LINK_ADDR*        NewAccessRouter =(LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	STATUS           LinkHandoverStatus;

	MacDot16Ss* dot16Ss = (MacDot16Ss*) dot16->ssData;

	OldAccessRouter->selector=0;
	memcpy(OldAccessRouter->ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);

	memcpy(OldLinkIdentifier->lid.la.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
	memcpy(OldLinkIdentifier->lad.ma.octet_string,
			dot16Ss->servingBs->bsId,
			MAC_ADDRESS_LENGTH_IN_BYTE);
//	OldAccessRouter->ma.octet_string=(char*)dot16Ss->servingBs->bsId;
	OldLinkIdentifier->lid.lt=27;
//	OldLinkIdentifier->lid.la.ma.octet_string=(char*)dot16Ss->servingBs->bsId;
//	OldLinkIdentifier->lad.ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	MacDot16SsBsInfo* bsInfo=( MacDot16SsBsInfo*)MEM_malloc(sizeof( MacDot16SsBsInfo));
	//memset(bsInfo, 0, sizeof(MacDot16SsBsInfo));
	bsInfo = MacDot16eSsSelectBestNbrBs(node,dot16);

	NewAccessRouter->selector=0;
	memcpy(NewAccessRouter->ma.octet_string,
				bsInfo->bsId,
				MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewAccessRouter->ma.octet_string=(char*)bsInfo->bsId;
	NewLinkIdentifier->lid.lt=27;
	memcpy(NewLinkIdentifier->lid.la.ma.octet_string,
				bsInfo->bsId,
				MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewLinkIdentifier->lid.la.ma.octet_string=(char*)bsInfo->bsId;
	memcpy(NewLinkIdentifier->lad.ma.octet_string,
				bsInfo->bsId,
				MAC_ADDRESS_LENGTH_IN_BYTE);
//	NewLinkIdentifier->lad.ma.octet_string=(char*)bsInfo->bsId;

	LinkHandoverStatus=0;//success



	Link_Handover_Complete_indication(node,OldLinkIdentifier,NewLinkIdentifier,
			OldAccessRouter,NewAccessRouter,LinkHandoverStatus);
	dot16Ss->stats.numLinkHandoverComplete++;

}

void MacDot16SsSendLinkGoingDownIndication(Node* node,
		MacDot16Ss* dot16Ss)
{
	/*Mac802Address destMacAddr;
	memcpy(&destMacAddr.byte,
	dot16Ss->servingBs->bsId,
	MAC_ADDRESS_LENGTH_IN_BYTE);

	MacHWAddress macAddr;
	Convert802AddressToVariableHWAddress(node, &macAddr, &destMacAddr);
	int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macAddr);
	NodeAddress destAddr = MacHWAddressToIpv4Address(node, interfaceIndex, &macAddr);

	Link_Going_Down_indication(node, NULL, 0, 1, destAddr);
	dot16Ss->stats.numLinkGoingDownIndSent++;*/



	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	//memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));
	LinkIdentifier->lid.lt=27;
	LinkIdentifier->lid.la.selector = 0;
	LinkIdentifier->lid.la.ma.addr_type = 6; //IEEE address type
	for (int j=0; j<6; j++)
		LinkIdentifier->lid.la.ma.octet_string[j] = dot16Ss->servingBs->bsId[j];

//	memcpy(LinkIdentifier->lid.la.ma.octet_string,
//			dot16Ss->servingBs->bsId,
//			MAC_ADDRESS_LENGTH_IN_BYTE);
//	memcpy(LinkIdentifier->lad.ma.octet_string,
//			dot16Ss->servingBs->bsId,
//			MAC_ADDRESS_LENGTH_IN_BYTE);
//	LinkIdentifier->lid.la.ma.octet_string=(char*)dot16Ss->servingBs->bsId;
//	LinkIdentifier->lad.ma.octet_string=(char*)dot16Ss->servingBs->bsId;

	UNSIGNED_INT_2 TimeInterval=0;
	//if (dot16Ss->servingBs->rssMean < -70)

	LINK_GD_REASON  LinkGoingDownReason=0;//explicit disconnect

	Link_Going_Down_indication(node, LinkIdentifier, TimeInterval, LinkGoingDownReason);

	dot16Ss->stats.numLinkGoingDownIndSent++;

}

void MacDot16SsReceiveLinkGetParametersRequest(Node* node,
		 MacDot16Ss* dot16Ss,
		 Message* msg)
{
	local_link_parameters *param_req = (local_link_parameters *)MESSAGE_ReturnInfo(msg);
	if (param_req->linkParamReq->elem->selector == 1) //this should be a switch
	{
		ParamTypeValue value = param_req->linkParamReq->elem->value;
		if (value.lp_gen == 2) //again this should be a switch, and it should be done inside a function
			// lp_gen = 2 means we want cinr
		{ //now we should build confirm primitive
			MacDot16SsBsInfo* bsInfo = dot16Ss->nbrBsList;
			while (bsInfo != NULL)
			{
				NodeAddress mihfID = param_req->srcMihfID;
				/*
							NodeAddress ssAddress = MAPPING_GetDefaultInterfaceAddressFromNodeId(node, mihfID);
							MacDot16BsSsInfo* ssInfo = NULL;
							BOOL found = FALSE;
							for (int index = 0; index < DOT16_BS_SS_HASH_SIZE; index ++)
							{
								ssInfo = dot16Bs->ssHash[index];
								while (ssInfo != NULL)
								{
									Mac802Address macAddr;
									memcpy(&macAddr.byte,
									ssInfo->macAddress,
									MAC_ADDRESS_LENGTH_IN_BYTE);
									MacHWAddress macHWAddr;
									Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
									int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);
									if (ssAddress == MacHWAddressToIpv4Address(node, interfaceIndex, &macHWAddr))
									{
										found = TRUE;
										break;
									}
									ssInfo = ssInfo->next;
								}
								if (found)
									break;
							}
				 */
				LINK_PARAM_TYPE *param =  (LINK_PARAM_TYPE *) MEM_malloc(sizeof(LINK_PARAM_TYPE));
				memset(param, 0, sizeof(LINK_PARAM_TYPE));
				ParamTypeValue confirmValue;
				confirmValue.lp_gen = value.lp_gen;
				param->value = value;
				param->selector = 1;
				param->next = NULL;

				LINK_PARAM *cinr =  (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
				memset(cinr, 0, sizeof(LINK_PARAM));

				cinr->lpt = param;
				cinr->selector = 1;
				LinkParamValue paramValue;
				paramValue.lpv = bsInfo->cinrMean;
				cinr->value = paramValue;

				l_LINK_PARAM *LinkParametersStatusList =  (l_LINK_PARAM *) MEM_malloc(sizeof(l_LINK_PARAM));
				memset(LinkParametersStatusList, 0, sizeof(l_LINK_PARAM));

				LinkParametersStatusList->length = 1;
				LinkParametersStatusList->elem = cinr;

				//LINK_STATES_REQ LinkStatesRequest = 0; //We want the OP_MODE

				//LINK_DESC_REQ LinkDescriptorsRequest = 0; //We want the Classes of Service supported

				Link_Get_Parameters_confirm(node, 0, LinkParametersStatusList, NULL, NULL, param_req->srcMihfID, param_req->destMihfID, param_req->transactionId, param_req->transactionInitiated);
				dot16Ss->stats.numLinkGetParamsConf++;
				bsInfo = bsInfo->next;
			}
		}

	}
}
