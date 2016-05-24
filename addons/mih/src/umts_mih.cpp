
/*
 * umts_mih.cpp
 *
 *  Created on: 02.06.2011
 *      Author: Ana Bucur
 */

#include "umts_mih.h"
#include "mih_protocol.h"
#include "layer3_umts.h"
#include "layer3_umts_ue.h"
#include "layer3_umts_rnc.h"
//#include "layer3_umts_rnc.cpp"

#define DEBUG	1
/* tre definit in MIH_LINK_SAP.cpp  */



RSSIandEcNo_list* Umts_RSSIandEcNo_parameteres(Node* node, UmtsLayer3Ue* ueL3)
{
	UmtsLayer3UeNodebInfo* nodeBInfo = ueL3->priNodeB;
    
   
	
	RSSIandEcNo_list* temp =  (RSSIandEcNo_list *) MEM_malloc(sizeof(RSSIandEcNo_list));
	temp->count=0;

	clocktype currentTime = getSimTime(node);

    if (ueL3->priNodeB->cpichRscpMeas != NULL)
	{
		if (ueL3->priNodeB->cpichRscpMeas->lastTimeStamp > 0)
		{
			RSSIandEcNo* list_params = (RSSIandEcNo *) MEM_malloc(sizeof(RSSIandEcNo));
			
		//	for (int j=0; j<=5;j++)
		//		list_params->MacAddress[j]=dot16Ss->servingBs->bsId[j];

			list_params->rssi=ueL3->priNodeB->cpichRscp;
			list_params->ecNo=ueL3->priNodeB->cpichEcNo;
			list_params->next=NULL;
			temp->first=list_params;
			temp->last=list_params;
			temp->count++;
		}
	}

	if (nodeBInfo==NULL) printf ("fara acoperire UMTS\n");

/*	while (nodeBInfo != NULL)
	{
		if (nodeBInfo->cpichRscpMeas->lastTimeStamp > 0)
		{
			RSSIandEcNo* list_params2 = (RSSIandEcNo *) MEM_malloc(sizeof(RSSIandEcNo));

			//for (int j=0; j<=5;j++)
			//	list_params2->MacAddress[j]=nodeBInfo->bsId[j];

			list_params2->rssi= nodeBInfo->cpichRscp;
			list_params2->ecNo=nodeBInfo->cpichEcNo;
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
		//nodeBInfo = nodeBInfo->next;
		}
*/
	return temp;

}


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
    	intPart_reversed[charCount++] = (int) (x_int - 2*floor((double) (x_int/2)));
    	x_int = floor((double) (x_int/2));
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

RSSIandEcNo_list* Umts_RSSIandEcNo_parameteres(Node* node, UmtsLayer3Ue* ueL3, double rscpVal, double ecNoVal)
{
	UmtsLayer3UeNodebInfo* nodeBInfo = ueL3->priNodeB;
    
   
	
	RSSIandEcNo_list* temp =  (RSSIandEcNo_list *) MEM_malloc(sizeof(RSSIandEcNo_list));
	temp->count=0;

	clocktype currentTime = getSimTime(node);

    //if (ueL3->priNodeB->cpichRscpMeas != NULL)
	//{
	//	if (ueL3->priNodeB->cpichRscpMeas->lastTimeStamp > 0)
	//	{
			RSSIandEcNo* list_params = (RSSIandEcNo *) MEM_malloc(sizeof(RSSIandEcNo));
			
		//	for (int j=0; j<=5;j++)
		//		list_params->MacAddress[j]=dot16Ss->servingBs->bsId[j];

			list_params->rssi=rscpVal;
			list_params->ecNo=ecNoVal;
			list_params->next=NULL;
			temp->first=list_params;
			temp->last=list_params;
			temp->count++;
		//}
	//}

	if (nodeBInfo==NULL) printf ("fara acoperire UMTS\n");


	return temp;

}


