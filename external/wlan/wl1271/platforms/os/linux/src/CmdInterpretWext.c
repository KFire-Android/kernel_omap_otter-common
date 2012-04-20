/*
 * CmdInterpretWext.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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


#include "tidef.h"
#include "WlanDrvIf.h"
#include "tiwlnif.h"
#include "osDot11.h"
#include "802_11Defs.h"
#include "paramOut.h"
#include "coreDefaultParams.h"
#include "version.h"
#include "osApi.h"
#include "CmdHndlr.h"
#include "CmdInterpret.h"
#include "CmdInterpretWext.h"
#include "TI_IPC_Api.h"
#include "WlanDrvIf.h"
#include <linux/wireless.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>
#include <net/iw_handler.h>
#include "privateCmd.h"
#include "DrvMain.h"
#include "CmdDispatcher.h"
#include "EvHandler.h"
#include "admCtrl.h"
#include "freq.h"

static TI_INT32 cmdInterpret_Event(IPC_EV_DATA* pData);
static int cmdInterpret_setSecurityParams (TI_HANDLE hCmdInterpret);
static int cmdInterpret_initEvents(TI_HANDLE hCmdInterpret);
static int cmdInterpret_unregisterEvents(TI_HANDLE hCmdInterpret, TI_HANDLE hEvHandler);

#define WEXT_FREQ_CHANNEL_NUM_MAX_VAL	1000
#define WEXT_FREQ_KHZ_CONVERT			3
#define WEXT_FREQ_MUL_VALUE				500000
#define WEXT_MAX_RATE_VALUE				63500000
#define WEXT_MAX_RATE_REAL_VALUE		65000000

#define CHECK_PENDING_RESULT(x,y)     if (x == COMMAND_PENDING) { os_printf ("Unexpected COMMAND PENDING result (cmd = 0x%x)\n",y->paramType);  break; }
#define CALCULATE_RATE_VALUE(x)                   ((x & 0x7f) * WEXT_FREQ_MUL_VALUE);

static const char *ieee80211_modes[] = {
    "?", "IEEE 802.11 B", "IEEE 802.11 A", "IEEE 802.11 BG", "IEEE 802.11 ABG"
};
#ifdef XCC_MODULE_INCLUDED
typedef struct
{

    TI_UINT8        *assocRespBuffer;
    TI_UINT32       assocRespLen;
} cckm_assocInformation_t;

#define ASSOC_RESP_FIXED_DATA_LEN 6
/* 1500 is the recommended size by the Motorola Standard team. TI recommendation is 700 */
#define MAX_BEACON_BODY_LENGTH    1500
#define BEACON_HEADER_FIX_SIZE    12
#define CCKM_START_EVENT_SIZE     23 /* cckm-start string + timestamp + bssid + null */
#endif

/* Initialize the CmdInterpreter module */
TI_HANDLE cmdInterpret_Create (TI_HANDLE hOs)
{
    cmdInterpret_t *pCmdInterpret;

    /* Allocate memory for object */
    pCmdInterpret = os_memoryAlloc (hOs, sizeof(cmdInterpret_t));

    /* In case of failure -> return NULL */
    if (!pCmdInterpret)
    {
        os_printf ("cmdInterpret_init: failed to allocate memory...aborting\n");
        return NULL;
    }

    /* Clear all fields in cmdInterpreter module object */
    os_memoryZero (hOs, pCmdInterpret, sizeof (cmdInterpret_t));

    /* Save handlers */
    pCmdInterpret->hOs = hOs;

    /* Return pointer to object */
    return (TI_HANDLE)pCmdInterpret;
}


/* Deinitialize the cmdInterpreter module */
TI_STATUS cmdInterpret_Destroy (TI_HANDLE hCmdInterpret, TI_HANDLE hEvHandler)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;

    /* Unregister events */
    cmdInterpret_unregisterEvents ((TI_HANDLE)pCmdInterpret, hEvHandler);

    /* Release allocated memory */
    os_memoryFree (pCmdInterpret->hOs, pCmdInterpret, sizeof(cmdInterpret_t));

    return TI_OK;
}


void cmdInterpret_Init (TI_HANDLE hCmdInterpret, TStadHandlesList *pStadHandles)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;

    pCmdInterpret->hCmdHndlr    = pStadHandles->hCmdHndlr;
    pCmdInterpret->hEvHandler   = pStadHandles->hEvHandler;
    pCmdInterpret->hCmdDispatch = pStadHandles->hCmdDispatch;

    /* Register to driver events */
    cmdInterpret_initEvents (hCmdInterpret);
}


