/*
 * smeDebug.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
 *                                                                       
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file smeDbg.c
 *  \brief This file include the SME debug module implementation
 *  \
 *  \date 13-February-2006
 */

#include "smePrivate.h"
#include "smeDebug.h"
#include "report.h"

void printSmeDbgFunctions (void);
void sme_dbgPrintObject (TI_HANDLE hSme);
void sme_printStats (TI_HANDLE hSme);
void sme_resetStats(TI_HANDLE hSme);
void sme_printBssidList(TI_HANDLE hSme);

#define CHAN_FREQ_TABLE_SIZE        (sizeof(ChanFreq) / sizeof(struct CHAN_FREQ))

struct  CHAN_FREQ {
    unsigned char  chan;
    unsigned long  freq;
} ChanFreq[] = {
    {1,2412000}, {2,2417000}, {3,2422000}, {4,2427000},
    {5,2432000}, {6,2437000}, {7,2442000}, {8,2447000},
    {9,2452000},
    {10,2457000}, {11,2462000}, {12,2467000}, {13,2472000},
    {14,2484000}, {36,5180000}, {40,5200000}, {44,5220000},
    {48,5240000}, {52,5260000}, {56,5280000}, {60,5300000},
    {64,5320000},
    {100,5500000}, {104,5520000}, {108,5540000}, {112,5560000},
    {116,5580000}, {120,5600000}, {124,5620000}, {128,5640000},
    {132,5660000}, {136,5680000}, {140,5700000}, {149,5745000},
    {153,5765000}, {157,5785000}, {161,5805000} };

TI_UINT32 scanResultTable_CalculateBssidListSize (TI_HANDLE hScanResultTable, TI_BOOL bAllVarIes);
TI_STATUS scanResultTable_GetBssidList (TI_HANDLE hScanResultTable, 
                                        OS_802_11_BSSID_LIST_EX *pBssidList, 
                                        TI_UINT32 *pLength, 
                                        TI_BOOL bAllVarIes);

/** 
 * \fn     smeDebugFunction
 * \brief  Main SME debug function
 * 
 * Main SME debug function
 * 
 * \param  hSme - handle to the SME object
 * \param  funcType - the specific debug function
 * \param  pParam - parameters for the debug function
 * \return None
 */
void smeDebugFunction (TI_HANDLE hSme, TI_UINT32 funcType, void *pParam)
{
    switch (funcType)
    {
    case DBG_SME_PRINT_HELP:
        printSmeDbgFunctions();
        break;

    case DBG_SME_PRINT_OBJECT:
        sme_dbgPrintObject( hSme );
        break;

    case DBG_SME_PRINT_STATS:
        sme_printStats( hSme );
        break;

    case DBG_SME_CLEAR_STATS:
        sme_resetStats( hSme );
        break;

   case DBG_SME_BSSID_LIST:
	sme_printBssidList( hSme );
	break;

	default:
   		WLAN_OS_REPORT(("Invalid function type in SME debug function: %d\n", funcType));
        break;
    }
}

int sme_strlen(char *s)
{
    int x=0;
    while (*s++)
        x++;
    return(x);
}


char* sme_strcpy(char *s1,char *s2)
{
    while (*s2)
    {
        *s1++ = *s2++;
    }
    *s1 = '\0';

	return s1;
}


int sme_memcmp(char* s1, char* s2, int n)
{
	while(n-- > 0 && *s1 == *s2)
		s1++, s2++;

	return( n < 0 ? 0 : *s1 - *s2 );
}




/** 
 * \fn     printSmeDbgFunctions
 * \brief  Print the SME debug menu
 * 
 * Print the SME debug menu
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */
void printSmeDbgFunctions(void)
{
    WLAN_OS_REPORT(("   SME Debug Functions   \n"));
	WLAN_OS_REPORT(("-------------------------\n"));
	WLAN_OS_REPORT(("1900 - Print the SME Debug Help\n"));
	WLAN_OS_REPORT(("1901 - Print the SME object\n"));
	WLAN_OS_REPORT(("1902 - Print the SME statistics\n"));
    WLAN_OS_REPORT(("1903 - Reset the SME statistics\n"));
	WLAN_OS_REPORT(("1904 - Print BSSID list\n"));
}