void Umts_Link_Parameters_Report_indication(Node* node,
		UmtsLayer3Data *umtsL3)
{
	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;
	RSSIandEcNo_list* aux_temp = Umts_RSSIandEcNo_parameteres(node, ueL3 );

	if (aux_temp != NULL)
	{
		RSSIandEcNo* temp_str = aux_temp->first;
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(temp_str->rssi);
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(temp_str->ecNo);
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
			LinkIdentifier->lid.lt = 23; //UMTS - type of link
			for (int i=0; i<3; i++)
			{
				LinkIdentifier->lid.la.g3.pid[i] = ueL3->priNodeB->plmnId.mnc[i]; //nu complicam situatia, plmn-ul acelasi in toata reteaua...acelasi provider*/
			}
			LinkIdentifier->lid.la.g3.cid[0] = ueL3->priNodeB->cellId; //cell id --aici e vector

			

			//extract nodeId from SS address
			//not for umts
			/*Mac802Address macAddr;
			memcpy(&macAddr.byte,
					dot16->macAddress,
					MAC_ADDRESS_LENGTH_IN_BYTE);
			MacHWAddress macHWAddr;
			Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
			int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);*/
/*
			NodeAddress interfaceAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId(node, (NodeAddress) (ueL3->priNodeB->cellId / 16));
			int interfaceIndex = MAPPING_GetInterfaceIndexFromInterfaceAddress(node, interfaceAddr);
			MacHWAddress macHWAddr = GetMacHWAddress(node, interfaceIndex);
			Mac802Address macAddr;
			*/
			unsigned char poaId[MAC_ADDRESS_LENGTH_IN_BYTE] = {'0','0','0','0','0','0'};
			NodeAddress nodeBId = (NodeAddress) (ueL3->priNodeB->cellId / 16);
			double a= double (nodeBId/10);

			//convert to char
		
			if (nodeBId < 10)
			{
				poaId[0] = (unsigned char) nodeBId;
				poaId[1] = (unsigned char) 0;
				poaId[2] = (unsigned char) 0;
				poaId[3] = (unsigned char) 0;
				poaId[4] = (unsigned char) 0;
				poaId[5] = (unsigned char) 0;
			}
			else
				if(nodeBId<100)
				{
					poaId[0] = (unsigned char) floor(a);
					poaId[1] = (unsigned char) (nodeBId - floor(a));
					poaId[2] = (unsigned char) 0;
					poaId[3] = (unsigned char) 0;
					poaId[4] = (unsigned char) 0;
					poaId[5] = (unsigned char) 0;
				}


			//poaId = itoa(nodeBId);
			Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, ueL3->imsi->getId(), poaId);

			ueL3->stat.numLink_Parameters_Report_indication++; //trebuie declarate niste variabile care sa tina statistici

			temp_str = temp_str->next ;

		}
	}
	
}

