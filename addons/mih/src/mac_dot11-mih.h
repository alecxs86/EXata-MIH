/*
 * mac_dot11-mih.h
 *
 *  Created on: 07.12.2010
 *      Author: alecxs
 */

#ifndef MAC_DOT11_MIH_H_
#define MAC_DOT11_MIH_H_


#include "MIH_LINK_SAP.h"
#include "mac_dot11.h"




void MacDot11sta_Link_Parameters_Report_indication(Node* node,
												   MacDataDot11* dot11);

void MacDot11Sta_Link_Up_indication(Node* node,
									MacDataDot11* dot11);

void  MacDot11Ap_Link_Up_indication(Node* node, MacDataDot11* dot11);

void MacDot11Sta_Send_LinkHandover_IminentIndication(Node* node,
													 MacDataDot11* dot11);

void MacDot11Sta_Send_LinkHandover_CompleteIndication(Node* node,
													  MacDataDot11* dot11);

void MacDot11_Send_Link_Going_Down_Indication(Node* node,
											  MacDataDot11* dot11);

void MacDot11sta_Link_Down_indication(Node* node,
									  MacDataDot11* dot11);

void MacDot11StaSendLinkParametersReportIndication (Node* node,
													MacDataDot11* dot11);

void MacDot11StaSendLinkParametersReportIndication (Node* node,
													MacDataDot11* dot11,
													DOT11_SignalMeasurementInfo* measInfo,
													Mac802Address bssAddr);

static
void MacDot11StationUpdateInstantaneousMeasurement(Node* node,
           const MacDataDot11* const dot11,
           DOT11_SignalMeasurementInfo* measInfo,
           PhySignalMeasurement* signalMea)
{
    int oldestMeas = 0;
    int numActiveMeas = 0;

    // store the current meas info
    measInfo->isActive = TRUE;
    measInfo->cinr = signalMea->cinr;
    measInfo->rssi = signalMea->rss;
    measInfo->measTime = getSimTime(node);

}

void MacDot11StationGetMeasurementInfoAndSendLinkParametersReportIndication(Node* node,
		MacDataDot11* dot11,
		DOT11_ApInfo* apInfo);

#endif /* MAC_DOT11_MIH_H_ */