#ifdef REPORT_LOG
static TI_UINT8 Freq2Chan(TI_UINT32 freq)
{
    TI_UINT32 i;

    for(i=0; i<CHAN_FREQ_TABLE_SIZE; i++)
        if(ChanFreq[i].freq == freq) 
            return ChanFreq[i].chan;

    return 0;
}
#endif

static void PrintBssidList(OS_802_11_BSSID_LIST_EX* bssidList, TI_UINT32 IsFullPrint, TMacAddr CurrentBssid)
{
    TI_UINT32 i;
    TI_INT8  connectionTypeStr[50];
    POS_802_11_BSSID_EX pBssid = &bssidList->Bssid[0];

    WLAN_OS_REPORT(("BssId List: Num=%u\n", bssidList->NumberOfItems));
    WLAN_OS_REPORT(("         MAC        Privacy Rssi  Mode    Channel    SSID\n"));
    for(i=0; i<bssidList->NumberOfItems; i++)
    {            
        switch (pBssid->InfrastructureMode)
        {
            case os802_11IBSS:
                sme_strcpy (connectionTypeStr, "Adhoc");
                break;
            case os802_11Infrastructure:
                sme_strcpy (connectionTypeStr, "Infra");
                break;
            case os802_11AutoUnknown:
                sme_strcpy (connectionTypeStr, "Auto");
                break;
            default:
                sme_strcpy (connectionTypeStr, " --- ");
                break;
        }
        WLAN_OS_REPORT(("%s%02x.%02x.%02x.%02x.%02x.%02x   %3u   %4d   %s %6d       %s\n",
            (!sme_memcmp(CurrentBssid, pBssid->MacAddress, MAC_ADDR_LEN))?"*":" ",
            pBssid->MacAddress[0],
            pBssid->MacAddress[1],
            pBssid->MacAddress[2],
            pBssid->MacAddress[3],
            pBssid->MacAddress[4],
            pBssid->MacAddress[5],
            pBssid->Privacy, 
            pBssid->Rssi,
            connectionTypeStr,
            Freq2Chan(pBssid->Configuration.Union.channel),
            (pBssid->Ssid.Ssid[0] == '\0')?(TI_INT8*)"****":((TI_INT8*)pBssid->Ssid.Ssid) ));

        if (IsFullPrint)
        {
            WLAN_OS_REPORT(("   BeaconInterval %d\n",  pBssid->Configuration.BeaconPeriod));
            WLAN_OS_REPORT(("   Capabilities   0x%x\n",  pBssid->Capabilities));
        }
#ifdef _WINDOWS /*temp fix until bringing the dual OS fix*/
		pBssid = (POS_802_11_BSSID_EX)((TI_INT8*)pBssid + (pBssid->Length ? pBssid->Length : sizeof(OS_802_11_BSSID_EX)));
#else /*for Linux*/
		pBssid = &bssidList->Bssid[i+1];
#endif
    }        
}

void sme_printBssidList(TI_HANDLE hSme)
{
	TSme* sme = (TSme*) hSme;
	TI_UINT32 length;
	TI_UINT8* blist;
	TMacAddr temp_bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	length = scanResultTable_CalculateBssidListSize (sme->hScanResultTable, TI_FALSE);

	blist = os_memoryAlloc(NULL, length);

	if(!blist) 
    {
		WLAN_OS_REPORT(("ERROR. sme_printBssidList(): Cannot allocate memory!! length = %d\n", length));
		return;
	}

	scanResultTable_GetBssidList (sme->hScanResultTable, (POS_802_11_BSSID_LIST_EX)blist, &length, TI_FALSE);

	PrintBssidList((OS_802_11_BSSID_LIST_EX*)blist, 0, temp_bssid);

	os_memoryFree(NULL, blist, length);
}

/** 
 * \fn     sme_dbgPrintObject
 * \brief  Print the SME object
 * 
 * Print the SME object
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */
void sme_dbgPrintObject (TI_HANDLE hSme)
{
    WLAN_OS_REPORT(("Not yet implemented!\n"));
}

/** 
 * \fn     sme_printStats
 * \brief  Print the SME statistics
 * 
 * Print the SME statistics
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */
void sme_printStats (TI_HANDLE hSme)
{
    WLAN_OS_REPORT(("Not yet implemented!\n"));
}

/** 
 * \fn     sme_resetStats
 * \brief  Reset the SME statistics
 * 
 * Reset the SME statistics
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */
void sme_resetStats(TI_HANDLE hSme)
{
    WLAN_OS_REPORT(("Not yet implemented!\n"));
}