void Umts_Link_Parameters_Report_indication(Node* node,
		UmtsLayer3Data *umtsL3, double rscpVal, double ecNoVal, unsigned int scCode)
{
	double x = node->mobilityData->current->position.cartesian.x;
	double y = node->mobilityData->current->position.cartesian.y;

	double NodeBIdX = 686.69246744830002;
	double NodeBIdY = 856.22789946880005;

	double distance;
	double speed;

	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;

	if (scCode / 16 == 4) //testing for hardcoded nodeB ID 4
	{
		distance = sqrt(pow((NodeBIdX - x),2) + pow((NodeBIdY - y),2));
		speed = node->mobilityData->current->speed;
		BOOL printStats = TRUE;
				if (printStats)
				{
					clocktype currentTime = getSimTime(node);
					FILE *fp;
					int i=1;
					if (fp = fopen("distancenode2", "a"))
					{
						fprintf(fp, "%d ", i);
						fprintf(fp, "%u ", node->nodeId);
						fprintf(fp, "%u ", scCode/16);
#ifdef _WIN32
						fprintf(fp, "%I64d ", currentTime);
#else
						fprintf(fp, "%lld ", currentTime);
#endif
						fprintf(fp, "%f ", distance);
						fprintf(fp, "%f ", speed);
						fprintf(fp, "%f ", rscpVal);
						fprintf(fp, "%f\n", ecNoVal);
						fclose(fp);
						i++;
					}
					else
					printf("Error opening stats\n");
				}

		
	}

	//RSSIandEcNo_list* aux_temp = Umts_RSSIandEcNo_parameteres(node, ueL3, rscpVal, ecNoVal);

	/*if (aux_temp != NULL)
	{
		RSSIandEcNo* temp_str = aux_temp->first;
		for (int i=0; i<aux_temp->count; i++)
		{*/
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(rscpVal);
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
			ParamValue.lpv = (UNSIGNED_INT_2) ConvertDoubleToInt(ecNoVal);
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
			LinkIdentifier->lid.lt = 23; //UMTS - type of link
			for (int i=0; i<3; i++)
			{
				LinkIdentifier->lid.la.g3.pid[i] = 0; 
				//deocamdata nu se cunoaste PLMN id, deoarece UE nu e inregistrata
			}
			LinkIdentifier->lid.la.g3.cid[0] = scCode; //cell id --aici e vector

			

			//extract nodeId from SS address
			//not for umts
			/*Mac802Address macAddr;
			memcpy(&macAddr.byte,
					dot16->macAddress,
					MAC_ADDRESS_LENGTH_IN_BYTE);
			MacHWAddress macHWAddr;
			Convert802AddressToVariableHWAddress(node, &macHWAddr, &macAddr);
			int interfaceIndex = MacGetInterfaceIndexFromMacAddress(node, macHWAddr);*/
/*
			NodeAddress interfaceAddr = MAPPING_GetDefaultInterfaceAddressFromNodeId(node, (NodeAddress) (ueL3->priNodeB->cellId / 16));
			int interfaceIndex = MAPPING_GetInterfaceIndexFromInterfaceAddress(node, interfaceAddr);
			MacHWAddress macHWAddr = GetMacHWAddress(node, interfaceIndex);
			Mac802Address macAddr;
			*/
			unsigned char poaId[MAC_ADDRESS_LENGTH_IN_BYTE] = {'0','0','0','0','0','0'};
			NodeAddress nodeBId = (NodeAddress) (scCode / 16);
			double a = double (nodeBId/10);

			//convert to char
		
			if (nodeBId < 10)
			{
				poaId[0] = (unsigned char) nodeBId;
				poaId[1] = (unsigned char) 0;
				poaId[2] = (unsigned char) 0;
				poaId[3] = (unsigned char) 0;
				poaId[4] = (unsigned char) 0;
				poaId[5] = (unsigned char) 0;
			}
			else
				if(nodeBId<100)
				{
					poaId[0] = (unsigned char) floor(a);
					poaId[1] = (unsigned char) (nodeBId - floor(a));
					poaId[2] = (unsigned char) 0;
					poaId[3] = (unsigned char) 0;
					poaId[4] = (unsigned char) 0;
					poaId[5] = (unsigned char) 0;
				}


			//poaId = itoa(nodeBId);
			Link_Parameters_Report_indication(node, LinkIdentifier, LinkParametersReportList, ueL3->imsi->getId(), poaId);

			ueL3->stat.numLink_Parameters_Report_indication++; //trebuie declarate niste variabile care sa tina statistici

		/*	temp_str = temp_str->next ;

		}
	}*/
	
}