/* Handle a single command */
int cmdInterpret_convertAndExecute(TI_HANDLE hCmdInterpret, TConfigCommand *cmdObj)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;
    paramInfo_t *pParam;
    TI_STATUS res = TI_NOK;
    int i,j;

    union iwreq_data *wrqu = (union iwreq_data *)cmdObj->buffer1;

    cmdObj->return_code = WEXT_NOT_SUPPORTED;
    pParam = (paramInfo_t *)os_memoryAlloc(pCmdInterpret->hOs, sizeof(paramInfo_t));
    if (!pParam)
        return res;
    /* Check user request */
    switch (cmdObj->cmd)
    {

        /* get name == wireless protocol - used to verify the presence of Wireless Extensions*/
    case SIOCGIWNAME:
        os_memoryCopy(pCmdInterpret->hOs, cmdObj->buffer1, WLAN_PROTOCOL_NAME, IFNAMSIZ);
        res = TI_OK;
        break;

        /* Set channel / frequency */
    case SIOCSIWFREQ:
        {
			int freq = wrqu->freq.m;

            /* If the input is frequency convert it to channel number -
				See explanation in [struct iw_freq] definition in wireless_copy.h*/
            if (freq >= WEXT_FREQ_CHANNEL_NUM_MAX_VAL)
            {
				int div = WEXT_FREQ_KHZ_CONVERT - wrqu->freq.e;
                /* Convert received frequency to a value in KHz*/
				if (div >= 0)
                {
                    while (div-- > 0)
                    {
                        freq /= 10;             /* down convert to KHz */
                    }
                }
                else
                {
                    while (div++ < 0)          /* up convert to KHz */
                    {
                        freq *= 10;
                    }
                }
				
                /* Convert KHz frequency to channel number*/
                freq = Freq2Chan(freq); /* convert to chan num */
            }
                
            /* If there is a given channel */
            if (freq != 0)
            {
                pParam->paramType = SITE_MGR_DESIRED_CHANNEL_PARAM;
                pParam->paramLength = sizeof(TI_UINT32);
                pParam->content.siteMgrDesiredChannel = freq;

                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
                CHECK_PENDING_RESULT(res,pParam)
            }
            break;
        }

        /* Get channel / frequency */
    case SIOCGIWFREQ:
        {
            pParam->paramType = SITE_MGR_CURRENT_CHANNEL_PARAM;
            pParam->paramLength = sizeof(TI_UINT32);

            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam);
            if(res == NO_SITE_SELECTED_YET)
                res = TI_OK;

            CHECK_PENDING_RESULT(res,pParam)

            if (res == TI_OK)
            {
                wrqu->freq.m = pParam->content.siteMgrCurrentChannel;
                wrqu->freq.e = 3;
                wrqu->freq.i = 0;
            }
            break;
        }

        /* Set Mode (Adhoc / infrastructure) */
    case SIOCSIWMODE:
        {
            pParam->paramType = SME_DESIRED_BSS_TYPE_PARAM;
            pParam->paramLength = sizeof(ScanBssType_e);

            switch (wrqu->mode)
            {
            case IW_MODE_AUTO:
                pParam->content.smeDesiredBSSType = BSS_ANY;
                break;
            case IW_MODE_ADHOC:
                pParam->content.smeDesiredBSSType = BSS_INDEPENDENT;
                break;
            case IW_MODE_INFRA:
                pParam->content.smeDesiredBSSType = BSS_INFRASTRUCTURE;
                break;
            default:
                res = -EOPNOTSUPP;
                goto cmd_end;
            }

            res = cmdDispatch_SetParam(pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

            /* also set the site mgr desired mode */
            pParam->paramType = SITE_MGR_DESIRED_BSS_TYPE_PARAM;
            res = cmdDispatch_SetParam(pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

            break;
        }

        /* Get Mode (Adhoc / infrastructure) */
    case SIOCGIWMODE:
        {
            pParam->paramType = SME_DESIRED_BSS_TYPE_PARAM;
            pParam->paramLength = sizeof(ScanBssType_e);
            res = cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

            switch (pParam->content.smeDesiredBSSType)
            {
            case BSS_ANY:
                wrqu->mode = IW_MODE_AUTO;
                break;
            case BSS_INDEPENDENT:
                wrqu->mode = IW_MODE_ADHOC;
                break;
            case BSS_INFRASTRUCTURE:
                wrqu->mode = IW_MODE_INFRA;
                break;
            default:
                break;
            }

            break;
        }

        /* Set sensitivity (Rssi roaming threshold)*/
    case SIOCSIWSENS:
        {
            /* First get the current roaming configuration as a whole */
            pParam->paramType = ROAMING_MNGR_APPLICATION_CONFIGURATION;
            pParam->paramLength = sizeof (roamingMngrConfigParams_t);
            res = cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

            CHECK_PENDING_RESULT(res,pParam)

            /* Now change the low rssi threshold supplied by the user */
            pParam->content.roamingConfigBuffer.roamingMngrThresholdsConfig.lowRssiThreshold = wrqu->param.value;

            /* And set the parameters back to the roaming module */
            res = cmdDispatch_SetParam(pCmdInterpret->hCmdDispatch, pParam);

            CHECK_PENDING_RESULT(res,pParam)

            break;
        }

        /* Get sensitivity (Rssi threshold OR CCA?)*/
    case SIOCGIWSENS:
        {
            pParam->paramType = ROAMING_MNGR_APPLICATION_CONFIGURATION;
            pParam->paramLength = sizeof (roamingMngrConfigParams_t);
            res = cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

            CHECK_PENDING_RESULT(res,pParam)

            if (res == TI_OK)
            {
                wrqu->param.value = pParam->content.roamingConfigBuffer.roamingMngrThresholdsConfig.lowRssiThreshold;
                wrqu->param.disabled = (wrqu->param.value == 0);
                wrqu->param.fixed = 1;
            }

            break;
        }

        /* Get a range of parameters regarding the device capabilities */
    case SIOCGIWRANGE:
        {
            struct iw_point *data = (struct iw_point *) cmdObj->buffer1;
            struct iw_range *range = (struct iw_range *) cmdObj->buffer2;
            int i;
            ScanBssType_e smeDesiredBssType = BSS_ANY;
            paramInfo_t *pParam2;

            /* Reset structure */
            data->length = sizeof(struct iw_range);
            os_memorySet(pCmdInterpret->hOs, range, 0, sizeof(struct iw_range));

            /* Wireless Extension version info */
            range->we_version_compiled = WIRELESS_EXT;   /* Must be WIRELESS_EXT */
            range->we_version_source = 19;               /* Last update of source */

            /* estimated maximum TCP throughput values (bps) */
            range->throughput = MAX_THROUGHPUT;

            /* NWID (or domain id) */
            range->min_nwid = 0; /* Minimal NWID we are able to set */
            range->max_nwid = 0; /* Maximal NWID we are able to set */

            /* Old Frequency - no need to support this*/
            range->old_num_channels = 0;
            range->old_num_frequency = 0;

            /* Wireless event capability bitmasks */
            IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);
            IW_EVENT_CAPA_SET(range->event_capa, IWEVREGISTERED);
            IW_EVENT_CAPA_SET(range->event_capa, IWEVEXPIRED);

            /* signal level threshold range */
            range->sensitivity = 0;

            /* Rates */
            pParam->paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam );

            CHECK_PENDING_RESULT(res,pParam)
            pParam2 = (paramInfo_t *)os_memoryAlloc(pCmdInterpret->hOs, sizeof(paramInfo_t));
            if (pParam2)
            {
                pParam2->paramType = SME_DESIRED_BSS_TYPE_PARAM;
                pParam2->paramLength = sizeof(ScanBssType_e);
                res = cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam2);
                CHECK_PENDING_RESULT(res,pParam2)
                smeDesiredBssType = pParam2->content.smeDesiredBSSType;
                os_memoryFree(pCmdInterpret->hOs, pParam2, sizeof(paramInfo_t));
            }
            /* Number of entries in the rates list */
            range->num_bitrates = pParam->content.siteMgrDesiredSupportedRateSet.len;
            for (i=0; i<pParam->content.siteMgrDesiredSupportedRateSet.len; i++)
            {
                switch(pParam->content.siteMgrDesiredSupportedRateSet.ratesString[i] & 0x7F)
                {
                    case NET_RATE_MCS0:
                    case NET_RATE_MCS1:
                    case NET_RATE_MCS2:
                    case NET_RATE_MCS3:
                    case NET_RATE_MCS4:
                    case NET_RATE_MCS5:
                    case NET_RATE_MCS6:
                    case NET_RATE_MCS7:
                         if (BSS_INDEPENDENT == smeDesiredBssType)
                             continue;
                    default:
                         range->bitrate[i] = CALCULATE_RATE_VALUE(pParam->content.siteMgrDesiredSupportedRateSet.ratesString[i])
                         if (WEXT_MAX_RATE_VALUE == range->bitrate[i])
                         {
                             range->bitrate[i] = WEXT_MAX_RATE_REAL_VALUE;   /* convert special code 0x7F to 65Mbps */
                         }
                         break;
                }
            }

            /* RTS threshold */
            range->min_rts = TWD_RTS_THRESHOLD_MIN; /* Minimal RTS threshold */
            range->max_rts = TWD_RTS_THRESHOLD_DEF; /* Maximal RTS threshold */

            /* Frag threshold */
            range->min_frag = TWD_FRAG_THRESHOLD_MIN;    /* Minimal frag threshold */
            range->max_frag = TWD_FRAG_THRESHOLD_DEF;    /* Maximal frag threshold */

            /* Power Management duration & timeout */
            range->min_pmp = 0;  /* Minimal PM period */
            range->max_pmp = 0;  /* Maximal PM period */
            range->min_pmt = 0;  /* Minimal PM timeout */
            range->max_pmt = 0;  /* Maximal PM timeout */
            range->pmp_flags = IW_POWER_ON;  /* How to decode max/min PM period */
            range->pmt_flags = IW_POWER_ON; /* How to decode max/min PM timeout */

            /* What Power Management options are supported */
            range->pm_capa = IW_POWER_UNICAST_R |                /* Receive only unicast messages */
                             IW_POWER_MULTICAST_R |              /* Receive only multicast messages */
                             IW_POWER_ALL_R |                    /* Receive all messages though PM */
                             IW_POWER_FORCE_S |                  /* Force PM procedure for sending unicast */
                             IW_POWER_PERIOD |                   /* Value is a period/duration of */
                             IW_POWER_TIMEOUT;                   /* Value is a timeout (to go asleep) */

            /* Transmit power */
            range->txpower_capa = IW_TXPOW_RELATIVE | IW_TXPOW_RANGE;    /* What options are supported */
            range->num_txpower = 5;  /* Number of entries in the list */
            range->txpower[0] = 1;   /* list of values (maximum is IW_MAX_TXPOWER = 8) */
            range->txpower[1] = 2;   /* list of values (maximum is IW_MAX_TXPOWER = 8) */
            range->txpower[2] = 3;   /* list of values (maximum is IW_MAX_TXPOWER = 8) */
            range->txpower[3] = 4;   /* list of values (maximum is IW_MAX_TXPOWER = 8) */
            range->txpower[4] = 5;   /* list of values (maximum is IW_MAX_TXPOWER = 8) */

            /* Retry limits and lifetime */
            range->retry_capa = 0;   /* What retry options are supported */
            range->retry_flags = 0;  /* How to decode max/min retry limit */
            range->r_time_flags = 0; /* How to decode max/min retry life */
            range->min_retry = 0;    /* Minimal number of retries */
            range->max_retry = 0;    /* Maximal number of retries */
            range->min_r_time = 0;   /* Minimal retry lifetime */
            range->max_r_time = 0;   /* Maximal retry lifetime */

            /* Get Supported channels */
            pParam->paramType = SITE_MGR_RADIO_BAND_PARAM;
            res = cmdDispatch_GetParam( pCmdInterpret->hCmdDispatch, pParam );

            CHECK_PENDING_RESULT(res,pParam)

            /* pParam->content.siteMgrRadioBand contains the current band, now get list of supported channels */
            pParam->paramType = REGULATORY_DOMAIN_ALL_SUPPORTED_CHANNELS;
            res = cmdDispatch_GetParam( pCmdInterpret->hCmdDispatch, pParam );

            CHECK_PENDING_RESULT(res,pParam)

            range->num_channels = pParam->content.supportedChannels.sizeOfList;    /* Number of channels [0; num - 1] */
            range->num_frequency = pParam->content.supportedChannels.sizeOfList;   /* Number of entry in the list */

            for (i=0; i<pParam->content.supportedChannels.sizeOfList; i++)
            {
                range->freq[i].e = 0;
                range->freq[i].m = i;
                range->freq[i].i = pParam->content.supportedChannels.listOfChannels[i]+1;
            }

            /* Encoder (Encryption) capabilities */
            range->num_encoding_sizes = 4;
            /* 64(40) bits WEP */
            range->encoding_size[0] = WEP_KEY_LENGTH_40;
            /* 128(104) bits WEP */
            range->encoding_size[1] = WEP_KEY_LENGTH_104;
            /* 256 bits for WPA-PSK */
            range->encoding_size[2] = TKIP_KEY_LENGTH;
            /* 128 bits for WPA2-PSK */
            range->encoding_size[3] = AES_KEY_LENGTH;
            /* 4 keys are allowed */
            range->max_encoding_tokens = 4;

            range->encoding_login_index = 0; /* token index for login token */

            /* Encryption capabilities */
            range->enc_capa = IW_ENC_CAPA_WPA |
                              IW_ENC_CAPA_WPA2 |
                              IW_ENC_CAPA_CIPHER_TKIP |
                              IW_ENC_CAPA_CIPHER_CCMP; /* IW_ENC_CAPA_* bit field */

        }
        break;

        /* Set desired BSSID */
    case SIOCSIWAP:
        {

            /* If MAC address is zeroes -> connect to "ANY" BSSID */
            if (MAC_NULL (wrqu->ap_addr.sa_data))
            {
                /* Convert to "FF:FF:FF:FF:FF:FF" since this driver requires this value */
                MAC_COPY (pParam->content.siteMgrDesiredBSSID, "\xff\xff\xff\xff\xff\xff");
            }
            else
            {
                MAC_COPY (pParam->content.siteMgrDesiredBSSID, wrqu->ap_addr.sa_data);
            }

            pParam->paramType = SITE_MGR_DESIRED_BSSID_PARAM;
            res = cmdDispatch_SetParam ( pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            /* also set it to the SME */
            pParam->paramType = SME_DESIRED_BSSID_PARAM;
            res = cmdDispatch_SetParam ( pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            break;
        }


        /* Get current BSSID */
    case SIOCGIWAP:
        {
            /* Get current AP BSSID */
            pParam->paramType = SITE_MGR_CURRENT_BSSID_PARAM;
            res = cmdDispatch_GetParam ( pCmdInterpret->hCmdDispatch, pParam );

            CHECK_PENDING_RESULT(res,pParam)

            /* In case we are not associated - copy zeroes into bssid */
            if (res == NO_SITE_SELECTED_YET)
            {
                MAC_COPY (wrqu->ap_addr.sa_data, "\x00\x00\x00\x00\x00\x00");
                cmdObj->return_code = WEXT_OK;
            }
            else if (res == TI_OK)
            {
                MAC_COPY (wrqu->ap_addr.sa_data, pParam->content.siteMgrDesiredBSSID);
            }

            break;
        }


        /* request MLME operation (Deauthenticate / Disassociate) */
    case SIOCSIWMLME:
        {
            struct iw_mlme *mlme = (struct iw_mlme *)cmdObj->param3;

            pParam->paramType = SITE_MGR_DESIRED_SSID_PARAM;

            /* In either case - we need to disconnect, so prepare "junk" SSID */
            for (i = 0; i < MAX_SSID_LEN; i++)
                pParam->content.siteMgrDesiredSSID.str[i] = (i+1);
            pParam->content.siteMgrDesiredSSID.len = MAX_SSID_LEN;

            switch (mlme->cmd)
            {
            case IW_MLME_DEAUTH:
            case IW_MLME_DISASSOC:
                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam );
                CHECK_PENDING_RESULT(res,pParam)
                /* now also set it to the SME */
                pParam->paramType = SME_DESIRED_SSID_ACT_PARAM;
                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam );
                CHECK_PENDING_RESULT(res,pParam)
                break;
            default:
                res = -EOPNOTSUPP;
                goto cmd_end;
            }
            break;
        }

        /* trigger scanning (list cells) */
    case SIOCSIWSCAN:
        {
            struct iw_scan_req pScanReq;
            TScanParams scanParams;

            pParam->content.pScanParams = &scanParams;

            /* Init the parameters in case the Supplicant doesn't support them*/
            pParam->content.pScanParams->desiredSsid.len = 0;
            pScanReq.scan_type = SCAN_TYPE_TRIGGERED_ACTIVE;

            if (wrqu->data.pointer)
            {
                if ( copy_from_user( &pScanReq, wrqu->data.pointer, sizeof(pScanReq)) )
                {
                    printk("CRITICAL: Could not copy data from user space!!!");
                    res = -EFAULT;
                    goto cmd_end;
                }
                if (wrqu->data.flags & IW_SCAN_THIS_ESSID)
                {
                    pParam->content.pScanParams->desiredSsid.len = pScanReq.essid_len;
                    os_memoryCopy(pCmdInterpret->hOs,pParam->content.pScanParams->desiredSsid.str, pScanReq.essid, pScanReq.essid_len);
                }
                else
                {
                    pParam->content.pScanParams->desiredSsid.len = 0; /* scan all*/
                }
            }

            /* set the scan type according to driver trigger scan */
            if (IW_SCAN_TYPE_PASSIVE == pScanReq.scan_type)
            {
                pParam->content.pScanParams->scanType = SCAN_TYPE_TRIGGERED_PASSIVE;
            }
            else
            {
                pParam->content.pScanParams->scanType = SCAN_TYPE_TRIGGERED_ACTIVE;
            }

            pParam->paramType = SCAN_CNCN_BSSID_LIST_SCAN_PARAM;
            pParam->paramLength = sizeof(TScanParams);
            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)
        }
        break;

        /* get scanning results */
    case SIOCGIWSCAN:
        {
            unsigned char buf[30];
            char *event = (char *)cmdObj->buffer2;
            struct iw_event iwe;
            char *end_buf, *current_val;
            int allocated_size, rates_allocated_size;
            OS_802_11_BSSID_LIST_EX *my_list;
            OS_802_11_BSSID_EX *my_current;
			OS_802_11_N_RATES *rate_list;
			TI_UINT8 *current_rates;
            int offset;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
            struct iw_request_info info;
            info.cmd = SIOCGIWSCAN;
            info.flags = 0;
#endif
            end_buf = (char *)(cmdObj->buffer2 + wrqu->data.length);

            /* First get the amount of memory required to hold the entire BSSID list by setting the length to 0 */
            pParam->paramType = SCAN_CNCN_BSSID_LIST_SIZE_PARAM;
            pParam->paramLength = 0;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            allocated_size = pParam->content.uBssidListSize;

            /* Allocate required memory */
            my_list = os_memoryAlloc (pCmdInterpret->hOs, allocated_size);
            if (!my_list) {
                res = -ENOMEM;
                goto cmd_end;
            }

            /* And retrieve the list */
            pParam->paramType = SCAN_CNCN_BSSID_LIST_PARAM;
            pParam->content.pBssidList = my_list;
            pParam->paramLength = allocated_size;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            /* Get the number of entries in the scan result list and allocate enough memory to hold rate list
               for every entry. This rate list is extended to include 11n rates */
            pParam->paramType = SCAN_CNCN_NUM_BSSID_IN_LIST_PARAM;
            pParam->paramLength = 0;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            rates_allocated_size = pParam->content.uNumBssidInList * sizeof(OS_802_11_N_RATES);

            /* Allocate required memory */
            rate_list = os_memoryAlloc (pCmdInterpret->hOs, rates_allocated_size);
            if (!rate_list) {
                os_memoryFree (pCmdInterpret->hOs, my_list, allocated_size);
                res = -ENOMEM;
                goto cmd_end;
            }

            /* And retrieve the list */
            pParam->paramType = SCAN_CNCN_BSSID_RATE_LIST_PARAM;
            pParam->content.pRateList = rate_list;
            pParam->paramLength = rates_allocated_size;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)

            my_current = &my_list->Bssid[0];
            i = 0;
            if(wrqu->data.flags)
            {
                for (i=0; i<wrqu->data.flags; i++)
                    my_current = (OS_802_11_BSSID_EX *) (((char *) my_current) + my_current->Length);
            }
            /* Now send a wireless event per BSSID with "tokens" describing it */

            for (; i<my_list->NumberOfItems; i++)
            {
                if (event + my_current->Length > end_buf)
                {
                    break;
                }

                /* The first entry MUST be the AP BSSID */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = SIOCGIWAP;
                iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
                iwe.len = IW_EV_ADDR_LEN;
                os_memoryCopy(pCmdInterpret->hOs, iwe.u.ap_addr.sa_data, &my_current->MacAddress, ETH_ALEN);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_event(event, end_buf, &iwe, IW_EV_ADDR_LEN);
#else
                event = iwe_stream_add_event(&info,event, end_buf, &iwe, IW_EV_ADDR_LEN);
#endif

                /* Add SSID */
                iwe.cmd = SIOCGIWESSID;
                iwe.u.data.flags = 1;
                iwe.u.data.length = min((TI_UINT8)my_current->Ssid.SsidLength, (TI_UINT8)32);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_point(event, end_buf, &iwe, my_current->Ssid.Ssid);
#else
                event = iwe_stream_add_point(&info,event, end_buf, &iwe, my_current->Ssid.Ssid);
#endif

                /* Add the protocol name (BSS support for A/B/G) */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = SIOCGIWNAME;
                os_memoryCopy(pCmdInterpret->hOs, (void*)iwe.u.name, (void*)ieee80211_modes[my_current->NetworkTypeInUse], IFNAMSIZ);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_event(event, end_buf, &iwe, IW_EV_CHAR_LEN);
#else
                event = iwe_stream_add_event(&info,event, end_buf, &iwe, IW_EV_CHAR_LEN);
#endif

                /* add mode (infrastructure or Adhoc) */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = SIOCGIWMODE;
                if (my_current->InfrastructureMode == os802_11IBSS)
                    iwe.u.mode = IW_MODE_ADHOC;
                else if (my_current->InfrastructureMode == os802_11Infrastructure)
                    iwe.u.mode = IW_MODE_INFRA;
                else
                    iwe.u.mode = IW_MODE_AUTO;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_event(event, end_buf, &iwe, IW_EV_UINT_LEN);
#else
                event = iwe_stream_add_event(&info,event, end_buf, &iwe, IW_EV_UINT_LEN);
#endif

                /* add freq */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = SIOCGIWFREQ;
                iwe.u.freq.m = my_current->Configuration.Union.channel;
                iwe.u.freq.e = 3; /* Frequency divider */
                iwe.u.freq.i = 0;
                iwe.len = IW_EV_FREQ_LEN;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_event(event, end_buf, &iwe, IW_EV_FREQ_LEN);
#else
                event = iwe_stream_add_event(&info,event, end_buf, &iwe, IW_EV_FREQ_LEN);
#endif

                /* Add quality statistics */
                iwe.cmd = IWEVQUAL;
                iwe.u.qual.updated = IW_QUAL_LEVEL_UPDATED | IW_QUAL_QUAL_INVALID | IW_QUAL_NOISE_INVALID | IW_QUAL_DBM;
                iwe.u.qual.qual = 0;
                iwe.u.qual.level = my_current->Rssi;
                iwe.u.qual.noise = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_event(event, end_buf, &iwe, IW_EV_QUAL_LEN);
#else
                event = iwe_stream_add_event(&info,event, end_buf, &iwe, IW_EV_QUAL_LEN);
#endif

                /* Add encryption capability */
                iwe.cmd = SIOCGIWENCODE;
                if ((my_current->Capabilities >> CAP_PRIVACY_SHIFT) & CAP_PRIVACY_MASK)
                    iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
                else
                    iwe.u.data.flags = IW_ENCODE_DISABLED;
                iwe.u.data.length = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_point(event, end_buf, &iwe, NULL);
#else
                event = iwe_stream_add_point(&info,event, end_buf, &iwe, NULL);
#endif

                /* add rate */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = SIOCGIWRATE;
                current_val = event + IW_EV_LCP_LEN;

                current_rates = (TI_UINT8 *)(rate_list[i]);

                for (j=0; j<32; j++)
                {
                    if (current_rates[j])
                    {
                        if ((current_rates[j] & 0x7f) == NET_RATE_MCS7)
                        {
                            iwe.u.bitrate.value = WEXT_MAX_RATE_REAL_VALUE;  /* convert the special code 0x7f to 65Mbps */
                        }
                        else
                        {
                            iwe.u.bitrate.value = CALCULATE_RATE_VALUE(current_rates[j])
                        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                        current_val = iwe_stream_add_value(event, current_val, end_buf, &iwe,IW_EV_PARAM_LEN);
#else
                        current_val = iwe_stream_add_value(&info,event, current_val,end_buf, &iwe,IW_EV_PARAM_LEN);
#endif
                    }
                }

                event = current_val;

                /* CUSTOM - Add beacon interval */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = IWEVCUSTOM;
                sprintf(buf, "Bcn int = %d ms ", my_current->Configuration.BeaconPeriod);
                iwe.u.data.length = strlen(buf);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                event = iwe_stream_add_point(event, end_buf, &iwe, buf);
#else
                event = iwe_stream_add_point(&info,event, end_buf, &iwe, buf);
#endif
                /* add ALL variable IEs */
                os_memorySet (pCmdInterpret->hOs, &iwe, 0, sizeof(iwe));
                iwe.cmd = IWEVGENIE;
                offset = sizeof(OS_802_11_FIXED_IEs);
                while(offset < my_current->IELength)
                {
                        OS_802_11_VARIABLE_IEs *pIE;
                        pIE = (OS_802_11_VARIABLE_IEs*)&(my_current->IEs[offset]);
                        iwe.u.data.flags = 1;
                        iwe.u.data.length = pIE->Length + 2;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
                        event = iwe_stream_add_point(event, end_buf, &iwe, (char *)&(my_current->IEs[offset]));
#else
                        event = iwe_stream_add_point(&info, event, end_buf, &iwe, (char *)&(my_current->IEs[offset]));
#endif
                        offset += pIE->Length + 2;
                }

                my_current = (OS_802_11_BSSID_EX *) (((char *) my_current) + my_current->Length);
            }

            wrqu->data.length = event - ((char *)cmdObj->buffer2);
            if(i == my_list->NumberOfItems)
            {
                wrqu->data.flags = 0;
            }
            else
            {
                wrqu->data.flags = i;
            }

            os_memoryFree (pCmdInterpret->hOs, my_list, allocated_size);
            os_memoryFree (pCmdInterpret->hOs, rate_list, rates_allocated_size);
            cmdObj->return_code = WEXT_OK;
        }

        break;

        /* Set ESSID */
    case SIOCSIWESSID:
        {
            char *extra = cmdObj->buffer2;
            int length;

            if (wrqu->essid.flags & SET_SSID_WITHOUT_SUPPL)
                wrqu->essid.flags &= ~SET_SSID_WITHOUT_SUPPL;
            else
                cmdInterpret_setSecurityParams (hCmdInterpret);

            os_memoryZero (pCmdInterpret->hOs, &pParam->content.siteMgrDesiredSSID.str, MAX_SSID_LEN);

            pParam->content.siteMgrCurrentSSID.len = 0;

            if (wrqu->essid.flags == 0)
            {
                /* Connect to ANY ESSID - use empty */
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.siteMgrCurrentSSID.str, "\00", 1);
                pParam->content.siteMgrCurrentSSID.len = 0;;
            } else
            {
                /* Handle ESSID length issue in WEXT (backward compatibility with old/new versions) */
                length = wrqu->essid.length - 1;
                if (length > 0)
                    length--;
                while (length < wrqu->essid.length && extra[length])
                    length++;

                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.siteMgrCurrentSSID.str, cmdObj->buffer2, length);
                pParam->content.siteMgrCurrentSSID.len = length;
            }

            pParam->paramType = SITE_MGR_DESIRED_SSID_PARAM;
            pParam->paramLength = sizeof (TSsid);
            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)
            /* also set it to the SME */
            pParam->paramType = SME_DESIRED_SSID_ACT_PARAM;
            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam );
            CHECK_PENDING_RESULT(res,pParam)
        }
        break;

        /* get ESSID */
    case SIOCGIWESSID:
        {
            char *extra = (char *)cmdObj->buffer2;

            pParam->paramType = SITE_MGR_CURRENT_SSID_PARAM;
            res = cmdDispatch_GetParam ( pCmdInterpret->hCmdDispatch, pParam );
            if(res == NO_SITE_SELECTED_YET)
                res = WEXT_OK;

            CHECK_PENDING_RESULT(res,pParam)

            wrqu->essid.flags  = 1;

            os_memoryCopy(pCmdInterpret->hOs, cmdObj->buffer2, &pParam->content.siteMgrCurrentSSID.str, pParam->content.siteMgrCurrentSSID.len );

            if (pParam->content.siteMgrCurrentSSID.len < MAX_SSID_LEN)
            {
                extra[pParam->content.siteMgrCurrentSSID.len] = 0;
            }
            wrqu->essid.length = pParam->content.siteMgrCurrentSSID.len;
        }

        break;

        /* set node name/nickname */
    case SIOCSIWNICKN:
        {
            if (wrqu->data.length > IW_ESSID_MAX_SIZE) {
                res = -EINVAL;
                goto cmd_end;
            }
            os_memoryCopy(pCmdInterpret->hOs, pCmdInterpret->nickName, cmdObj->buffer2, wrqu->data.length);
            pCmdInterpret->nickName[IW_ESSID_MAX_SIZE] = 0;
            res = TI_OK;
        }

        break;

        /* get node name/nickname */
    case SIOCGIWNICKN:
        {
            struct iw_point *data = (struct iw_point *) cmdObj->buffer1;

            data->length = strlen(pCmdInterpret->nickName);
            os_memoryCopy(pCmdInterpret->hOs, cmdObj->buffer2, &pCmdInterpret->nickName, data->length);

            res = TI_OK;
        }
        break;

        /* Set RTS Threshold */
    case SIOCSIWRTS:
        {
            pParam->paramType = TWD_RTS_THRESHOLD_PARAM;

            if (wrqu->rts.disabled)
                pParam->content.halCtrlRtsThreshold = TWD_RTS_THRESHOLD_DEF;
            else
                pParam->content.halCtrlRtsThreshold = wrqu->rts.value;

            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch,pParam);
            CHECK_PENDING_RESULT(res,pParam)
            break;
        }

        /* Get RTS Threshold */
    case SIOCGIWRTS:
        {
            pParam->paramType = TWD_RTS_THRESHOLD_PARAM;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);

            CHECK_PENDING_RESULT(res,pParam)

            wrqu->rts.value = pParam->content.halCtrlRtsThreshold;
            wrqu->rts.fixed = 1;
            cmdObj->return_code = WEXT_OK;
            break;
        }

        /* Set Fragmentation threshold */
    case SIOCSIWFRAG:
        {
            pParam->paramType = TWD_FRAG_THRESHOLD_PARAM;
            pParam->content.halCtrlFragThreshold = ((wrqu->frag.value+1)>>1) << 1; /* make it always even */

            res = cmdDispatch_SetParam(pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

            break;
        }

        /* Get Fragmentation threshold */
    case SIOCGIWFRAG:
        {
            pParam->paramType = TWD_FRAG_THRESHOLD_PARAM;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);

            CHECK_PENDING_RESULT(res,pParam)

            wrqu->rts.value = pParam->content.halCtrlFragThreshold;
            wrqu->rts.fixed = 1;
            cmdObj->return_code = WEXT_OK;
            break;
        }

        /* Set TX power level */
    case SIOCSIWTXPOW:
        if (wrqu->txpower.disabled == 1)
        {
            cmdObj->return_code = WEXT_INVALID_PARAMETER;
        }
        else
        {
            pParam->paramType = REGULATORY_DOMAIN_CURRENT_TX_POWER_LEVEL_PARAM;
            pParam->content.desiredTxPower = wrqu->txpower.value;
            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch,pParam);
            CHECK_PENDING_RESULT(res,pParam)
        }
        break;

        /* Get TX power level */
    case SIOCGIWTXPOW:
        {
            pParam->paramType = REGULATORY_DOMAIN_CURRENT_TX_POWER_IN_DBM_PARAM;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);

            CHECK_PENDING_RESULT(res,pParam)

            wrqu->txpower.flags = IW_TXPOW_RELATIVE | IW_TXPOW_RANGE;
            wrqu->txpower.disabled = 0;
            wrqu->txpower.fixed = 0;
            wrqu->txpower.value = pParam->content.desiredTxPower;

            break;
        }

        /* set encoding token & mode - WEP only */
    case SIOCSIWENCODE:
        {
            int index;

            index = (wrqu->encoding.flags & IW_ENCODE_INDEX);

            /* iwconfig gives index as 1 - N */
            if (index > 0)
                index--;
            else
            {
                pParam->paramType = RSN_DEFAULT_KEY_ID;
                res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);
                CHECK_PENDING_RESULT(res,pParam)
                index = pParam->content.rsnDefaultKeyID;
            }

            pParam->paramType = RSN_ADD_KEY_PARAM;
            /* remove key if disabled */
            if (wrqu->data.flags & IW_ENCODE_DISABLED)
            {
                pParam->paramType = RSN_REMOVE_KEY_PARAM;
            }

            pParam->content.rsnOsKey.KeyIndex = index;

            if (wrqu->data.length)
            {
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.rsnOsKey.KeyMaterial, cmdObj->buffer2, wrqu->data.length);
                pParam->content.rsnOsKey.KeyLength = wrqu->data.length;
            } else
            {
                /* No key material is provided, just set given index as default TX key */
                pParam->paramType = RSN_DEFAULT_KEY_ID;
                pParam->content.rsnDefaultKeyID = index;
            }

            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

            break;
        }


        /* get encoding token & mode */
    case SIOCGIWENCODE:
        {
            int index, encr_mode;
            char *extra = (char *)cmdObj->buffer2;
            TSecurityKeys myKeyInfo;

            wrqu->data.length = 0;
            extra[0] = 0;

            /* Get Index from user request */
            index = (wrqu->encoding.flags & IW_ENCODE_INDEX);
            if (index > 0)
                index--;
            else
            {
                pParam->paramType = RSN_DEFAULT_KEY_ID;
                res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);
                CHECK_PENDING_RESULT(res,pParam)
                index = pParam->content.rsnDefaultKeyID;
                wrqu->data.flags = (index+1);
            }

            pParam->content.pRsnKey = &myKeyInfo;

            pParam->paramType = RSN_KEY_PARAM;
            pParam->content.pRsnKey->keyIndex = index;
            res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);
            CHECK_PENDING_RESULT(res,pParam)

            if ((pParam->content.pRsnKey) && (pParam->content.pRsnKey->encLen))
            {
                wrqu->data.flags |= IW_ENCODE_ENABLED;
                wrqu->data.length = pParam->content.pRsnKey->encLen;
                os_memoryCopy(pCmdInterpret->hOs,extra, &pParam->content.pRsnKey->encKey,wrqu->data.length);
            }

            /* Convert from driver (OID-like) authentication parameters to WEXT */
            if (pCmdInterpret->wai.iw_auth_cipher_pairwise & IW_AUTH_CIPHER_CCMP)
                encr_mode = os802_11Encryption3Enabled;
            else if (pCmdInterpret->wai.iw_auth_cipher_pairwise & IW_AUTH_CIPHER_TKIP)
                encr_mode = os802_11Encryption2Enabled;
            else if (pCmdInterpret->wai.iw_auth_cipher_pairwise & (IW_AUTH_CIPHER_WEP40 | IW_AUTH_CIPHER_WEP104))
                encr_mode = os802_11Encryption1Enabled;
            else if (pCmdInterpret->wai.iw_auth_cipher_group & IW_AUTH_CIPHER_CCMP)
                encr_mode = os802_11Encryption3Enabled;
            else if (pCmdInterpret->wai.iw_auth_cipher_group & IW_AUTH_CIPHER_TKIP)
                encr_mode = os802_11Encryption2Enabled;
            else
                encr_mode = os802_11EncryptionDisabled;

            if (encr_mode == os802_11EncryptionDisabled)
                wrqu->data.flags |= IW_ENCODE_OPEN;
            else
                wrqu->data.flags |= IW_ENCODE_RESTRICTED;

            cmdObj->return_code = WEXT_OK;

        }
        break;

    case SIOCSIWGENIE:
        {
            pParam->paramType = RSN_GENERIC_IE_PARAM;
            pParam->content.rsnGenericIE.length = wrqu->data.length;
            if (wrqu->data.length) {
                os_memoryCopy(pCmdInterpret->hOs, pParam->content.rsnGenericIE.data, cmdObj->param3, wrqu->data.length);
            }
            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam);
        }
        break;

        /* Set Authentication */
    case SIOCSIWAUTH:
        res = TI_OK;
        switch (wrqu->param.flags & IW_AUTH_INDEX)
        {
        case IW_AUTH_WPA_VERSION:
            pCmdInterpret->wai.iw_auth_wpa_version = wrqu->param.value;
            break;
        case IW_AUTH_CIPHER_PAIRWISE:
            pCmdInterpret->wai.iw_auth_cipher_pairwise = wrqu->param.value;
            break;
        case IW_AUTH_CIPHER_GROUP:
            pCmdInterpret->wai.iw_auth_cipher_group = wrqu->param.value;
            break;
        case IW_AUTH_KEY_MGMT:
            pCmdInterpret->wai.iw_auth_key_mgmt = wrqu->param.value;
            break;
        case IW_AUTH_80211_AUTH_ALG:
            pCmdInterpret->wai.iw_auth_80211_auth_alg = wrqu->param.value;
            break;
        case IW_AUTH_WPA_ENABLED:
            break;
        case IW_AUTH_TKIP_COUNTERMEASURES:
            break;
        case IW_AUTH_DROP_UNENCRYPTED:
            break;
        case IW_AUTH_RX_UNENCRYPTED_EAPOL:
            break;
        case IW_AUTH_PRIVACY_INVOKED:
            break;
        default:
            res = -EOPNOTSUPP;
        }
        break;

        /* Get Authentication */
    case SIOCGIWAUTH:
        res = TI_OK;
        {
            switch (wrqu->param.flags & IW_AUTH_INDEX)
            {
            case IW_AUTH_WPA_VERSION:
                wrqu->param.value = pCmdInterpret->wai.iw_auth_wpa_version;
                break;
            case IW_AUTH_CIPHER_PAIRWISE:
                wrqu->param.value = pCmdInterpret->wai.iw_auth_cipher_pairwise;
                break;
            case IW_AUTH_CIPHER_GROUP:
                wrqu->param.value = pCmdInterpret->wai.iw_auth_cipher_group;
                break;
            case IW_AUTH_KEY_MGMT:
                wrqu->param.value = pCmdInterpret->wai.iw_auth_key_mgmt;
                break;
            case IW_AUTH_80211_AUTH_ALG:
                wrqu->param.value = pCmdInterpret->wai.iw_auth_80211_auth_alg;
                break;
            default:
                res = -EOPNOTSUPP;
            }
        }
        break;

        /* set encoding token & mode */
    case SIOCSIWENCODEEXT:
        {
            struct iw_encode_ext *ext = (struct iw_encode_ext *)cmdObj->buffer2;
            TI_UINT8 *addr;
            TI_UINT8 temp[32];

#ifdef GEM_SUPPORTED
            if ( ext->alg == KEY_GEM ) {
                TSecurityKeys key;

                os_memoryZero(pCmdInterpret->hOs, &key, sizeof(key));
                key.keyType = ext->alg;
                if (ext->key_len > MAX_KEY_LEN) {
                    res = -EINVAL;
                    break;
                }
                key.encLen = ext->key_len;
                os_memoryCopy(pCmdInterpret->hOs, key.encKey, ext->key, ext->key_len);
                key.keyIndex = (wrqu->encoding.flags & IW_ENCODE_INDEX) - 1;
                os_memoryCopy(pCmdInterpret->hOs, &key.macAddress, ext->addr.sa_data, sizeof(key.macAddress));

                pParam->paramType = RSN_SET_KEY_PARAM;
                pParam->paramLength = sizeof(pParam->content.pRsnKey);
                pParam->content.pRsnKey = &key;

                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
                CHECK_PENDING_RESULT(res,pParam);
                break;
            }
#endif
            addr = ext->addr.sa_data;

            /*
            os_printf ("\next->address = %02x:%02x:%02x:%02x:%02x:%02x \n",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
            os_printf ("ext->alg = 0x%x\n",ext->alg);
            os_printf ("ext->ext_flags = 0x%x\n",ext->ext_flags);
            os_printf ("ext->key_len = 0x%x\n",ext->key_len);
            os_printf ("ext->key_idx = 0x%x\n",(wrqu->encoding.flags & IW_ENCODE_INDEX));

            os_printf ("key = ");
            for (i=0; i<ext->key_len; i++)
            {
                os_printf ("0x%02x:",ext->key[i]);
            }
            os_printf ("\n");
            */

            MAC_COPY (pParam->content.rsnOsKey.BSSID, addr);

            pParam->content.rsnOsKey.KeyLength = ext->key_len;

            pParam->content.rsnOsKey.KeyIndex = wrqu->encoding.flags & IW_ENCODE_INDEX;
            pParam->content.rsnOsKey.KeyIndex -= 1;

            if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
            {
                pParam->content.rsnOsKey.KeyIndex |= TIWLAN_KEY_FLAGS_TRANSMIT;
            }

            if (addr[0]!=0xFF)
            {
                pParam->content.rsnOsKey.KeyIndex |= TIWLAN_KEY_FLAGS_PAIRWISE;
            }

            if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
            {
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.rsnOsKey.KeyRSC, &ext->rx_seq, IW_ENCODE_SEQ_MAX_SIZE);
                pParam->content.rsnOsKey.KeyIndex |= TIWLAN_KEY_FLAGS_SET_KEY_RSC;
            }

            /* If key is TKIP - need to switch RX and TX MIC (to match driver API) */
            if (ext->alg == IW_ENCODE_ALG_TKIP)
            {
                os_memoryCopy(pCmdInterpret->hOs,(TI_UINT8*)(((TI_UINT8*)&temp)+24),(TI_UINT8*)(((TI_UINT8*)&ext->key)+16),8);
                os_memoryCopy(pCmdInterpret->hOs,(TI_UINT8*)(((TI_UINT8*)&temp)+16),(TI_UINT8*)(((TI_UINT8*)&ext->key)+24),8);
                os_memoryCopy(pCmdInterpret->hOs,&temp,&ext->key,16);
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.rsnOsKey.KeyMaterial, &temp, ext->key_len);
            } else
            {
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.rsnOsKey.KeyMaterial, &ext->key, ext->key_len);
            }

            if (ext->key_len == 0)
                pParam->paramType = RSN_REMOVE_KEY_PARAM;
            else
                pParam->paramType = RSN_ADD_KEY_PARAM;

            res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
            CHECK_PENDING_RESULT(res,pParam)

        }
        break;

        /* SIOCSIWPMKSA - PMKSA cache operation */
    case SIOCSIWPMKSA:
        {
            struct iw_pmksa *pmksa = (struct iw_pmksa *) cmdObj->buffer2;

            switch (pmksa->cmd)
            {
            case IW_PMKSA_ADD:
                pParam->paramType = RSN_PMKID_LIST;
                pParam->content.rsnPMKIDList.BSSIDInfoCount = 1;
                pParam->content.rsnPMKIDList.Length = 2*sizeof(TI_UINT32) + MAC_ADDR_LEN + PMKID_VALUE_SIZE;
                MAC_COPY (pParam->content.rsnPMKIDList.osBSSIDInfo[0].BSSID, pmksa->bssid.sa_data);
                os_memoryCopy(pCmdInterpret->hOs, &pParam->content.rsnPMKIDList.osBSSIDInfo[0].PMKID, pmksa->pmkid, IW_PMKID_LEN);

                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
                CHECK_PENDING_RESULT(res,pParam)

                break;
            case IW_PMKSA_REMOVE:
                /* Not supported yet */
                break;
            case IW_PMKSA_FLUSH:
                pParam->paramType = RSN_PMKID_LIST;
                /* By using info count=0, RSN knows to clear its tables */
                /* It's also possible to call rsn_resetPMKIDList directly, but cmdDispatcher should be the interface */
                pParam->content.rsnPMKIDList.BSSIDInfoCount = 0;
                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch, pParam);
                CHECK_PENDING_RESULT(res,pParam)

                break;
            default:
                cmdObj->return_code = WEXT_NOT_SUPPORTED;
                break;
            }
        }

        break;

    case SIOCIWFIRSTPRIV:
        {
            ti_private_cmd_t *my_command = (ti_private_cmd_t *)cmdObj->param3;

            /*
            os_printf ("cmd =  0x%x     flags = 0x%x\n",(unsigned int)my_command->cmd,(unsigned int)my_command->flags);
            os_printf ("in_buffer =  0x%x (len = %d)\n",my_command->in_buffer,(unsigned int)my_command->in_buffer_len);
            os_printf ("out_buffer =  0x%x (len = %d)\n",my_command->out_buffer,(unsigned int)my_command->out_buffer_len);
            */

            pParam->paramType = my_command->cmd;

            if (IS_PARAM_ASYNC(my_command->cmd))
            {
                /* os_printf ("Detected ASYNC command - setting CB \n"); */
                pParam->content.interogateCmdCBParams.hCb  =  (TI_HANDLE)pCmdInterpret;
                pParam->content.interogateCmdCBParams.fCb  =  (void*)cmdInterpret_ServiceCompleteCB;
                pParam->content.interogateCmdCBParams.pCb  =  my_command->out_buffer;
                if (my_command->out_buffer)
                {
                    /* the next copy is need for PLT commands */
                    os_memoryCopy(pCmdInterpret->hOs,  my_command->out_buffer, my_command->in_buffer, min(my_command->in_buffer_len,my_command->out_buffer_len));
                }
            }
            else if ((my_command->in_buffer) && (my_command->in_buffer_len))
            {
                /*
                this cmd doesnt have the structure allocated as part of the paramInfo_t structure.
                as a result we need to allocate the memory internally.
                */
                if(IS_ALLOC_NEEDED_PARAM(my_command->cmd))
                {
                    *(void **)&pParam->content = os_memoryAlloc(pCmdInterpret->hOs, my_command->in_buffer_len);
                    os_memoryCopy(pCmdInterpret->hOs, *(void **)&pParam->content, my_command->in_buffer, my_command->in_buffer_len);
                }
                else
                    os_memoryCopy(pCmdInterpret->hOs,&pParam->content,my_command->in_buffer,my_command->in_buffer_len);
            }

            if (my_command->flags & PRIVATE_CMD_SET_FLAG)
            {
                /* os_printf ("Calling setParam\n"); */
                pParam->paramLength = my_command->in_buffer_len;
                res = cmdDispatch_SetParam (pCmdInterpret->hCmdDispatch,pParam);
            }
            else if (my_command->flags & PRIVATE_CMD_GET_FLAG)
            {
                /* os_printf ("Calling getParam\n"); */
                pParam->paramLength = my_command->out_buffer_len;
                res = cmdDispatch_GetParam (pCmdInterpret->hCmdDispatch,pParam);
                if(res == EXTERNAL_GET_PARAM_DENIED)
                {
                    cmdObj->return_code  = WEXT_INVALID_PARAMETER;
                    goto cmd_end;
                }

                /*
                this is for cmd that want to check the size of memory that they need to
                allocate for the actual data.
                */
                if(pParam->paramLength && (my_command->out_buffer_len == 0))
                {
                    my_command->out_buffer_len = pParam->paramLength;
                }
            }
            else
            {
                res = TI_NOK;
            }

            if (res == TI_OK)
            {
                if(IS_PARAM_ASYNC(my_command->cmd))
                {
                    pCmdInterpret->pAsyncCmd = cmdObj; /* Save command handle for completion CB */
                    res = COMMAND_PENDING;
                }
                else
                {
                    if ((my_command->out_buffer) && (my_command->out_buffer_len))
                    {
                        if(IS_ALLOC_NEEDED_PARAM(my_command->cmd))
                        {
                            os_memoryCopy(pCmdInterpret->hOs,my_command->out_buffer,*(void **)&pParam->content,my_command->out_buffer_len);
                        }
                        else
                        {
                            os_memoryCopy(pCmdInterpret->hOs,my_command->out_buffer,&pParam->content,my_command->out_buffer_len);
                        }
                    }
                }
            }

            /* need to free the allocated memory */
            if(IS_ALLOC_NEEDED_PARAM(my_command->cmd))
            {
                os_memoryFree(pCmdInterpret->hOs, *(void **)&pParam->content, my_command->in_buffer_len);
            }
        }

        break;

    default:
        break;

    }

    if (res == TI_OK)
    {
        cmdObj->return_code = WEXT_OK;
    }
