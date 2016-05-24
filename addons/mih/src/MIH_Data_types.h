/*
 * MIH_Data_types.h  -  Common header file for global data structures
 *
 * The Intellectual Property Rights of authors of this code are regulated by 
 * Intellectual Property Rights Agreement established for NATO Science for 
 * Peace Project RIWCoS (SfP-982469). The AGREEMENT was made between 
 *     University of Peloponnese, Tripoli, GREECE, 
 *     University POLITEHNICA of Bucharest, Bucharest, ROMANIA,  
 *     University ���Ss Cyril and Methodius���, Skopje, FYROM,
 *     Aalborg University,  Aalborg, DENMARK, and 
 *     Vodafone Romania, Bucharest, ROMANIA 
 * on the 14th of December 2006, and shall be governed by the laws of Belgium.
 *
 * As stated into the Agreement, ���Foreground Intellectual Property��� includes
 * copyright material including computer software, and shall be owned solely 
 * or jointly by the Party(ies) who generated the Intellectual Property or 
 * engaged or employed the person or persons who made or conceived the 
 * Intellectual Property.
 *
 * The Agreement also states the all Parties shall have free use of all 
 * Foreground Intellectual Property to exploit the results by themselves.  
 *
 * This software was developed solely at the 
 *     University POLITEHNICA of Bucharest, Bucharest, ROMANIA,
 * by employees involved in NATO Science for Peace Project RIWCoS (SfP-982469)
 *
 * @authors Eduard C. Popovici
 * @version 1.0
 */

    // ************************************************************************
    //
    //   MIH_Data_types definitions - Annex F
    //
    // ************************************************************************



    
    //******************************************
    // Table F.1?Basic data types
    //******************************************
    
    // INTEGER(size)

    typedef char           INTEGER_1;
    typedef short          INTEGER_2;
    typedef int            INTEGER_4;
    typedef long           INTEGER_8;

    // UNSIGNED_INT(size)

    typedef unsigned char  UNSIGNED_INT_1;
    typedef unsigned short UNSIGNED_INT_2;
    typedef unsigned int   UNSIGNED_INT_4;
    typedef unsigned long  UNSIGNED_INT_8;

    // BITMAP(size)
    // A BITMAP(N), where N must be a multiple of 8, is made up of an N/8
    // octet values and encoded in network byte order
    
    typedef unsigned char  BITMAP_8;
    typedef unsigned short BITMAP_16;
    typedef unsigned int   BITMAP_32;
    typedef unsigned long  BITMAP_64;
    typedef BITMAP_64   BITMAP_256[4];

    // CHOICE(DATATYPE1,DATATYPE2[,?])

    /*
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            <type> <elem>;
        }
    } CH_;
    */


    // LIST(DATATYPE) - The encoding rule is a variable length Length field
    // (1-2 octets) followed by a variable length Value field

    /*
    typedef struct {
        UNSIGNED_INT_2  length;
           *elem;
    } L_;
    */

	
    //******************************************
    // Table F.2?General data types
    //******************************************

    typedef UNSIGNED_INT_1 ENUMERATED;

