/*
 * mac_dot16_mih.h
 *
 *  Created on: 01.02.2011
 *      Author: alecxs
 */

#ifndef MAC_DOT16_MIH_H_
#define MAC_DOT16_MIH_H_

#include "MIH_LINK_SAP.h"
#include "mac_dot16_bs.h"
#include "mac_dot16_ss.h"

void MacDot16BsSendLinkParametersReportIndication(Node* node,
		MacDataDot16* dot16,
		MacDot16BsSsInfo* ssInfo);


void MacDot16SsSendLinkGoingDownIndication(Node* node,
		MacDot16Ss* dot16Ss);

void MacDot16Ss_Link_Parameters_Report_indication(Node* node,
		MacDataDot16* dot16);

CinrAndRss_list_str *mac_dot16_ss_CinrAndRss_parameteres(Node* node,
		MacDataDot16* dot16);

void MacDot16Ss_Send_LinkHandover_IminentIndication(Node* node,
		MacDataDot16* dot16);

void  MacDot16Ss_Link_Up_indication(Node* node,
		MacDataDot16* dot16);

void MacDot16Ss_Send_LinkHandover_IminentIndication(Node* node,
		MacDataDot16* dot16);

void MacDot16Ss_Link_Down_indication(Node* node,
		MacDataDot16* dot16);

void MacDot16SsReceiveLinkGetParametersRequest(Node* node,
		 MacDot16Ss* dot16Ss,
		 Message* msg);

void MacDot16Ss_Link_Parameters_Report_indication(Node* node,
		MacDataDot16* dot16,
		PhySignalMeasurement* signalMea,
		unsigned char* bsId);

#endif /* MAC_DOT16_MIH_H_ */