cmd_end:
    os_memoryFree(pCmdInterpret->hOs, pParam, sizeof(paramInfo_t));
    /* Return with return code */
    return res;

}



/* This routine is called by the command mailbox module to signal an ASYNC command has complete */
int cmdInterpret_ServiceCompleteCB (TI_HANDLE hCmdInterpret, int status, void *buffer)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;

    if (pCmdInterpret->pAsyncCmd == NULL)
    {
        os_printf ("cmdInterpret_ServiceCompleteCB: AsyncCmd is NULL!!\n");
        return TI_NOK;
    }

    pCmdInterpret->pAsyncCmd->return_code = status;

    pCmdInterpret->pAsyncCmd = NULL;

    /* Call the Cmd module to complete command processing */
    cmdHndlr_Complete (pCmdInterpret->hCmdHndlr);

    /* Call commands handler to continue handling further commands if queued */
    cmdHndlr_HandleCommands (pCmdInterpret->hCmdHndlr);

    return TI_OK;
}

/* Register to receive events */
static int cmdInterpret_initEvents(TI_HANDLE hCmdInterpret)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)(hCmdInterpret);
    IPC_EVENT_PARAMS evParams;
    int i = 0;

    for (i=0; i<IPC_EVENT_MAX; i++)
    {
        evParams.uDeliveryType    = DELIVERY_PUSH;
        evParams.uProcessID       = 0;
        evParams.uEventID         = 0;
        evParams.hUserParam       = hCmdInterpret;
        evParams.pfEventCallback  = cmdInterpret_Event;
        evParams.uEventType       = i;
        EvHandlerRegisterEvent (pCmdInterpret->hEvHandler, (TI_UINT8*) &evParams, sizeof(IPC_EVENT_PARAMS));
        pCmdInterpret->hEvents[i] = evParams.uEventID;
    }

    return TI_OK;
}