//    typedef bool BOOL;

    typedef struct {
        UNSIGNED_INT_2 length;
        INTEGER_1 *elem;
    } OCTET_STRING;

    typedef UNSIGNED_INT_1 PERCENTAGE;

    typedef UNSIGNED_INT_1 STATUS;
        // A list of link commands  Bit 0: Reserved  Bit 1: Link_Event_Subscribe
    // Bit 2: Link_Event_Unsubscribe  Bit 3: Link_Get_Parameters
    // Bit 4: Link_Configure_Thresholds  Bit 5: Link_Action  Bit 6-31: (Rsrvd)
    typedef BITMAP_32 LINK_CMD_LIST;

    // A set of link descriptors  Bit 0: Number of Classes of Service Supported
    // Bit 1: Number of Queues Supported   Bits 2-15: (Reserved)
    typedef BITMAP_16 LINK_DESC_REQ;

    // A type to represent the maximum data rate in kb/s (0 - 2^32-1)
    typedef UNSIGNED_INT_4 LINK_DATA_RATE;

    // Represents the reason of a link down event  0: Explicit disconnect  
    // 1: Packet timeout  2: No resource  3: No broadcast  4: Auth. failure   
    // 5: Billing failure  6-127: (Reserved)  128-255: Vendor specific reason
    typedef UNSIGNED_INT_1 LINK_DN_REASON;

    // A list of link events  Bit 0: Link_Detected  Bit 1: Link_Up
    // Bit 2: Link_Down  Bit 3: Link_Parameters_Report  Bit 4: Link_Going_Down
    // Bit 5: Link_Handover_Imminent  Bit 6: Link_Handover_Complete
    // Bit 7: Link_PDU_Transmit_Status  Bit 8-31: (Reserved)
    typedef BITMAP_32 LINK_EVENT_LIST;

    // Represents the reason of a link going down
    // 0: Explicit disconnect  1: Link parameter degrading  2: Low power
    // 3: No resource  4-127: (Reserved) 128-255: Vendor specific reason codes
    typedef UNSIGNED_INT_2 LINK_GD_REASON;
    
    // Represents if MIH capability is supported (bit is set) or not
    // Bit 1: event serv. (ES) supported  Bit 2: command serv. (CS) supported
    // Bit 3: information service (IS) supported   Bit 0, 4-7: (Reserved)
    typedef BITMAP_8 LINK_MIHCAP_FLAG;

    // A type to represent a link parameter for 802.11 
    // 0: RSSI of the beacon channel def in IEEE Std 802.11-2007 (only for MN)
    // 1: No QoS resource available (LINK_PARAM_VAL is set TRUE 
    // 2: Multicast packet loss rate   3-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_802_11;

    // A type to represent a link parameter for 802.16   0-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_802_16;

    // A type to represent a link parameter for 802.20   0-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_802_20;

    // A type to represent a link parameter for 802.22   0-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_802_22;

    // A type to represent a link parameter for CDMA2000
    // 0: PILOT_STRENGTH   1-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_C2K;

    // A type to represent a link parameter for CDMA2000 HRPD
    // 0: PILOT_STRENGTH   1-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_HRPD;

    // A type to represent a link parameter for EDGE   0-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_EDGE;

    // A type to represent a link parameter for Ethernet   0-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_ETH;

    // A type to represent a generic link parameter applicable to any link type
    // 0: Data Rate  1: Signal Strength  2: Signal over interference + noise
    // ratio (SINR)   3:Throughput   4: Packet Error Rate   5-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_GEN;

    // A type to represent a link parameter for GSM and GPRS
    // 0: RxQual   1: RsLev   2: Mean BEP   3: StDev BEP   4-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_GG;

    // A type to represent QOS_LIST parameters
    // 0: Maximum number of differentiable classes of service supported
    // 1: Minimum packet transfer delay for all CoS
    // 2: Average packet transfer delay for all CoS 
    // 3: Maximum packet transfer delay for all CoS 
    // 4: Packet transfer delay jitter for all CoS 
    // 5: Packet loss rate for all CoS    6-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_PARAM_QOS;

    // The current value of the parameter
    typedef UNSIGNED_INT_2 LINK_PARAM_VAL;

    // A type to represent a link parameter for UMTS   0: CPICH RSCP
    // 1: PCCPCH RSCP  2: UTRA carrier RSSI  3: GSM carrier RSSI
    // 4: CPICH Ec/No  5: Transport channel BLER  6: user equipment (UE) 
    // transmitted power  7-255: (Reserved) 
    typedef UNSIGNED_INT_1 LINK_PARAM_FDD;
    
    // Indicates if a resource is available or not (TRUE: Available)
    typedef BOOL LINK_RES_STATUS;
    
    // Represents link type  0: Reserved  1: Wireless - GSM  2: Wireless - GPRS
    // 3: Wireless - EDGE  15: Ethernet  18: Wireless - Other
    // 19: Wireless - IEEE 802.11  22: Wireless - CDMA2000  23: Wireless - UMTS
    // 24: Wireless - cdma2000-HRPD  27: Wireless - IEEE 802.16
    // 28: Wireless - IEEE 802.20    29: Wireless - IEEE 802.22
    typedef UNSIGNED_INT_1 LINK_TYPE;

    // Maximum number of differentiable classes of service supported (0..255)
    typedef UNSIGNED_INT_1 NUM_COS;

    // The number of transmit queues supported  (0..255)
    typedef UNSIGNED_INT_1 NUM_QUEUE;

    // The link power mode  0: Normal Mode  1: Power Saving Mode
    // 2: Powered Down  3-255: (Reserved)
    typedef UNSIGNED_INT_1 OP_MODE;

    // Action on threshold  0: Set normal threshold  1: Set one-shot threshold  
    // 2: Cancel threshold
    typedef ENUMERATED TH_ACTION;

    // Threshold value (format of media-dependent value is defined in standard,
    // remaining unused bits in the data type are marked as all zeros)
    typedef UNSIGNED_INT_2 THRESHOLD_VAL;
 
    // Direction threshold is crossed   0: ABOVE_THRESHOLD   1: BELOW_THRESHOLD
    typedef UNSIGNED_INT_1 THRESHOLD_X_DIR;

    // Timer value (ms) used to set the interval between periodic reports
    typedef UNSIGNED_INT_2 TIMER_INTERVAL;
    
    // Link states to be requested  Bit 0: OP_MODE  Bit 1: CHANNEL_ID
    // Bit 2-15: (Reserved)
    typedef BITMAP_16 LINK_STATES_REQ;
    
    // A binary encoded structure (TLV) for Information Elements
    typedef struct {
        UNSIGNED_INT_4 type;
        struct length_value {
            UNSIGNED_INT_2  length;
            INTEGER_1       *value;
        };
    } INFO_ELEMENT;

	 // The public land mobile network (PLMN) unique identifier
    typedef INTEGER_1 PLMN_ID[3];

    // Location Area Code (LAC) identifying a location area within a PLMN
    typedef INTEGER_1 LAC[2];

    // The BSS and cell within the BSS are identified by Cell Identity (CI)
    typedef INTEGER_1 CI[2];

    // A data type to represent a 3GPP 2G cell identifier
    typedef struct {
        PLMN_ID pid;
        LAC     lac;
        CI      ci;
    } THREEGPP_2G_CELL_ID;

    // This data type identifies a cell uniquely within 3GPP UTRAN
    typedef UNSIGNED_INT_4 CELL_ID[4];

    // A data type to represent a 3GPP 2G cell identifier
    typedef struct {
        PLMN_ID pid;
        CELL_ID cid;
    } THREEGPP_3G_CELL_ID;

    // A data type to represent a 3GPP transport address
    typedef INTEGER_1 *THREEGPP_ADDR;

    // A data type to represent a 3GPP2 transport address
    typedef INTEGER_1 *THREEGPP2_ADDR;

    // A data type to represent other link-layer address 
    typedef INTEGER_1 *OTHER_L2_ADDR;

   



    //******************************************
    // Table F.3?Data types for address
    //******************************************

    // A type to represent a transport address
    typedef struct {
        // address type http://www.iana.org/assignments/address-family-numbers
        UNSIGNED_INT_2 addr_type;
        INTEGER_1 octet_string[6];
    } TRANSPORT_ADDR;

    // Represents a MAC address for a specific link layer
     typedef TRANSPORT_ADDR MAC_ADDR; //modified!!!
	
	//typedef char* MAC_ADDR; //modified!!

    // Represents an IP address (type is either 1 for IPv4 or 2 for IPv6)
    typedef TRANSPORT_ADDR IP_ADDR;

    // A data type to represent an address of any link-layer
    typedef struct {
        UNSIGNED_INT_1 selector;
        //union value {
            MAC_ADDR ma;
            THREEGPP_3G_CELL_ID g3;
            THREEGPP_2G_CELL_ID g2;
            THREEGPP_ADDR a3;
            THREEGPP2_ADDR a2;
            OTHER_L2_ADDR ot;
       // };
    } LINK_ADDR;

	 // The identifier of a link that is not associated with the peer node
    typedef struct {
        LINK_TYPE   lt;
        LINK_ADDR   la;
    } LINK_ID;


    //******************************************
    // Table F.9?Data types for QoS
    //******************************************
    
    // Maximum number of differentiable classes of service supported
    typedef UNSIGNED_INT_1 NUM_COS_TYPES;

    // A type to represent a class of service identifier
    typedef UNSIGNED_INT_1 COS_ID;
    
    // Minimum packet transfer delay in ms for the specific CoS 
    typedef struct {
        COS_ID          cid;
        UNSIGNED_INT_2  val;
    } MIN_PK_TX_DELAY;

    // Average packet transfer delay in ms for the specific CoS 
    typedef struct {
        COS_ID          cid;
        UNSIGNED_INT_2  val;
    } AVG_PK_TX_DELAY;

    // Maximum packet transfer delay in ms for the specific CoS
    typedef struct {
        COS_ID          cid;
        UNSIGNED_INT_2  val;
    } MAX_PK_TX_DELAY;

    // Packet transfer delay jitter in ms for the specific CoS
    typedef struct {
        COS_ID          cid;
        UNSIGNED_INT_2  val;
    } PK_DELAY_JITTER;

    // Packet loss rate for the specific CoS
    typedef struct {
        COS_ID          cid;
        UNSIGNED_INT_2  val;
    } PK_LOSS_RATE;

    // A choice of Class of Service (CoS) parameters
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            NUM_COS_TYPES  nct;
            // LIST(MIN_PK_TX_DELAY)
            struct l_MIN_PK_TX_DELAY {
                UNSIGNED_INT_2   length;
                MIN_PK_TX_DELAY  *elem;
            };
            // LIST(AVG_PK_TX_DELAY)
            struct l_AVG_PK_TX_DELAY {
                UNSIGNED_INT_2   length;
                AVG_PK_TX_DELAY  *elem;
            };
            // LIST(MAX_PK_TX_DELAY)
            struct l_MAX_PK_TX_DELAY {
                UNSIGNED_INT_2   length;
                MAX_PK_TX_DELAY  *elem;
            };
            // LIST(PK_DELAY_JITTER)
            struct l_APK_DELAY_JITTER {
                UNSIGNED_INT_2   length;
                PK_DELAY_JITTER  *elem;
            };
            // LIST(PK_LOSS_RATE)
            struct l_PK_LOSS_RATE {
                UNSIGNED_INT_2   length;
                PK_LOSS_RATE     *elem;
            };
        };
    } QOS_PARAM_VAL;

    // A list of Class of Service (CoS) parameters
    typedef struct {
        NUM_COS_TYPES  nct;
        // LIST(MIN_PK_TX_DELAY)
        struct l_MIN_PK_TX_DELAY{
            UNSIGNED_INT_2   length;
            MIN_PK_TX_DELAY  *elem;
        };
        // LIST(AVG_PK_TX_DELAY)
        struct l_AVG_PK_TX_DELAY{
            UNSIGNED_INT_2   length;
            AVG_PK_TX_DELAY  *elem;
        };
        // LIST(MAX_PK_TX_DELAY)
        struct l_MAX_PK_TX_DELAY{
            UNSIGNED_INT_2   length;
            MAX_PK_TX_DELAY  *elem;
        };
        // LIST(PK_DELAY_JITTER)
        struct l_PK_DELAY_JITTER{
            UNSIGNED_INT_2   length;
            PK_DELAY_JITTER  *elem;
        };
        // LIST(PK_LOSS_RATE)
        struct l_PK_LOSS_RATE{
            UNSIGNED_INT_2   length;
            PK_LOSS_RATE     *elem;
        };
    } QOS_LIST;
    
    
    //******************************************
    // Table F.4?Data types for links
    //******************************************
    
    // Represents percentage of battery charge remaining (-1..100)
    typedef INTEGER_1 BATT_LEVEL;

    // Channel identifier as defined in the specific link technology
    typedef UNSIGNED_INT_2 CHANNEL_ID;

    // The status of link parameter configuration (TRUE: Success)
    typedef BOOL CONFIG_STATUS;

    // information on manufacturer, model number, revision number, etc.
    typedef INTEGER_1 *DEVICE_INFO;

    // A list of device status request
    // Bitmap Values: Bit 0: DEVICE_INFO Bit 1: BATT_LEVEL Bit 2-15: (Reserved)
    typedef BITMAP_16 DEV_STATES_REQ;

    // Represents a device status
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            DEVICE_INFO di;
            BATT_LEVEL bl;
        };
    } DEV_STATES_RSP;

    // Time (ms) to elapse before an action needs to be taken (0 = immediately)
    typedef UNSIGNED_INT_2 LINK_AC_EX_TIME;

    // Link action result. 0: Success  1: Failure  2: Refused  3: Incapable
    typedef ENUMERATED LINK_AC_RESULT;
    
    // An action for a link  0: NONE   1: LINK_DISCONNECT   2: LINK_LOW_POWER
    // 3: LINK_POWER_DOWN  4: LINK_POWER_UP  5-255: (Reserved)
    typedef UNSIGNED_INT_1 LINK_AC_TYPE;
    
    // Link action attribute that can be executed along with valid link action
    // Bit 0: LINK_SCAN Bit 1: LINK_RES_RETAIN Bit 2: DATA_FWD Bit 3-7: (Rsrvd)
    typedef BITMAP_8 LINK_AC_ATTR;
    
    // Link action
    typedef struct {
        LINK_AC_TYPE type;
        LINK_AC_ATTR attr;
    } LINK_ACTION;

    
     
   typedef union link_param_type_value {
            LINK_PARAM_GEN     lp_gen;
            LINK_PARAM_QOS     lp_qos;
            LINK_PARAM_GG      lp_gg;
            LINK_PARAM_EDGE    lp_edge;
            LINK_PARAM_ETH     lp_eth;
            LINK_PARAM_802_11  lp_11;
            LINK_PARAM_C2K     lp_c2k;
            LINK_PARAM_FDD     lp_fdd;
            LINK_PARAM_HRPD    lp_hprd;
            LINK_PARAM_802_16  lp_16;
            LINK_PARAM_802_20  lp_20;
            LINK_PARAM_802_22  lp_22;
   } ParamTypeValue;
    
    // Measurable link parameter for which thresholds are being set
    typedef struct link_param_type_str{
        ParamTypeValue value;
        UNSIGNED_INT_1 selector;
		link_param_type_str *next;
    } LINK_PARAM_TYPE;

    // A link threshold (threshold is crossed when value of link parameter 
    // passes the threshold in the specified direction)
    typedef struct {
        THRESHOLD_VAL   t_val;
        THRESHOLD_X_DIR t_dir;
    } THRESHOLD;
    
    // Descriptors of a link
    typedef struct {
        UNSIGNED_INT_1 selector;
        //union value {
            NUM_COS   nc;
            NUM_QUEUE nq;
        //};
    } LINK_DESC_RSP;
    
    // A link configuration parameter
    typedef struct {
        LINK_PARAM_TYPE lpt;
        TIMER_INTERVAL  interval;
        TH_ACTION       tha;
        // LIST(THRESHOLD)
        struct {
            UNSIGNED_INT_2 length;
            THRESHOLD      *elem;
        };
    } LINK_CFG_PARAM;

    // Status of link parameter configuration for each threshold specified
    typedef struct {
        LINK_PARAM_TYPE lpt;
        THRESHOLD threshold;
        CONFIG_STATUS cs;
    } LINK_CFG_STATUS;

    
    
    // Represents a link parameter type and value pair

	typedef union link_param_value
		{
            LINK_PARAM_VAL lpv;
            QOS_PARAM_VAL  qpv;
        } LinkParamValue;
    typedef struct link_param_str {
        LINK_PARAM_TYPE* lpt;
        UNSIGNED_INT_1 selector;
        LinkParamValue value;
	    link_param_str* next;
    } LINK_PARAM;
    
	// Represents a link parameter report
    typedef struct link_param_rpt_str{
        LINK_PARAM* lp;
        //UNSIGNED_INT_1 selector;
        THRESHOLD* threshold;
		link_param_rpt_str* next;
    } LINK_PARAM_RPT;

    // A list of PoAs for a particular link
    // The LIST(LINK_ADDR) is a list of PoA link addresses and is sorted from 
    // most preferred first to least preferred last 
    typedef struct {
        LINK_ID lid;
        // LIST(LINK_ADDR)
        struct {
            UNSIGNED_INT_2 length;
            LINK_ADDR      *elem;
        };
    } LINK_POA_LIST;
    
    // Signal strength in dBm or relative value in arbitrary percentage scale
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            INTEGER_1   ss_dBm_units;
            PERCENTAGE  ss_perc;
        };
    } SIG_STRENGTH;
    
    // A type to represent a network identifier (non-NULL terminated string 
    // whose length shall not exceed 253 octets)
    // !!!!!!!!! MOVED FROM BELOW
    typedef OCTET_STRING NETWORK_ID;
    
    // Represents a scan response (LINK_ADDR contains PoA link address, 
    // PoA belongs to the NETWORK_ID with the given SIG_STRENGTH)
    typedef struct {
        LINK_ADDR la;
        NETWORK_ID nid;
        SIG_STRENGTH sigs;
    } LINK_SCAN_RSP;


    // The operation mode or the channel ID of the link
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            OP_MODE     opm;
            CHANNEL_ID  cid;
        };
    } LINK_STATES_RSP;
    
    // Represents the possible information to request from a link
    
	typedef struct link_param_type_list{
            UNSIGNED_INT_2   length;
            LINK_PARAM_TYPE  *elem;
			//LINK_PARAM_TYPE  *last;
    } L_LINK_PARAM_TYPE;

	typedef struct {
        LINK_STATES_REQ lsr;
        // LIST(LINK_PARAM_TYPE)
        L_LINK_PARAM_TYPE *lpt;
        LINK_DESC_REQ ldr;
    } LINK_STATUS_REQ;

    // A set of link status parameter values correspond to LINK_STATUS_REQ

	 typedef struct {
            UNSIGNED_INT_2   length;
            LINK_STATES_RSP  *elem;
        }l_LINK_STATES_RSP;

	 typedef struct {
            UNSIGNED_INT_2   length;
            LINK_PARAM       *elem;
        } l_LINK_PARAM;

	 typedef struct {
            UNSIGNED_INT_2   length;
            LINK_DESC_RSP    *elem;
        }l_LINK_DESC_RSP;

	typedef struct {
        // LIST(LINK_STATES_RSP)
       l_LINK_STATES_RSP *lkstr;
        // LIST(LINK_PARAM)
        l_LINK_PARAM *linkparam;
        // LIST(LINK_DESC_RSP)
        l_LINK_DESC_RSP *linkDescRsp;
    } LINK_STATUS_RSP;

    // The identifier of a link that is associated with a PoA
    typedef struct {
        LINK_ID    lid;
        LINK_ADDR  lad;
    } LINK_TUPLE_ID;

    






    //******************************************
    // Table F.10?Data types for location
    //******************************************
    
    // A type to represent an XML-formatted civic location (IETF RFC 4119)
    typedef OCTET_STRING XML_CIVIC_LOC;

    // Represent civic address elements (IETF RFC 4776) in BIN_CIVIC_LOC
    typedef OCTET_STRING CIVIC_ADDR;
    
    // A type to represent a binary-formatted (128b) geospatial location
    // see Table F.11?Value field format of PoA location information
    typedef INTEGER_1 BIN_GEO_LOC[16];

    // Represent an XML-formatted geospatial location  (IETF RFC 4119)
    // Example: <gml:location>
    //             <gml:Point gml:id=?point1? srsName=?epsg:4326?>
    //             <gml:coordinates>37:46:30N 122:25:10W</gml:coordinates>
    //             </gml:Point>
    //          </gml:location>
    typedef OCTET_STRING XML_GEO_LOC;

    // Country code, represented as two capital ASCII letters (ISO 3166-1)
    // !!!!!!!!! MOVED FROM BELOW
    typedef INTEGER_1 CNTRY_CODE[2];

    // A type to represent a binary-formatted civic address
    typedef struct {
        CNTRY_CODE  cc;
        CIVIC_ADDR  ca;
    } BIN_CIVIC_LOC;

    // A type to represent a civic address
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            BIN_CIVIC_LOC  bcl;
            XML_CIVIC_LOC  xcl;
        };
    } CIVIC_LOC;

    // A type to represent a geospatial location 
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            BIN_GEO_LOC  bgl;
            XML_GEO_LOC  xgl;
        };
    } GEO_LOC;

    // Represent the format and value of the location information
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            CIVIC_LOC  cl;
            GEO_LOC    gl;
            CELL_ID    cid;
        };
    } LOCATION;


    //*********************************************
    // Table F.12?Data types for IP configuration
    //*********************************************
    
    // A set of IP configuration methods  Bit 0: IPv4 static configuration
    // Bit 1: IPv4 dynamic configuration (DHCPv4)
    // Bit 2: Mobile IPv4 with foreign agent (FA) care-of address (CoA)
    // Bit 3: Mobile IPv4 without FA (Co-located CoA)
    // Bits 4-10: reserved for IPv4 address configurations
    // Bit 11: IPv6 stateless address configuration
    // Bit 12: IPv6 stateful address configuration (DHCPv6)
    // Bit 13: IPv6 manual configuration    Bits 14-31: (Reserved)
    typedef BITMAP_32 IP_CFG_MTHDS;

    // Indicates the supported mobility management protocols
    // Bit 0: Mobile IPv4 (IETF RFC 3344)
    // Bit 1: Mobile IPv4 Regional Registration (IETF RFC 4857)
    // Bit 2: Mobile IPv6 (IETF RFC 3775)
    // Bit 3: Hierarchical Mobile IPv6 (IETF RFC 4140)
    // Bit 4: Low Latency Handoffs (IETF RFC 4881)
    // Bit 5: Fast Handovers for Mobile IPv6 (IETF RFC 4068)
    // Bit 6: IKEv2 Mobility and Multihoming Protocol (IETF RFC4555)
    // Bit 7-15: (Reserved)
    typedef BITMAP_16 IP_MOB_MGMT;

    // The length of an IP subnet prefix   0..32 for IPv4 subnet.
    // 0..64, 65..127 for IPv6 subnet. (IETF RFC 4291 [B22])
    typedef UNSIGNED_INT_1 IP_PREFIX_LEN;

    // Indicates whether MN?s IP address needs to be changed (TRUE) or not
    typedef BOOL IP_RENEWAL_FLAG;

    // Represent an IP subnet (IP_PREFIX_LEN contains the bit length of the 
    // prefix of the subnet to which the IP_ADDR belongs)
    typedef struct {
        IP_PREFIX_LEN  ipl;
        IP_ADDR        ia;
    } IP_SUBNET_INFO;



    //*************************************************
    // Table F.13?Data types for information elements
    //*************************************************

    // A type to represent an auxiliary access network identifier
    // (HESSID if network type is IEEE 802.11)
    typedef OCTET_STRING NET_AUX_ID;

    // A type to represent a network identifier (non-NULL terminated string 
    // whose length shall not exceed 253 octets)
    // !!!!!!! MOVED ABOVE !!!!!!!!
    // typedef OCTET_STRING NETWORK_ID;

    // CDMA band class
    typedef UNSIGNED_INT_1 BAND_CLASS;

    // Channel bandwidth in kb/s
    typedef UNSIGNED_INT_2 BANDWIDTH;

    // Base station identifier
    typedef UNSIGNED_INT_2 BASE_ID;

    // Currency of a cost (three-letter ISO 4217 currency code, e.g., ?USD?)
    typedef INTEGER_1 COST_CURR[3];

    // Represent the unit of a cost  0: second  1: minute  2: hours  3: day 
    // 4: week  5: month  6: year  7: free  8: flat rate  9-255: (Reserved)
    typedef UNSIGNED_INT_1 COST_UNIT;
    
    // Country code, represented as two capital ASCII letters (ISO 3166-1)
    // !!!!!!!!!! MOVED ABOVE
    // typedef INTEGER_1 CNTRY_CODE[2];

    // A type to represent the maximum data rate in kb/s (Range: 0..2^32-1)
    typedef UNSIGNED_INT_4 DATA_RATE;

    // List of FEC Code Type for Downlink burst(11.4.1 in IEEE 802.16Rev2/D5.0)
    typedef BITMAP_256 DOWN_BP;

    // BS?s effective isotropic radiated power level (in units of 1 dBm)
    typedef INTEGER_1 EIRP;

    // The fully qualified domain name of a host as described in IETF RFC 2181
    typedef OCTET_STRING FQDN;

    // Downlink/Uplink center frequency in KHz
    typedef INTEGER_8 DU_CTR_FREQ;
    
    // Identifier the carrier frequency (Valid Range: 0..65535)
    typedef INTEGER_2 FREQ_ID;

    // UMTS scrambling code, cdma2000 Walsh code (Valid Range: 0..65535)
    typedef INTEGER_2 FQ_CODE_NUM;

    // HANDOVER_RANGING_CODE (Refer to 11.3.1 in IEEE 802.16Rev2/D5.0)
    typedef INTEGER_1 HO_CODE;

    // INITIAL_RANGING_CODE (Refer to 11.3.1 in IEEE 802.16Rev2/D5.0)
    typedef INTEGER_1 INIT_CODE;

    // An IPv4 address as described in IETF RFC 791
    typedef INTEGER_1 IP4_ADDR[4];

    // An IPv6 address as described in IETF RFC 2373
    typedef INTEGER_1 IP6_ADDR[16];

    // Operator name (uniquely identified within scope of OP_NAMESPACE, 
    // a non NULL terminated string, length shall not exceed 253 octets)
    typedef OCTET_STRING OP_NAME;

    // A type to represent a type of operator name   0: GSM/UMTS   1: CDMA
    // 2: REALM   3: ITU-T/TSB   4: General   5-255: (Reserved)
    typedef UNSIGNED_INT_1 OP_NAMESPACE;

    // Pilot PN sequence offset index
    typedef INTEGER_2 PILOT_PN;

    // A network subtype (See Table F.14)
    typedef BITMAP_64 SUBTYPE;

    // Service provider identifier (non-NULL terminated string, 
    // length shall not exceed 253 octets)
    typedef OCTET_STRING SP_ID;

    // Represent supported Location Configuration Protocol   1: LLDP
    // 2: LbyR with LLDP   3-10: (Reserved)   11: LLDP-MED
    // 12: LbyR with LLDP-MED   13-20: (Reserved)   21: U-TDoA   22: D-TDoA
    // 23-30: (Reserved)   31: DHCP   32: LbyR with DHCP   33-40: (Reserved)
    // 41: OMA SUPL   42: IEEE 802.11 LCI   43-50: (Reserved)   51: HELD
    // 52: LbyR with HELD   53-255: (Reserved)
    typedef UNSIGNED_INT_1 SUPPORTED_LCP;

    // Generic type extension contained indicating a flexible length and format 
    // field (content is to be defined and filled by the appropriate SDO or 
    // service provider consortium, etc., non-NULL terminated string, 
    // length shall not exceed 253 octets)
    typedef OCTET_STRING TYPE_EXT;

    // List of FEC Code Type for Uplink burst (11.3.1 in IEEE 802.16Rev2/D5.0)
    typedef BITMAP_256 UP_BP;

    // These bits provide high level capabilities supported on a network
    // Bit 0: Security        Bit 1: QoS Class 0      Bit 2: QoS Class 1 
    // Bit 3: QoS Class 2     Bit 4: QoS Class 3      Bit 5: QoS Class 4 
    // Bit 6: QoS Class 5     Bit 7: Internet Access  Bit 8: Emergency Services
    // Bit 9: MIH Capability  Bit 10-31: (Reserved)
    typedef BITMAP_32 NET_CAPS;

    // Burst profile
    typedef struct {
        DOWN_BP  dbp;
        UP_BP    ubp;
    } BURST_PROF;

    // A type that contains two range values (in KHz) 
    // (first value should always be less than or equal to the second value)
    typedef struct {
        UNSIGNED_INT_4  low_range;
        UNSIGNED_INT_4  high_range;
    } CH_RANGE;

    // Represent the value of a cost (fraction part is a 3-digit fraction,
    // the value range of the fraction part is [0,999], for example, 
    // for a value of ?0.5?, integer part is zero and fraction part is 500)
    typedef struct {
        UNSIGNED_INT_4 integer_part;
        UNSIGNED_INT_2 fraction_part;
    } COST_VALUE;

    // A type to represent a cost
    typedef struct {
        COST_UNIT   cu;
        COST_VALUE  cv;
        COST_CURR   cc;
    } COST;

    // This gap is an integer number of physical slot durations and starts 
    // on a physical slot boundary (used on TDD systems only)
    typedef struct {
        UNSIGNED_INT_2   transmitreceive_transition_gap;
        UNSIGNED_INT_1   receivetransmit_transition_gap;
    } GAP;

    // A list of frequency bands in KHz
    // LIST(UNSIGNED_INT_4)
    typedef struct {
        UNSIGNED_INT_2   length;
        UNSIGNED_INT_4   *elem;
    } FREQ_BANDS;

    // A set of CDMA ranging codes
    typedef struct {
        INIT_CODE  inc;
        HO_CODE    hoc;
    } CDMA_CODES;

    // Represent the downlink channel descriptor and uplink channel descriptor
    typedef struct {
        BASE_ID      bid;
        BANDWIDTH    bw;
        DU_CTR_FREQ  dcf;
        EIRP         eipr;
        GAP          gap;
        BURST_PROF   bp;
        CDMA_CODES   cc;
    } DCD_UCD;

    
    // IP address of candidate DHCP Server (only included when dynamic address 
    // configuration is supported)
    // !!!!!!!!!!!! MOVED FROM BELOW
    typedef IP_ADDR DHCP_SERV;

    // IP address of candidate Foreign Agent (only included when Mobile IPv4 
    // is supported)
    // !!!!!!!!!!!! MOVED FROM BELOW
    typedef IP_ADDR FN_AGNT;

    // IP address of candidate Access Router (only included when IPv6 Stateless 
    // configuration is supported.
    // !!!!!!!!!!!! MOVED FROM BELOW
    typedef IP_ADDR ACC_RTR;

    // IP Configuration Methods supported by the access network
    typedef struct {
        IP_CFG_MTHDS   icm;
        DHCP_SERV      ds;
        FN_AGNT        fa;
        ACC_RTR        ar;
    } IP_CONFIG;

    // A type to represent a network type and its subtype (Table F.14)

    typedef struct NETWORK_TYPES_str {
        LINK_TYPE   lt;
        SUBTYPE     st;
        TYPE_EXT    te;
	} NETWORK_TYPES;



    // A type to represent an operator identifier
    typedef struct {
        OP_NAME       on;
        OP_NAMESPACE  ons;
    } OPERATOR_ID;

    // A type to represent UMTS system information block (SIB)
    typedef struct {
        CELL_ID      cid;
        FQ_CODE_NUM  fcn;
    } SIB;

    // CDMA2000 system parameters
    typedef struct {
        BASE_ID     bid;
        PILOT_PN    ppn;
        FREQ_ID     fid;
        BAND_CLASS  bc;
    } SYS_PARAMS;
    
    // Data type to represent system information depending on the network type
    // (DCD_UCD: IEEE 802.16   SIB: UMTS   SYS_PARAMS: cdma2000)
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            DCD_UCD     du;
            SIB         sib;
            SYS_PARAMS  sp;
        };
    } PARAMETERS;

    // L3 address of a proxy server
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            IP4_ADDR  ip4;
            IP6_ADDR  ip6;
            FQDN      fqdn;
        };
    } PROXY_ADDR;

    // Represent a regulatory domain (identified by a country code, CNTRY_CODE,
    // and a regulatory class, UNSIGNED_INT_1, cf. Annex J of IEEE Std 802.11k)
    typedef struct {
        CNTRY_CODE      cc;
        UNSIGNED_INT_1  rc;
    } REGU_DOMAIN;

    // A list of roaming partners
    // LIST(OPERATOR_ID)
    typedef struct {
        UNSIGNED_INT_2   length;
        OPERATOR_ID      *elem;
    } ROAMING_PTNS;

    // A type to represent system information
    typedef struct {
        NETWORK_TYPES  net;
        LINK_ADDR     lad;
        PARAMETERS    par;
    } SYSTEM_INFOS;


    //******************************************
    // Table F.15?Data types for binary query
    //******************************************
    

    // A type to indicate currency preference
    typedef COST_CURR CURR_PREF;

    // A type to represent an IE type (See Table G.1 for more information)
    typedef UNSIGNED_INT_4 IE_TYPE;

    // Radius in meters from center point of querier's location (0..2^32-1)
    typedef UNSIGNED_INT_4 NGHB_RADIUS;

    // A type to represent a set of link types   Bit 0: Wireless - GSM
    // Bit 1: Wireless - GPRS         Bit 2: Wireless - EDGE
    // Bit 3: IEEE 802.3 (Ethernet)   Bit 4: Wireless - Other
    // Bit 5: Wireless - IEEE 802.11  Bit 6: Wireless - CDMA2000
    // Bit 7: Wireless - UMTS         Bit 8: Wireless - cdma2000-HRPD
    // Bit 9: Wireless - IEEE 802.16  Bit 10: Wireless - IEEE 802.20
    // Bit 11: Wireless - IEEE 802.22 Bit 12-31: (Reserved AND shall be ?0?)
    typedef BITMAP_32 NET_TYPE_INC;

    // A type to represent a querier's location (it is not valid to use both 
    // LOCATION and LINK_ADDR at the same time
    typedef struct {
        LOCATION     loc;
        LINK_ADDR    lad;
        NGHB_RADIUS  nr;
    } QUERIER_LOC;

    // A type to represent a list of network identifiers
    // LIST(NETWORK_ID)
    typedef struct {
        UNSIGNED_INT_2   length;
        NETWORK_ID       *elem;
    } NETWK_INC;

    // A type to represent a report limitation (first UNSIGNED_INT_2 contains 
    // the maximum number of IEs in the IR_BIN_DATA, second UNSIGNED_INT_2 
    // contains the starting entry number from which a chunk of entries are
    // to be included in the IQ_BIN_DATA, offset = 1 points to first entry)
    typedef struct {
        UNSIGNED_INT_2   maximum_number_of_IEs;
        UNSIGNED_INT_2   starting_entry_number;
    } RPT_LIMIT;

    // Represent a list of IE types (Inclusion of any IE type is optional)
    // LIST(IE_TYPE)
    typedef struct {
        UNSIGNED_INT_2   length;
        IE_TYPE          *elem;
    } RPT_TEMPL;

    // Represents a binary query
    typedef struct {
        QUERIER_LOC   qc;
        NET_TYPE_INC  nti;
        NETWK_INC     ni;
        RPT_TEMPL     rt;
        RPT_LIMIT     rl;
        CURR_PREF     cp;
    } IQ_BIN_DATA;


    //******************************************
    // Table F.16?Data type for RDF query
    //******************************************
    
    // A type to represent the URL of an RDF schema to obtain
    typedef OCTET_STRING IQ_RDF_SCHM;

    // Represents MIME type
    typedef OCTET_STRING MIME_TYPE;

    // Represents RDF query (If MIME_TYPE omitted, ?application/sparql-query? 
    // MIME type is used, OCTET_STRING is formatted with the MIME type)
    typedef struct {
        MIME_TYPE     mt;
        OCTET_STRING  query;
    } IQ_RDF_DATA;



    //*************************************************************
    // Table F.17?Data type for binary information query response
    //*************************************************************
    
    // A type to represent a binary query response data
    // LIST(INFO_ELEMENT)
    typedef struct {
        UNSIGNED_INT_2   length;
        INFO_ELEMENT     *elem;
    } IR_BIN_DATA;



    //**********************************************************
    // Table F.18?Data type for RDF information query response
    //**********************************************************
    
    // Represents RDF data query result (If MIME_TYPE omitted, 
    // ?application/ sparqlresults+xml? MIME type is used
    typedef struct {
        MIME_TYPE     mt;
        OCTET_STRING  query;
    } IR_RDF_DATA;

    // An URL of an RDF schema
    typedef OCTET_STRING IR_SCHM_URL;

    // Represents RDF schema (If MIME_TYPE omitted, ?application/xml? 
    // MIME type is used, OCTET_STRING is formatted with the MIME type)
    typedef struct {
        MIME_TYPE     mt;
        OCTET_STRING  query;
    } IR_RDF_SCHM;



    //***********************************************
    // Table F.19?Data type for MIHF identification
    //***********************************************
    
    // The MIHF Identifier (MIHF_ID is a network access identifier, NAI,
    // NAI shall be unique as per IETF RFC 4282
    // The maximum length is 253 octets
    typedef OCTET_STRING MIHF_ID;


    //***********************************************
    // Table F.20?Data type for MIH capabilities
    //***********************************************
    
    // A data type for configuring link detected event trigger
    typedef struct {
        NETWORK_ID      nid;
        SIG_STRENGTH    ss;
        LINK_DATA_RATE  ldr;
    } LINK_DET_CFG;

    // Represents additional configuration information for event subscription
    typedef struct {
        UNSIGNED_INT_1  selector;
        union value {
            // LIST(LINK_DET_CFG)
            struct l_LINK_DET_CFG {
                UNSIGNED_INT_2   length;
                LINK_DET_CFG     *elem;
            };
            // LIST(LINK_CFG_PARAM)
            struct l_LINK_CFG_PARAM {
                UNSIGNED_INT_2   length;
                LINK_CFG_PARAM     *elem;
            };
        };
    } EVT_CFG_INFO;

    // Information of a detected link
    typedef struct {
        LINK_TUPLE_ID     ltid;
        NETWORK_ID        nid;
        NET_AUX_ID        naid;
        SIG_STRENGTH      ss;
        UNSIGNED_INT_2    snr;
        LINK_DATA_RATE    ldr;
        LINK_MIHCAP_FLAG  lmf;
        NET_CAPS          nc;
    } LINK_DET_INFO;

    // Indicates if make before break (MBB) is supported FROM first network
    // TO the second network (BOOL value assignment: True: MBB is supported)
    typedef struct {
        NETWORK_TYPES   nt1;
        NETWORK_TYPES   nt2;
        BOOL        mbb_supported;
    } MBB_HO_SUPP;

    // A list of MIH commands   Bit 0: MIH_Link_Get_Parameters
    // Bit 1: MIH_Link_Configure_Thresholds   Bit 2: MIH_Link_Actions
    // Bit 3: MIH_Net_HO_Candidate_Query   MIH_Net_HO_Commit
    //        MIH_N2N_HO_Query_Resources   MIH_N2N_HO_Commit
    //        MIH_N2N_HO_Complete          
    // Bit 4: MIH_MN_HO_Candidate_Query    MIH_MN_HO_Commit    
    //        MIH_MN_HO_Complete
    // Bit 5-31: (Reserved)
    typedef BITMAP_32 MIH_CMD_LIST;

    // A list of MIH events     Bit 0: MIH_Link_Detected   Bit 1: MIH_Link_Up
    // Bit 2: MIH_Link_Down     Bit 3: MIH_Link_Parameters_Report
    // Bit 4: MIH_Link_Going_Down          Bit 5: MIH_Link_Handover_Imminent
    // Bit 6: MIH_Link_Handover_Complete   Bit 7: MIH_Link_PDU_Transmit_Status
    // Bit 8-31: (Reserved)
    typedef BITMAP_32 MIH_EVT_LIST;

    // A list of IS query types   Bit 0: Binary data   Bit 1: RDF data
    // Bit 2: RDF schema URL      Bit 3: RDF schema    Bit 4: IE_NETWORK_TYPES
    // Bit 5: IE_OPERATOR_ID      Bit 6: IE_SERVICE_PROVIDER_ID
    // Bit 7: IE_COUNTRY_CODE     Bit 8: IE_NETWORK_ID
    // Bit 9: IE_NETWORK_AUX_ID   Bit 10: IE_ROAMING_PARTNERS
    // Bit 11: IE_COST            Bit 12: IE_NETWORK_QOS
    // Bit 13: IE_NETWORK_DATA_RATE    Bit 14: IE_NET_REGULT_DOMAIN
    // Bit 15: IE_NET_FREQUENCY_BANDS  Bit 16: IE_NET_IP_CFG_METHODS
    // Bit 17: IE_NET_CAPABILITIES     Bit 18: IE_NET_SUPPORTED_LCP
    // Bit 19: IE_NET_MOB_MGMT_PROT    Bit 20: IE_NET_EMSERV_PROXY
    // Bit 21: IE_NET_IMS_PROXY_CSCF   Bit 22: IE_NET_MOBILE_NETWORK
    // Bit 23: IE_POA_LINK_ADDR        Bit 24: IE_POA_LOCATION
    // Bit 25: IE_POA_CHANNEL_RANGE    Bit 26: IE_POA_SYSTEM_INFOS
    // Bit 27: IE_POA_SUBNET_INFO      Bit 28: IE_POA_IP_ADDR
    // Bit 29- 63: (Reserved)
    typedef BITMAP_64 MIH_IQ_TYPE_LST;

    // List of supported transports   Bit 0: UDP  Bit 1: TCP  Bit 2-15: (Rsrvd)
    typedef BITMAP_16 MIH_TRANS_LST;
 
    // Represent a link address of a specific network type
    typedef struct {
        NETWORK_TYPES   nt;
        LINK_ADDR      lad;
    } NET_TYPE_ADDR;



    //***********************************************
    // Table F.21?Data type for MIH registration
    //***********************************************
    
    // The registration code:   0 - Registration   1 - Re-Registration
    typedef ENUMERATED REG_REQUEST_CODE;


    //***********************************************
    // Table F.22?Data type for handover operation
    //***********************************************
    
    // Represents the reason for performing a handover   
    // Same enumeration list as LINK_DN_REASON coden    0: Explicit disconnect  
    // 1: Packet timeout   2: No resource   3: No broadcast   4: Auth. failure   
    // 5: Billing failure   6-127: (Reserved)  128-255: Vendor specific reason
    typedef UNSIGNED_INT_1 HO_CAUSE;
	//typedef UNSIGNED_INT_1 LINK_DN_REASON;


    // Handover result   0: Success   1: Failure   2: Rejected
    typedef ENUMERATED HO_RESULT;

    // Permission for handover   0: HandoverPermitted  1: HandoverDeclined
    typedef ENUMERATED HO_STATUS;

    // Pre-defined configuration identifier   0..255
    typedef INTEGER_1 PREDEF_CFG_ID;

    // IP address of candidate DHCP Server (only included when dynamic address 
    // configuration is supported)
    // !!!!!!!!!!!! MOVED ABOVE
    // typedef IP_ADDR DHCP_SERV;

    // IP address of candidate Foreign Agent (only included when Mobile IPv4 
    // is supported)
    // !!!!!!!!!!!! MOVED ABOVE
    // typedef IP_ADDR FN_AGNT;

    // IP address of candidate Access Router (only included when IPv6 Stateless 
    // configuration is supported.
    // !!!!!!!!!!!! MOVED ABOVE
    // typedef IP_ADDR ACC_RTR;

    // Transparent carrier containing link specific information whose content 
    // and format are to be specified by the link specific SDO
    typedef OCTET_STRING TSP_CARRIER;

    // Transparent container 
    typedef struct {
        UNSIGNED_INT_1 selector;
        union value {
            PREDEF_CFG_ID  pci;
            TSP_CARRIER    tc;
        };
    } TSP_CONTAINER;

    // Set of resource parameters reserved and assigned by target network 
    // to the MN for performing handover to a network PoA
    typedef struct {
        QOS_LIST       qosl;
        TSP_CONTAINER  tspc;
    } ASGN_RES_SET;

    // Represents the result of network resource query
    typedef struct {
        LINK_POA_LIST  lpl;
        IP_CFG_MTHDS   icm;
        FN_AGNT        fna;
        ACC_RTR        acc;
    } RQ_RESULT;

    // Set of resource parameters required for performing admission control 
    // and resource reservation for the MN at the target network
    typedef struct {
        QOS_LIST       qosl;
        TSP_CONTAINER  tspc;
        HO_CAUSE       hoc;
    } REQ_RES_SET;

     // Handover commit information (LINK_ADDR is target PoA link address)
	typedef union tgt_net_info {
            NETWORK_ID  nid;
            NET_AUX_ID  naid;
            LINK_ADDR   lad;
    } TgtNetInfoValue;

    typedef struct tgt_net_info_str{
        UNSIGNED_INT_1 selector;
        TgtNetInfoValue value;
        tgt_net_info* next; 
    } TGT_NET_INFO;



    //***********************************************
    // Table F.23?Data type for MIH_NET_SAP primitives
    //***********************************************
    
    // The transport type supported:    0: L2   1: L3 or higher layer protocols
    typedef ENUMERATED TRANSPORT_TYPE;


    
    // *******************************************************
    //   !!!!!!!!!!!!!! Moved down structures !!!!!!!!!!!!
    // *******************************************************
    
    
    // A set of handover action request parameters
    // LINK_ADDR provide PoA addr when LINK_ACTION contains attr 4 DATA_FWD_REQ
    typedef struct {
        LINK_ID* lid;
        LINK_ADDR* lad;
        LINK_ACTION lac;
        LINK_AC_EX_TIME laet;
    } LINK_ACTION_REQ;

	typedef struct {
            UNSIGNED_INT_2 length;
            LINK_SCAN_RSP *elem;
        } L_LINK_SCAN_RSP;
    // A set of link action returned results
    typedef struct {
        LINK_ID lid;
        LINK_AC_RESULT lat;
        // LIST(LINK_SCAN_RSP)
        L_LINK_SCAN_RSP* linkScanRsp;
    } LINK_ACTION_RSP;


    // *******************************************************
    //   LIST(DATATYPE) for MIH_SAP primitives
    // *******************************************************
    
	///PRIMITIVE PT MIH_LINK_SAP


	///////////////////////
    typedef struct {
        UNSIGNED_INT_2  length;
        NET_TYPE_ADDR   *elem;
    } L_NET_TYPE_ADDR;

    typedef struct {
        UNSIGNED_INT_2  length;
        MBB_HO_SUPP      *elem;
    } L_MBB_HO_SUPP;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        LINK_ID         *elem;
    } L_LINK_ID;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        EVT_CFG_INFO    *elem;
    } L_EVT_CFG_INFO;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        LINK_DET_INFO   *elem;
    } L_LINK_DET_INFO;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        LINK_PARAM_RPT  *elem;
       // l_link_param_rpt_str* next;

    } L_LINK_PARAM_RPT;
    
    typedef struct {
        UNSIGNED_INT_2   length;
        LINK_STATUS_RSP  *elem;
    } L_LINK_STATUS_RSP;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        LINK_CFG_PARAM  *elem;
    } L_LINK_CFG_PARAM;
    
    typedef struct {
        UNSIGNED_INT_2   length;
        LINK_CFG_STATUS  *elem;
    } L_LINK_CFG_STATUS;
    
    typedef struct {
        UNSIGNED_INT_2   length;
        LINK_ACTION_REQ  *elem;
    } L_LINK_ACTION_REQ;
    
    typedef struct {
        UNSIGNED_INT_2   length;
        LINK_ACTION_RSP  *elem;
    } L_LINK_ACTION_RSP;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        LINK_POA_LIST   *elem;
    } L_LINK_POA_LIST;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        RQ_RESULT       *elem;
    } L_RQ_RESULT;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        TGT_NET_INFO    *elem;
    } L_TGT_NET_INFO;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        IQ_BIN_DATA     *elem;
    } L_IQ_BIN_DATA;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        IQ_RDF_DATA     *elem;
    } L_IQ_RDF_DATA;
    
    typedef struct {
        UNSIGNED_INT_2  length;
        IQ_RDF_SCHM     *elem;
    } L_IQ_RDF_SCHM;
    



//#endif // !defined(EA_118E949B_DBD8_466c_B8AB_CDD31388C3A5__INCLUDED_)