void UmtsReceiveLinkActionRequest(Node* node,
								  UmtsLayer3Data *umtsL3,
								  Message* msg)
{
	UmtsLayer3Ue* ueL3 = (UmtsLayer3Ue*)(umtsL3->dataPtr);
	action_req *act_req = (action_req *) MESSAGE_ReturnInfo(msg);

	switch (act_req->LinkAction->type)
	{
	case 1: //LINK_DISCONNECT
		{
			UmtsLayer3UeReleaseDedctRsrc(node, umtsL3);
			UmtsLayer3UeEnterIdle(node, umtsL3, ueL3);
			UmtsLayer3UeUpdateStatAfterFailure(node, umtsL3, ueL3);
			UmtsLayer3UeRrcConnRelInd(node, umtsL3);
#if 1
			char clockStr[24];
			printf("MIH Function: Node %u received Link_Action.request\n",
				   node->nodeId);
			printf("    from MIHF ID %u\n", act_req->mobileNodeId);
			printf("with LINK_DISCONNECT action");
			TIME_PrintClockInSecond(getSimTime(node), clockStr);
			printf("time is %sS\n", clockStr);
#endif

			break;
		}
	case 2: //LINK_LOW_POWER
		{
			UmtsLayer3UeReleaseDedctRsrc(node, umtsL3);
			UmtsLayer3UeEnterIdle(node, umtsL3, ueL3);
			break;
		}
	case 3: //LINK_POWER_DOWN
		{
			break;
		}
	case 4: //LINK_POWER_UP
		{
			break;
		}
	case 0:
	default:
		{
			break;
		}
	}

}

void Umts_Link_Up_indication(Node* node,
		UmtsLayer3Data *umtsL3)
{   
	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(OldAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof(LINK_ADDR));
	memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	LinkIdentifier->lid.lt = 23; //UMTS
	LinkIdentifier->lid.la.selector = 1;
	LinkIdentifier->lid.la.g3.cid[0] = ueL3->priNodeB->cellId;

    
	std::list<UmtsLayer3UeNodebInfo*>::iterator it;
	if (ueL3->ueNodeInfo)
        for(it = ueL3->ueNodeInfo->begin();
            it != ueL3->ueNodeInfo->end();
            it ++)
        { 
			if (ueL3->priNodeB!= NULL)
			{
				OldAccessRouter->g3.cid[0]=(*it-1)->cellId;	
				NewAccessRouter->g3.cid[0]=(*it)->cellId;
			}
			 else
				NewAccessRouter->g3.cid[0]=(*it)->cellId;
		}


	for (int i=0; i<3; i++)
	{
	OldAccessRouter->g3.pid[i] = ueL3->priNodeB->plmnId.mnc[i]; 
	NewAccessRouter->g3.pid[i] = ueL3->priNodeB->plmnId.mnc[i];
	}	

   
	IP_MOB_MGMT MobilityManagementSupport=10/*00000000000000*/ ;

	
	Link_Up_indication( node, LinkIdentifier,OldAccessRouter,NewAccessRouter,false,MobilityManagementSupport);
	
	ueL3->stat.numLink_Up_indication++;
	
}	


void Umts_Link_Up_indication(Node* node,
		UmtsLayer3Data *umtsL3, UmtsRrcUeDataInRnc* ueRrc)
{   
	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));
	memset(OldAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_ADDR* NewAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof(LINK_ADDR));
	memset(NewAccessRouter, 0, sizeof(LINK_ADDR));

	LINK_TUPLE_ID* LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));
	memset(LinkIdentifier, 0, sizeof(LINK_TUPLE_ID));

	LinkIdentifier->lid.lt = 23; //UMTS
	LinkIdentifier->lid.la.selector = 1;
	if (ueL3->priNodeB) {}
		//LinkIdentifier->lid.la.g3.cid[0] = ueL3->priNodeB->cellId;
	else
	{
		char errMsg[MAX_STRING_LENGTH];
		sprintf(errMsg,
                    "UMTS Node %d (UE) has no primary nodeB, setting link identifier to 0 ", node->nodeId);
        ERROR_ReportWarning(errMsg);
		LinkIdentifier->lid.la.g3.cid[0] = 0;
	}
    
	OldAccessRouter->g3.cid[0]=ueRrc->prevPrimCellId;      
	NewAccessRouter->g3.cid[0]=ueRrc->primCellId;

