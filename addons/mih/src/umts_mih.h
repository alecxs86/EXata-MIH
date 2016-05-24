/*
 * umts_mih.h
 *
 *  Created on: 04.06.2011
 *      Author: Ana Bucur
 */

#ifndef UMTS_MIH_H_
#define UMTS_MIH_H_

#include "MIH_LINK_SAP.h"
#include "layer3_umts.h"
#include "layer3_umts_rnc.h"

typedef struct RSSIandEcNo_str {
	unsigned char MacAddress[6];
	double rssi;
	double ecNo;
	RSSIandEcNo_str* next;
} RSSIandEcNo;

typedef struct RSSIandEcNo_list_str {
	RSSIandEcNo* first;
	RSSIandEcNo* last;
	int count;
} RSSIandEcNo_list;

void Umts_Link_Parameters_Report_indication(Node* node, UmtsLayer3Data *umtsL3);

void Umts_Link_Parameters_Report_indication(Node* node,
		UmtsLayer3Data *umtsL3, double rscpVal, double ecNoVal, unsigned int scCode);

RSSIandEcNo_list* Umts_RSSIandEcNo_parameteres(Node* node,
		UmtsLayer3Data *umtsL3);

void Umts_Link_Up_indication(Node* node, UmtsLayer3Data *umtsL3,
		UmtsRrcUeDataInRnc* ueRrc);

void UmtsReceiveLinkActionRequest(Node* node,
								  UmtsLayer3Data *umtsL3,
								  Message* msg);

void Umts_Link_Up_indication(Node* node, UmtsLayer3Data *umtsL3);

void Umts_Link_Down_indication(Node* node, UmtsLayer3Data *umtsL3);

void UmtsReceiveLinkGetParametersRequest(Node* node, UmtsLayer3Data* umtsL3, Message* msg);

void UmtsReceiveBestPoaIdReport(Node* node, UmtsLayer3Data *umtsL3, Message *msg);

#endif /* UMTS_MIH_H_ */