/* Unregister events */
static int cmdInterpret_unregisterEvents(TI_HANDLE hCmdInterpret, TI_HANDLE hEvHandler)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)(hCmdInterpret);
    IPC_EVENT_PARAMS evParams;
    int i = 0;
    os_setDebugOutputToLogger(TI_FALSE);

    for (i=0; i<IPC_EVENT_MAX; i++)
    {
        evParams.uEventType =  i;
        evParams.uEventID = pCmdInterpret->hEvents[i];
        EvHandlerUnRegisterEvent (pCmdInterpret->hEvHandler, &evParams);
    }

    return TI_OK;
}


/* Handle driver events and convert to WEXT format */
static TI_INT32 cmdInterpret_Event(IPC_EV_DATA* pData)
{
    IPC_EVENT_PARAMS * pInParam =  (IPC_EVENT_PARAMS *)pData;
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)(pInParam->hUserParam);
    OS_802_11_ASSOCIATION_INFORMATION  *assocInformation;
    TI_UINT8 *requestIEs;
    TI_UINT8 *responseIEs;
    union iwreq_data wrqu;
    char *memptr;
    int TotalLength, res = TI_OK;
#ifdef XCC_MODULE_INCLUDED
    cckm_assocInformation_t cckm_assoc;
    unsigned char beaconIE[MAX_BEACON_BODY_LENGTH];
    unsigned char Cckmstart[CCKM_START_EVENT_SIZE * 2];
    int i,len,n;
    OS_802_11_BSSID_EX *my_current;