/*	for (int i=0; i<3; i++)
	{
	OldAccessRouter->g3.pid[i] = NewAccessRouter->g3.pid[i];
	}	*/
	NewAccessRouter->g3.pid[0]=OldAccessRouter->g3.pid[0]; 
	NewAccessRouter->g3.pid[1]=OldAccessRouter->g3.pid[1];
	NewAccessRouter->g3.pid[2]=OldAccessRouter->g3.pid[2];

   

	IP_MOB_MGMT MobilityManagementSupport=10/*00000000000000*/;
	
	Link_Up_indication( node, LinkIdentifier,OldAccessRouter,NewAccessRouter,false,MobilityManagementSupport);
	
	ueL3->stat.numLink_Up_indication++;
	
}	

void Umts_Link_Down_indication(Node* node,
		UmtsLayer3Data *umtsL3)
{
	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;
    UmtsLayer3Rnc *rncL3 = (UmtsLayer3Rnc *) (umtsL3->dataPtr);

	LINK_ADDR* OldAccessRouter=( LINK_ADDR*)MEM_malloc(sizeof( LINK_ADDR));

	LINK_TUPLE_ID*  LinkIdentifier=( LINK_TUPLE_ID*)MEM_malloc(sizeof(LINK_TUPLE_ID));

	LINK_DN_REASON   ReasonCode;

/*	NodeId ueId;
	UmtsRrcUeDataInRnc* ueRrc = UmtsLayer3RncFindUeRrc(
                                    node,
                                    rncL3,
                                    ueId);  */

	LinkIdentifier->lid.lt=23;                   //UMTS
	for (int i=0; i<3; i++)
	{
		LinkIdentifier->lid.la.g3.pid[i] = ueL3->priNodeB->plmnId.mnc[i]; 
	}
	
	 
		//LinkIdentifier->lid.la.g3.cid[0]=ueRrc->primCellId;
		LinkIdentifier->lid.la.g3.cid[0]=ueL3->priNodeB->cellId;
			
/*	
	list<UmtsMntNodebInfo*>* mntNodebList;
	//UmtsMntNodebInfo* cellInfolist = ueRrc->mntNodebList;
	if (!mntNodebList->empty())
	{
		std::list<UmtsMntNodebInfo*>::iterator it;
		double bestActiveSig = 0;
		
		UmtsMntNodebInfo* bestActiveCell = (UmtsMntNodebInfo*)MEM_malloc(sizeof( UmtsMntNodebInfo));
     
		for (it = ueRrc->mntNodebList->begin();
			it != ueRrc->mntNodebList->end();
			it ++)
    
		{ 
			if (bestActiveCell == NULL)
				{
					bestActiveCell = (*it);
					bestActiveSig = (*it)->dlRscpMeas;
				}
            else if (bestActiveSig < (*it)->dlRscpMeas)
				{
					bestActiveCell = (*it);
					bestActiveSig = (*it)->dlRscpMeas;
				}
		}
		
*/

		//if (ueL3->priNodeB->cellId != bestActiveCell->cellId)
		{
			for (int i=0; i<3; i++)
				{
				OldAccessRouter->g3.pid[i] = ueL3->priNodeB->plmnId.mnc[i];
				}	
		

			//OldAccessRouter->g3.cid[0]=ueRrc->primCellId;		
			OldAccessRouter->g3.cid[0]=ueL3->priNodeB->cellId;
		}
//	}
	//if (dot16Ss->servingBs->rssMean < -78)
	
	ReasonCode=0;//explicit disconnect

	Link_Down_indication(node,LinkIdentifier,OldAccessRouter,ReasonCode);
	


	ueL3->stat.numLink_Down_indication++;

}