#endif
    /* indicate to the OS */
    os_IndicateEvent (pCmdInterpret->hOs, pData);


    switch (pData->EvParams.uEventType)
    {
    case IPC_EVENT_ASSOCIATED:
        {
            paramInfo_t *pParam;

            pParam = (paramInfo_t *)os_memoryAlloc(pCmdInterpret->hOs, sizeof(paramInfo_t));
            if (!pParam)
                return TI_NOK;

            /* Get Association information */

            /* first check if this is ADHOC or INFRA (to avoid retrieving ASSOC INFO for ADHOC)*/

            pParam->paramType = CTRL_DATA_CURRENT_BSS_TYPE_PARAM;
            cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);
            if (pParam->content.ctrlDataCurrentBssType == BSS_INFRASTRUCTURE)
            {

                /* First get length of data */
                pParam->paramType   = ASSOC_ASSOCIATION_INFORMATION_PARAM;
                pParam->paramLength = 0;
                res = cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

                if (res != TI_NOK)
                {
                    TotalLength = sizeof(OS_802_11_ASSOCIATION_INFORMATION) + pParam->content.assocAssociationInformation.RequestIELength +
                                  pParam->content.assocAssociationInformation.ResponseIELength;

                    memptr = os_memoryAlloc(pCmdInterpret->hOs, TotalLength);

                    if (!memptr) {
                        res = TI_NOK;
                        goto event_end;
                    }

                    /* Get actual data */

                    pParam->paramType   = ASSOC_ASSOCIATION_INFORMATION_PARAM;
                    pParam->paramLength = TotalLength;
                    cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

                    os_memoryCopy(pCmdInterpret->hOs, memptr, &pParam->content, TotalLength);

                    assocInformation = (OS_802_11_ASSOCIATION_INFORMATION*)memptr;
                    requestIEs = (TI_UINT8*)memptr + sizeof(OS_802_11_ASSOCIATION_INFORMATION);

                    if (assocInformation->RequestIELength > 0)
                    {
                        os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
                        wrqu.data.length = assocInformation->RequestIELength;
                        wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVASSOCREQIE, &wrqu, (char *)assocInformation->OffsetRequestIEs);
                    }

                    responseIEs = (char *)assocInformation->OffsetRequestIEs + assocInformation->RequestIELength;

                    if (assocInformation->ResponseIELength > 0)
                    {
                        os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
                        wrqu.data.length = assocInformation->ResponseIELength;
                        wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVASSOCRESPIE, &wrqu, (char *)responseIEs);
                    }

                    os_memoryFree(pCmdInterpret->hOs, memptr, TotalLength);

                }
            }

#ifdef XCC_MODULE_INCLUDED
            /*
               the driver must provide BEACON IE for calculate MIC in case of fast roaming
               the data is an ASCII NUL terminated string
            */


            my_current = os_memoryAlloc (pCmdInterpret->hOs,MAX_BEACON_BODY_LENGTH);
            if (!my_current) {
                res = TI_NOK;
                goto event_end;
            }
            pParam->paramType   = SITE_MGR_GET_SELECTED_BSSID_INFO_EX;
            pParam->content.pSiteMgrSelectedSiteInfo = my_current;
            pParam->paramLength = MAX_BEACON_BODY_LENGTH;
            cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

            len = pParam->content.pSiteMgrSelectedSiteInfo->IELength - BEACON_HEADER_FIX_SIZE;

            n = sprintf(beaconIE, "BEACONIE=");
            for (i = 0; i < len; i++)
            {
              n += sprintf(beaconIE + n, "%02x", pParam->content.pSiteMgrSelectedSiteInfo->IEs[BEACON_HEADER_FIX_SIZE+i] & 0xff);
            }

            os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = n;
            wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVCUSTOM, &wrqu, beaconIE);
            os_memoryFree(pCmdInterpret->hOs,my_current,MAX_BEACON_BODY_LENGTH);


            /*
              The driver should be sending the Association Resp IEs
              This informs the supplicant of the IEs used in the association exchanged which are required to proceed with CCKM.
            */


            pParam->paramType   = ASSOC_ASSOCIATION_RESP_PARAM;
            pParam->paramLength = sizeof(TAssocReqBuffer);
            cmdDispatch_GetParam(pCmdInterpret->hCmdDispatch, pParam);

            cckm_assoc.assocRespLen = Param.content.assocReqBuffer.bufferSize - ASSOC_RESP_FIXED_DATA_LEN ;
            cckm_assoc.assocRespBuffer = os_memoryAlloc (pCmdInterpret->hOs, cckm_assoc.assocRespLen);
            if (!cckm_assoc.assocRespBuffer) {
                res = TI_NOK;
                goto event_end;
            }
            memcpy(cckm_assoc.assocRespBuffer,(pParam->content.assocReqBuffer.buffer)+ASSOC_RESP_FIXED_DATA_LEN,cckm_assoc.assocRespLen);
            wrqu.data.length = cckm_assoc.assocRespLen;
            wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVASSOCRESPIE, &wrqu, (TI_UINT8*)cckm_assoc.assocRespBuffer);
            os_memoryFree(pCmdInterpret->hOs,cckm_assoc.assocRespBuffer,cckm_assoc.assocRespLen);