void UmtsReceiveLinkGetParametersRequest(Node* node,
		 UmtsLayer3Data *umtsL3,
		 Message* msg)
{	
	UmtsLayer3Ue *ueL3 = (UmtsLayer3Ue *) umtsL3->dataPtr;
	local_link_parameters *param_req = (local_link_parameters *)MESSAGE_ReturnInfo(msg);
	if (param_req->linkParamReq->elem->selector == 1) //this should be a switch
	{
		ParamTypeValue value = param_req->linkParamReq->elem->value;
		if (value.lp_gen == 2) //again this should be a switch, and it should be done inside a function
			// lp_gen = 2 means we want sinr
		{ //now we should build confirm primitive
			
			UmtsLayer3UeNodebInfo* nodeBInfo = ueL3->priNodeB;
			while (nodeBInfo != NULL)
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

				LINK_PARAM *ecNo =  (LINK_PARAM *) MEM_malloc(sizeof(LINK_PARAM));
				memset(ecNo, 0, sizeof(LINK_PARAM));

				ecNo->lpt = param;
				ecNo->selector = 1;
				LinkParamValue paramValue;
				paramValue.lpv = ueL3->priNodeB->cpichEcNo;
				ecNo->value = paramValue;

				l_LINK_PARAM *LinkParametersStatusList =  (l_LINK_PARAM *) MEM_malloc(sizeof(l_LINK_PARAM));
				memset(LinkParametersStatusList, 0, sizeof(l_LINK_PARAM));

				LinkParametersStatusList->length = 1;
				LinkParametersStatusList->elem = ecNo;

				//LINK_STATES_REQ LinkStatesRequest = 0; //We want the OP_MODE

				//LINK_DESC_REQ LinkDescriptorsRequest = 0; //We want the Classes of Service supported

				Link_Get_Parameters_confirm(node, 0, LinkParametersStatusList, NULL, NULL, param_req->srcMihfID, param_req->destMihfID, param_req->transactionId, param_req->transactionInitiated);
				ueL3->stat.numLinkGetParamsConf++;
				nodeBInfo = nodeBInfo++; // ?????????????????????????????????????? nodeBInfo = nodeBInfo->next DAR nu are in next in clasa
			}
		}

	}
}

void UmtsReceiveBestPoaIdReport(Node* node, UmtsLayer3Data *umtsL3, Message *msg)
{
	MihReportBestPoa *info = (MihReportBestPoa *)MESSAGE_ReturnInfo(msg);
	if (umtsL3->nodeType == CELLULAR_UE)
	{
		UmtsLayer3Ue* ueL3 = (UmtsLayer3Ue*) umtsL3->dataPtr;
		ueL3->bestPoaId = info->bestPoaId;
		ueL3->bestPoaIdMacProtocol = info->macProtocol;		

#if DEBUG
	{
		char clockStr[MAX_STRING_LENGTH];
		char buff[MAX_STRING_LENGTH];
		TIME_PrintClockInSecond(getSimTime(node), clockStr);
		printf("MIH Function: Node %u received MIH message report best PoA on CELLULAR MAC at %s s\n",
			   node->nodeId, clockStr);
		switch (ueL3->bestPoaIdMacProtocol)
		{
		case MAC_PROTOCOL_CELLULAR:
			{
				
				sprintf(buff, "Cellular MAC");
				printf("best PoA ID is %u of protocol %s\n", ueL3->bestPoaId, buff);
				break;
			}
		case MAC_PROTOCOL_DOT11:
			{
				
				sprintf(buff, "802.11");
				printf("best PoA ID is %u of protocol %s\n", ueL3->bestPoaId, buff);
				break;
			}
		default:
			{
				printf("best PoA ID is %u of protocol %d\n", ueL3->bestPoaId, ueL3->bestPoaIdMacProtocol);
				break;
			}
		}
	}
#endif /* DEBUG */
	}

	//MESSAGE_Free(node, msg);
	//break;
}