#endif
           /* Send associated event (containing BSSID of AP) */

            os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
            pParam->paramType = SITE_MGR_CURRENT_BSSID_PARAM;
            cmdDispatch_GetParam ( pCmdInterpret->hCmdDispatch, pParam );
            MAC_COPY (wrqu.ap_addr.sa_data, pParam->content.siteMgrDesiredBSSID);
            wrqu.ap_addr.sa_family = ARPHRD_ETHER;
            wireless_send_event(NETDEV(pCmdInterpret->hOs), SIOCGIWAP, &wrqu, NULL);
event_end:
            os_memoryFree(pCmdInterpret->hOs, pParam, sizeof(paramInfo_t));
        }
        break;
    case IPC_EVENT_DISASSOCIATED:
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
        os_memorySet (pCmdInterpret->hOs,wrqu.ap_addr.sa_data, 0, ETH_ALEN);

        wireless_send_event(NETDEV(pCmdInterpret->hOs), SIOCGIWAP, &wrqu, NULL);

        os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = sizeof(IPC_EV_DATA);
        wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVCUSTOM, &wrqu, (TI_UINT8 *)pData);

        break;

    case IPC_EVENT_SCAN_COMPLETE:
        {
            TI_UINT8 *buf;
            wrqu.data.length = 0;
            wrqu.data.flags = 0;
            buf = pData->uBuffer;

            if (*(TI_UINT32*)buf == SCAN_STATUS_COMPLETE)
                wireless_send_event(NETDEV(pCmdInterpret->hOs), SIOCGIWSCAN, &wrqu, NULL);
            else
            {
                if (*(TI_UINT32*)buf == SCAN_STATUS_STOPPED)          // scan is stopped successfully
                    pData->EvParams.uEventType = IPC_EVENT_SCAN_STOPPED;
                else if (*(TI_UINT32*)buf == SCAN_STATUS_FAILED)          // scan is stopped successfully
                    pData->EvParams.uEventType = IPC_EVENT_SCAN_FAILED;
                else
                    break;

                os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
                wrqu.data.length = sizeof(IPC_EV_DATA);
                wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVCUSTOM, &wrqu, (u8 *)pData);
            }
        }
        break;

    case IPC_EVENT_MEDIA_SPECIFIC:
        {
            TI_UINT8 *buf;
            OS_802_11_AUTHENTICATION_REQUEST *request;
            struct iw_michaelmicfailure ev;
            struct iw_pmkid_cand pcand;

            buf = pData->uBuffer;

            if (*(TI_UINT32*)buf == os802_11StatusType_Authentication)
            {
                request = (OS_802_11_AUTHENTICATION_REQUEST *) (buf + sizeof(TI_UINT32));
                if ( request->Flags == OS_802_11_REQUEST_PAIRWISE_ERROR || request->Flags == OS_802_11_REQUEST_GROUP_ERROR)
                {
                    os_printf ("MIC failure detected\n");

                    os_memorySet (pCmdInterpret->hOs,&ev, 0, sizeof(ev));

                    ev.flags = 0 & IW_MICFAILURE_KEY_ID;

                    if (request->Flags == OS_802_11_REQUEST_GROUP_ERROR)
                        ev.flags |= IW_MICFAILURE_GROUP;
                    else
                        ev.flags |= IW_MICFAILURE_PAIRWISE;

                    ev.src_addr.sa_family = ARPHRD_ETHER;
                    MAC_COPY (ev.src_addr.sa_data, request->BSSID);
                    os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
                    wrqu.data.length = sizeof(ev);
                    wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVMICHAELMICFAILURE, &wrqu, (char *) &ev);
                }

            } else if (*(TI_UINT32*)buf == os802_11StatusType_PMKID_CandidateList)
            {
                OS_802_11_PMKID_CANDIDATELIST  *pCandList = (OS_802_11_PMKID_CANDIDATELIST *) (buf + sizeof(TI_UINT32));
                int i;

                os_printf ("Preauthentication list (%d entries)!\n",pCandList->NumCandidates);

                for (i=0; i<pCandList->NumCandidates; i++)
                {
                    os_memorySet (pCmdInterpret->hOs,&pcand, 0, sizeof(pcand));
                    pcand.flags |= IW_PMKID_CAND_PREAUTH;

                    pcand.index = i;

                    MAC_COPY (pcand.bssid.sa_data, pCandList->CandidateList[i].BSSID);

                    os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));

                    wrqu.data.length = sizeof(pcand);

                    wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVPMKIDCAND,
                                        &wrqu, (TI_UINT8 *)&pcand);
                }

            }

        }

        break;
#ifdef XCC_MODULE_INCLUDED
    case IPC_EVENT_CCKM_START:

        n = sprintf(Cckmstart, "CCKM-Start=");
        for (i = 0; i < 14; i++)
        {
          n += sprintf(Cckmstart + n, "%02x", pData->uBuffer[i] & 0xff);
        }

        os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = n;
        wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVCUSTOM, &wrqu, Cckmstart);

        break;
#endif

    default:
        /* Other event? probably private and does not need interface-specific conversion */
        /* Send as "custom" event */
        {
            os_memorySet (pCmdInterpret->hOs,&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = sizeof(IPC_EV_DATA);
            wireless_send_event(NETDEV(pCmdInterpret->hOs), IWEVCUSTOM, &wrqu, (TI_UINT8 *)pData);
        }

        break;
    }

    return res;
}


/* Configure driver authentication and security by converting from WEXT interface to driver (OID-like) settings */
static int cmdInterpret_setSecurityParams (TI_HANDLE hCmdInterpret)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;
    paramInfo_t *pParam;
    int auth_mode, encr_mode;

    /*
        printk ("wpa_version=0x%x auth_alg=0x%x key_mgmt=0x%x "
           "cipher_pairwise=0x%x cipher_group=0x%x\n",
           pCmdInterpret->wai.iw_auth_wpa_version, pCmdInterpret->wai.iw_auth_80211_auth_alg,
           pCmdInterpret->wai.iw_auth_key_mgmt, pCmdInterpret->wai.iw_auth_cipher_pairwise,
           pCmdInterpret->wai.iw_auth_cipher_group);
    */
    pParam = (paramInfo_t *)os_memoryAlloc(pCmdInterpret->hOs, sizeof(paramInfo_t));
    if (!pParam)
        return TI_NOK;
    if (pCmdInterpret->wai.iw_auth_wpa_version & IW_AUTH_WPA_VERSION_WPA2)
    {
        if (pCmdInterpret->wai.iw_auth_key_mgmt & IW_AUTH_KEY_MGMT_802_1X)
            auth_mode = os802_11AuthModeWPA2;
        else
            auth_mode = os802_11AuthModeWPA2PSK;
    } else if (pCmdInterpret->wai.iw_auth_wpa_version & IW_AUTH_WPA_VERSION_WPA)
    {
        if (pCmdInterpret->wai.iw_auth_key_mgmt & IW_AUTH_KEY_MGMT_802_1X)
            auth_mode = os802_11AuthModeWPA;
        else if (pCmdInterpret->wai.iw_auth_key_mgmt & IW_AUTH_KEY_MGMT_PSK)
            auth_mode = os802_11AuthModeWPAPSK;
        else
            auth_mode = os802_11AuthModeWPANone;
    } else if (pCmdInterpret->wai.iw_auth_80211_auth_alg & IW_AUTH_ALG_SHARED_KEY)
    {
        if (pCmdInterpret->wai.iw_auth_80211_auth_alg & IW_AUTH_ALG_OPEN_SYSTEM)
            auth_mode = os802_11AuthModeAutoSwitch;
        else
            auth_mode = os802_11AuthModeShared;
    } else
        auth_mode = os802_11AuthModeOpen;

    if (pCmdInterpret->wai.iw_auth_cipher_pairwise & IW_AUTH_CIPHER_CCMP)
        encr_mode = os802_11Encryption3Enabled;
    else if (pCmdInterpret->wai.iw_auth_cipher_pairwise & IW_AUTH_CIPHER_TKIP)
        encr_mode = os802_11Encryption2Enabled;
    else if (pCmdInterpret->wai.iw_auth_cipher_pairwise &
             (IW_AUTH_CIPHER_WEP40 | IW_AUTH_CIPHER_WEP104))
        encr_mode = os802_11Encryption1Enabled;
    else if (pCmdInterpret->wai.iw_auth_cipher_group & IW_AUTH_CIPHER_CCMP)
        encr_mode = os802_11Encryption3Enabled;
    else if (pCmdInterpret->wai.iw_auth_cipher_group & IW_AUTH_CIPHER_TKIP)
        encr_mode = os802_11Encryption2Enabled;
    else
        encr_mode = os802_11EncryptionDisabled;

    switch (encr_mode)
    {
    case os802_11WEPDisabled:
        encr_mode = TWD_CIPHER_NONE;
        break;
    case os802_11WEPEnabled:
        encr_mode = TWD_CIPHER_WEP;
        break;
    case os802_11Encryption2Enabled:
        encr_mode = TWD_CIPHER_TKIP;
        break;
    case os802_11Encryption3Enabled:
        encr_mode = TWD_CIPHER_AES_CCMP;
        break;
    default:
        break;
    }

    pParam->paramType = RSN_EXT_AUTHENTICATION_MODE;
    pParam->content.rsnExtAuthneticationMode = auth_mode;
    cmdDispatch_SetParam ( pCmdInterpret->hCmdDispatch, pParam );

    pParam->paramType = RSN_ENCRYPTION_STATUS_PARAM;
    pParam->content.rsnEncryptionStatus = encr_mode;
    cmdDispatch_SetParam ( pCmdInterpret->hCmdDispatch, pParam );
    os_memoryFree(pCmdInterpret->hOs, pParam, sizeof(paramInfo_t));
    return TI_OK;
}


void *cmdInterpret_GetStat (TI_HANDLE hCmdInterpret)
{
    cmdInterpret_t *pCmdInterpret = (cmdInterpret_t *)hCmdInterpret;

    /* Check if driver is initialized - If not - return empty statistics */
    if (hCmdInterpret)
    {
        paramInfo_t *pParam;
        TI_STATUS res;

        pParam = (paramInfo_t *)os_memoryAlloc(pCmdInterpret->hOs, sizeof(paramInfo_t));
        if (!pParam)
            return NULL;

        pParam->paramType = SITE_MGR_GET_STATS;
        res = cmdDispatch_GetParam ( pCmdInterpret->hCmdDispatch, pParam );

        if (res == TI_OK)
        {
            pCmdInterpret->wstats.qual.level = (TI_UINT8)pParam->content.siteMgrCurrentRssi;
            pCmdInterpret->wstats.qual.updated = IW_QUAL_LEVEL_UPDATED | IW_QUAL_QUAL_UPDATED | IW_QUAL_NOISE_INVALID | IW_QUAL_DBM;
        }
        else
        {
            pCmdInterpret->wstats.qual.level = 0;
            pCmdInterpret->wstats.qual.updated = IW_QUAL_ALL_INVALID;
        }

        pCmdInterpret->wstats.qual.noise = 0;
        pCmdInterpret->wstats.qual.qual = 0;
        pCmdInterpret->wstats.status = 0;
        pCmdInterpret->wstats.miss.beacon = 0;
        pCmdInterpret->wstats.discard.retries = 0;      /* Tx : Max MAC retries num reached */
        pCmdInterpret->wstats.discard.nwid = 0;         /* Rx : Wrong nwid/essid */
        pCmdInterpret->wstats.discard.code = 0;         /* Rx : Unable to code/decode (WEP) */
        pCmdInterpret->wstats.discard.fragment = 0;     /* Rx : Can't perform MAC reassembly */
        pCmdInterpret->wstats.discard.misc = 0;     /* Others cases */
        os_memoryFree(pCmdInterpret->hOs, pParam, sizeof(paramInfo_t));
        return &pCmdInterpret->wstats;
    }
    return (void *)NULL;
}
