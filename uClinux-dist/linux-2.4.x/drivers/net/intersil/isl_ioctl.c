/*  $Header$
 *  
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/version.h>
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <linux/kernel.h>
#include <linux/if_arp.h>

#include <asm/uaccess.h>

#include <isl_ioctl.h>
#include <isl_gen.h>
#include <islpci_mgt.h>
#include <bloboid.h>      // additional types and defs for isl38xx fw


#ifdef WIRELESS_IOCTLS

#undef IW_MAX_BITRATES
#define	IW_MAX_BITRATES	16

/******************************************************************************
        Global variable definition section
******************************************************************************/
extern int pc_debug;

int last_key_id = 0;
struct iw_priv_args privtab[] =
{
        { SIOCIWFIRSTPRIV + 0x00, IW_PRIV_TYPE_INT | 1, 0,
                "setauthenten" },
        { SIOCIWFIRSTPRIV + 0x01, 0, IW_PRIV_TYPE_INT | 1,
                "getauthenten" },
        { SIOCIWFIRSTPRIV + 0x02, IW_PRIV_TYPE_INT | 1, 0,
                "setunencrypt" },
        { SIOCIWFIRSTPRIV + 0x03, 0, IW_PRIV_TYPE_INT | 1,
                "getunencrypt" },
        { SIOCIWFIRSTPRIV + 0x04, IW_PRIV_TYPE_INT | 1, 0,
                "setprivinvok" },
        { SIOCIWFIRSTPRIV + 0x05, 0, IW_PRIV_TYPE_INT | 1,
                "getprivinvok" },
        { SIOCIWFIRSTPRIV + 0x06, IW_PRIV_TYPE_INT | 1, 0,
                "setdefkeyid" },
        { SIOCIWFIRSTPRIV + 0x07, 0, IW_PRIV_TYPE_INT | 1,
                "getdefkeyid" },
        { SIOCIWFIRSTPRIV + 0x08, IW_PRIV_TYPE_CHAR | 16, 0,
                "setdefkeyx" },
        { SIOCIWFIRSTPRIV + 0x09, IW_PRIV_TYPE_INT | 1, IW_PRIV_TYPE_CHAR | 16,
                "getdefkeyx" },
        { SIOCIWFIRSTPRIV + 0x0A, 0, IW_PRIV_TYPE_INT | 4,
                "getprivstat" },
        { SIOCIWFIRSTPRIV + 0x0B, 0, IW_PRIV_TYPE_BYTE | IW_MAX_BITRATES,
                "getsuprates" },
        { SIOCIWFIRSTPRIV + 0x0C, IW_PRIV_TYPE_INT | 1, 0,
                "setdtimper" },
        { SIOCIWFIRSTPRIV + 0x0D, 0, IW_PRIV_TYPE_INT | 1,
                "getdtimper" },
        { SIOCIWFIRSTPRIV + 0x0E, IW_PRIV_TYPE_INT | 1, 0,
                "setbeaconper" },
        { SIOCIWFIRSTPRIV + 0x0F, 0, IW_PRIV_TYPE_INT | 1,
                "getbeaconper" },
        { SIOCIWFIRSTPRIV + 0x10, IW_PRIV_TYPE_CHAR | 12, 0,
                "wdslinkadd" },
        { SIOCIWFIRSTPRIV + 0x11, IW_PRIV_TYPE_CHAR | 12, 0,
                "wdslinkdel" },
        { SIOCIWFIRSTPRIV + 0x12, IW_PRIV_TYPE_CHAR | 12, 0,
                "eapauthen" },
        { SIOCIWFIRSTPRIV + 0x13, IW_PRIV_TYPE_CHAR | 12, 0,
                "eapunauth" },
        { SIOCIWFIRSTPRIV + 0x14, IW_PRIV_TYPE_INT | 1, 0,
                "setdot1xen" },
        { SIOCIWFIRSTPRIV + 0x15, 0, IW_PRIV_TYPE_INT | 1,
                "getdot1xen" },
        { SIOCIWFIRSTPRIV + 0x16, IW_PRIV_TYPE_CHAR | 32, 0,
                "setstakeyx" }, // <address>:<key_nr>:<key>
        { SIOCIWFIRSTPRIV + 0x17, IW_PRIV_TYPE_CHAR | 32, IW_PRIV_TYPE_CHAR | 32,
                "getstakeyx" }, // <address>:<key_nr> -> <key>
        { SIOCIWFIRSTPRIV + 0x18, IW_PRIV_TYPE_INT | 1, 0,
                "setfixedrate" },
        { SIOCIWFIRSTPRIV + 0x19, 0, IW_PRIV_TYPE_INT | 1,
                "getfixedrate" },
/*
        { SIOCIWFIRSTPRIV + 0x18, IW_PRIV_TYPE_INT | 1, 0,
                "setfragthresh" },
        { SIOCIWFIRSTPRIV + 0x19, 0, IW_PRIV_TYPE_INT | 1,
                "getfragthresh" },
*/
        { SIOCIWFIRSTPRIV + 0x1A, IW_PRIV_TYPE_INT | 1, 0,
                "setpsm" },
        { SIOCIWFIRSTPRIV + 0x1B, 0, IW_PRIV_TYPE_INT | 1,
                "getpsm" },
        { SIOCIWFIRSTPRIV + 0x1C, IW_PRIV_TYPE_INT | 1, 0,
                "setnonerp" },
        { SIOCIWFIRSTPRIV + 0x1D, 0, IW_PRIV_TYPE_INT | 1,
                "getnonerp" },
        { SIOCIWFIRSTPRIV + 0x1E, 0, IW_PRIV_TYPE_BYTE | 4, 
                "getcountrystr" },
        { SIOCIWFIRSTPRIV + 0x1F, 0, IW_PRIV_TYPE_BYTE | 16,
                "checktraps" }
};

/******************************************************************************
    Driver Management IOCTL functions
******************************************************************************/
int isl_ioctl_getrange(struct net_device *nwdev, struct iw_range *range )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    struct iw_range range_obj;
    char *data;
    unsigned long dlen;
    int rvalue, counter, operation;

    // request the device for the rates
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
       DOT11_OID_RATES, 0, NULL, IW_MAX_BITRATES+1, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_RATES, 
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "getrates: bufaddr %p \n", range );
#endif

    // set the wireless extension version number
    range_obj.we_version_source = SUPPORTED_WIRELESS_EXT;
    range_obj.we_version_compiled = WIRELESS_EXT;

    for( counter=0; counter < IW_MAX_BITRATES; counter++, data++ )
    {
        // check whether the rate value is set
        if( *data )
        {
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG( SHOW_TRACING, "rate: basic %i value %x \n",
                (*data & 0x80) >> 7, *data & 0x7F );
#endif

            // convert the bitrates to Mbps
            range_obj.bitrate[counter] = (double)(*data & 0x7F);
            range_obj.bitrate[counter] *= 1000000;

            // increment the number of rates counter
            range_obj.num_bitrates++;
        }
    }

    // return the data if the destination buffer is defined
    if (copy_to_user( (void *) range, (void *) &range_obj,
        sizeof(struct iw_range)))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }
    
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setrange(struct net_device *nwdev, struct iw_range *range )
{
    return 0;
}

#if WIRELESS_EXT > 11

int isl_ioctl_getstats(struct net_device *nwdev, struct iw_statistics *stats )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    struct iw_statistics stats_obj = { 0 };
    long *data;
    unsigned long dlen;
    int rvalue, operation;

    // request the device for the rates
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
       DOT11_OID_MPDUTXFAILED, 0, NULL, 4, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_MPDUTXFAILED,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "MPDU Tx Failed: %lu \n", *data );
        stats_obj.discard.retries = *data;
#endif

    // return the data if the destination buffer is defined
    if (copy_to_user( (void *) stats, (void *) &stats_obj,
        sizeof(struct iw_statistics)))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }
    
    islpci_mgt_release( private_config, entry );
    return 0;
}


int isl_ioctl_setstats(struct net_device *nwdev, struct iw_statistics *stats )
{
    return 0;
}

#endif

int isl_ioctl_getauthenten(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_AUTHENABLE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_AUTHENABLE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *flag = (long) *data;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Authenticate enable: %i\n", (int) *flag );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setauthenten(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char filter[4] = { 0, 0, 0, 0 };
    int rvalue, operation;
    void *data;
    unsigned long dlen;

    filter[0] = (char) *flag;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Authenticate enable: %i\n", (int) filter[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_AUTHENABLE,
        0, &filter[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_AUTHENABLE,
        &operation, (void **) &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;       
}

int isl_ioctl_getprivfilter(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_EXUNENCRYPTED, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_EXUNENCRYPTED,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *flag = (long) *data;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Privacy filter: %i\n", (int) *flag );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setprivfilter(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char filter[4] = { 0, 0, 0, 0 };
    int rvalue, operation;
    void *data;
    unsigned long dlen;

    filter[0] = (char) *flag;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Privacy filter: %i\n", (int) filter[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_EXUNENCRYPTED,
        0, &filter[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_EXUNENCRYPTED,
        &operation, (void **) &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getprivinvoke(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PRIVACYINVOKED, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVACYINVOKED,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *flag = (long) *data;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Privacy invoked: %i\n", (int) *flag );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setprivinvoke(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char invoke[4] = { 0, 0, 0, 0 };
    int rvalue, operation;
    void *data;
    unsigned long dlen;

    invoke[0] = (char) *flag;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Privacy invoked: %i\n", (int) invoke[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_PRIVACYINVOKED, 0, &invoke[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVACYINVOKED,
        &operation, (void **) &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;    
}

int isl_ioctl_getdefkeyid(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *key = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_DEFKEYID, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_DEFKEYID,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Default Key ID %i\n", (int) *key );
#endif
    *key = (long) *data;

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setdefkeyid(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *key = (long *) wrq->u.data.pointer;
    char keyid[4] = { 0, 0, 0, 0 };
    int rvalue, operation;
    void *data;
    unsigned long dlen;

    keyid[0] = (char) *key;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Default Key ID: %i\n", (int) keyid[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_DEFKEYID, 0, &keyid[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_DEFKEYID,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
		return -EINVAL;
    return rvalue;
}

int isl_ioctl_getdefkeyx(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    int *key = (int *) wrq->u.data.pointer;
    int rvalue, operation;
    struct obj_key *pkey;
    unsigned long oid;
    unsigned long dlen;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Default Key %i\n", (int) *key );
#endif

    switch (*key)
    {
        case 0:
            oid = DOT11_OID_DEFKEY1;
            break;

        case 1:
            oid = DOT11_OID_DEFKEY2;
            break;

        case 2:
            oid = DOT11_OID_DEFKEY3;
            break;

        case 3:
            oid = DOT11_OID_DEFKEY4;
            break;

        default:
            return -EOPNOTSUPP;
    }

    // store the key value for future write operations
    last_key_id = *key;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET, oid, 0, NULL,
        sizeof(struct obj_key), 0);
    if (rvalue = islpci_mgt_response(private_config, oid, &operation, 
    	(void **) &pkey, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    if (copy_to_user( (char *) key, (void *) pkey->key, pkey->length))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }
    
    wrq->u.data.length = sizeof(pkey->key);
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setdefkeyx(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char *key = (char *) wrq->u.data.pointer;
    void *data;
    unsigned long dlen;
    struct obj_key defkey;
    unsigned long oid;
    int rvalue, operation;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Default Key %i \n", last_key_id );
#endif

    switch (last_key_id)
    {
        case 0:
            oid = DOT11_OID_DEFKEY1;
            break;

        case 1:
            oid = DOT11_OID_DEFKEY2;
            break;

        case 2:
            oid = DOT11_OID_DEFKEY3;
            break;

        case 3:
            oid = DOT11_OID_DEFKEY4;
            break;

        default:
            return -EOPNOTSUPP;
    }

    defkey.type = DOT11_PRIV_WEP;
    defkey.length = strlen(key) > sizeof(defkey.key) ? sizeof(defkey.key) :
        strlen(key);
    if (copy_from_user(defkey.key, key, defkey.length))
    {
        return -EFAULT;
    }
    
    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, oid, 0, &defkey,
        sizeof(struct obj_key), 0);

    rvalue = islpci_mgt_response(private_config, oid, &operation, &data, &dlen,
    	&entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;    
}

int isl_ioctl_getprivstat(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    int *stat = (int *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get transmit rejected frame count
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PRIVTXREJECTED, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVTXREJECTED,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "MPDU Tx Rejected %i\n", (int) *data );
#endif
    *stat = (int) *data;
    stat++;
    islpci_mgt_release( private_config, entry );

    // send a get object to get rejected unencrypted frame count
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PRIVRXPLAIN, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVRXPLAIN,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "MPDU Rx Plain %i\n", (int) *data );
#endif
    *stat = (int) *data;
    stat++;
    islpci_mgt_release( private_config, entry );

    // send a get object to get rejected crypted frame count
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PRIVRXFAILED, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVRXFAILED,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "MPDU Rx Failed %i\n", (int) *data );
#endif
    *stat = (int) *data;
    stat++;
    islpci_mgt_release( private_config, entry );

    // send a get object to get received no key frames
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PRIVRXNOKEY, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PRIVRXNOKEY,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "MPDU Rx No Key %i\n", (int) *data );
#endif
    *stat = (int) *data;
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_getsuprates(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char *rates = (char *) wrq->u.data.pointer;
    char *data;
    unsigned long dlen;
    int rvalue, operation;

    // request the device for the rates
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
       DOT11_OID_SUPPORTEDRATES, 0, NULL, IW_MAX_BITRATES+1, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_SUPPORTEDRATES,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    if (copy_to_user( rates, data, IW_MAX_BITRATES+1))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_getfixedrate(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *rate = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_ALOFT_FIXEDRATE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_ALOFT_FIXEDRATE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *rate = (long) *data;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Fixed Rate index: %i\n", (int) *rate );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setfixedrate(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *index = (long *) wrq->u.data.pointer;
    char rate[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    rate[0] = (char) *index;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Fixed Rate index: %i\n", (int) rate[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_ALOFT_FIXEDRATE,
        0, &rate[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_ALOFT_FIXEDRATE,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}


int isl_ioctl_getdtimper(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_DTIMPERIOD, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_DTIMPERIOD,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *flag = (long) ( (*(data+3) & 0xff) <<24 | (*(data+2) & 0xff) <<16 | 
	     (*(data+1) & 0xff) <<8 | (*data & 0xff)); 

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get DTIM Period: %lx\n", *flag );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setdtimper(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char period[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    period[0] = (char) (*flag & 0xff);
    period[1] = (char) ((*flag >> 8) & 0xff);
    period[2] = (char) ((*flag >> 16) & 0xff);
    period[3] = (char) ((*flag >> 24) & 0xff);

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set DTIM Period: %i\n", (int) period[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_DTIMPERIOD,
        0, &period[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_DTIMPERIOD,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}

int isl_ioctl_getbeaconper(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_BEACONPERIOD, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_BEACONPERIOD,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *flag = (long) ( (*(data+3) & 0xff) <<24 | 
		     (*(data+2) & 0xff) <<16 | 
		     (*(data+1) & 0xff) <<8 |
		     (*data & 0xff)); 

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get Beacon Period: %lx\n", *flag );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setbeaconper(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char period[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    period[0] = (char) (*flag & 0xff);
    period[1] = (char) ((*flag >> 8) & 0xff);
    period[2] = (char) ((*flag >> 16) & 0xff);
    period[3] = (char) ((*flag >> 24) & 0xff);

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set Beacon Period: %i\n", (int) period[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_BEACONPERIOD,
        0, &period[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_BEACONPERIOD,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}

int isl_ioctl_getrate(struct net_device *nwdev, struct iw_param *bitrate )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    double rate;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        GEN_OID_LINKSTATE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, GEN_OID_LINKSTATE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Current rate value %i\n", (int) *data );
#endif

    rate = (double) *data * 500000;
    bitrate->value = (int) rate;

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_getrtsthresh(struct net_device *nwdev, struct iw_param *rts )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    int rvalue, operation;
    long *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_RTSTHRESH, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_RTSTHRESH,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    rts->value = *data;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get RTS Thresh 0x%08x (%i)\n", rts->value, 
    	rts->value );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setrtsthresh(struct net_device *nwdev, struct iw_param *rts )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    void *data;
    int rvalue, operation;
    unsigned long dlen;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set RTS Thresh: 0x%08x\n", rts->value );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_RTSTHRESH, 0, &rts->value, 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_RTSTHRESH, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}

int isl_ioctl_getfragthresh(struct net_device *nwdev, struct iw_param *frag )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    int rvalue, operation;
    long *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
    	DOT11_OID_FRAGTHRESH, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_FRAGTHRESH,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    frag->value = *data;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Get Frag Thresh 0x%08x (%i)\n", frag->value, 
    	frag->value );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setfragthresh(struct net_device *nwdev, struct iw_param *frag )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    void *data;
    unsigned long dlen;
    int rvalue, operation;
	
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Set Frag Thresh: 0x%08x\n", frag->value );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_FRAGTHRESH, 0, &frag->value, 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_FRAGTHRESH, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_wdslinkadd(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *address = (unsigned char *) wrq->u.data.pointer;
    unsigned char macaddr[10];
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    // convert the address string to a MAC address
    string_to_macaddress( address, macaddr );

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "WDS Link Add addr %02X:%02X:%02X:%02X:%02X:%02X\n",
        macaddr[0], macaddr[1], macaddr[2],
        macaddr[3], macaddr[4], macaddr[5] );
#endif

    // transmit the object to the card
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_WDSLINKADD, 0, &macaddr[0], 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_WDSLINKADD,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_wdslinkdel(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *address = (unsigned char *) wrq->u.data.pointer;
    unsigned char macaddr[10];
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    // convert the address string to a MAC address
    string_to_macaddress( address, macaddr );

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "WDS Link Remove addr %02X:%02X:%02X:%02X:%02X:%02X\n",
        macaddr[0], macaddr[1], macaddr[2],
        macaddr[3], macaddr[4], macaddr[5] );
#endif

    // transmit the object to the card
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_WDSLINKREMOVE, 0, &macaddr[0], 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_WDSLINKREMOVE,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_eapauthen(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *address = (unsigned char *) wrq->u.data.pointer;
    unsigned char macaddr[10];
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    // convert the address string to a MAC address
    string_to_macaddress( address, macaddr );

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "EAP Auth STA addr %02X:%02X:%02X:%02X:%02X:%02X\n",
        macaddr[0], macaddr[1], macaddr[2],
        macaddr[3], macaddr[4], macaddr[5] );
#endif

    // transmit the object to the card
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_EAPAUTHSTA, 0, &macaddr[0], 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_EAPAUTHSTA, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_eapunauth(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *address = (unsigned char *) wrq->u.data.pointer;
    unsigned char macaddr[10];
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    // convert the address string to a MAC address
    string_to_macaddress( address, macaddr );

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "EAP Unauth STA addr %02X:%02X:%02X:%02X:%02X:%02X\n",
        macaddr[0], macaddr[1], macaddr[2],
        macaddr[3], macaddr[4], macaddr[5] );
#endif

    // transmit the object to the card
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_EAPUNAUTHSTA, 0, &macaddr[0], 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_EAPUNAUTHSTA, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}


int isl_ioctl_getdot1xen(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_DOT1XENABLE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_DOT1XENABLE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Get .1x flag: %i\n", (int) *data );
#endif

    *flag = (long) *data;

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setdot1xen(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *flag = (long *) wrq->u.data.pointer;
    char enabled[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    enabled[0] = (char) *flag;
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Set .1x flag: %i\n", (int) enabled[0] );
#endif

    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_DOT1XENABLE, 0, &enabled[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_DOT1XENABLE,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getstakeyx(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *parameter = (unsigned char *) wrq->u.data.pointer;
    struct obj_stakey stakey, *pstakey;
    int rvalue, operation;
    unsigned long dlen;

    // convert the address string to a MAC address
    string_to_macaddress( parameter, stakey.address );

    // increase the read parameter pointer and read the key number
    parameter += 13;
    stakey.keyid = *parameter - '0';;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "sta addr %02X:%02X:%02X:%02X:%02X:%02X key %i\n",
        (unsigned char) stakey.address[0], (unsigned char) stakey.address[1],
        (unsigned char) stakey.address[2], (unsigned char) stakey.address[3],
        (unsigned char) stakey.address[4], (unsigned char) stakey.address[5],
        (int) stakey.keyid );
#endif

    // send a get object to get the stations entry
    islpci_mgt_queue(private_config, PIMFOR_OP_GET, DOT11_OID_STAKEY, 0,
        &stakey, sizeof(struct obj_stakey), 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_STAKEY,
        &operation, (void **) &pstakey, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "key %s length %i\n", pstakey->key, pstakey->length );
#endif

    parameter = (unsigned char *) wrq->u.data.pointer;
    if (copy_to_user( (char *) parameter, (void *) &pstakey->key,
        sizeof(pstakey->key) ))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }
    
    wrq->u.data.length = sizeof(pstakey->key)+1;
    parameter += sizeof(pstakey->key);
    *parameter = '\0';
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setstakeyx(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    unsigned char *parameter = (unsigned char *) wrq->u.data.pointer;
    struct obj_stakey stakey;
    void *data;
    int rvalue, operation;
    unsigned long dlen;

    // convert the address string to a MAC address
    string_to_macaddress( parameter, stakey.address );

    // increase the read parameter pointer and read the key number
    parameter += 13;
    stakey.keyid = *parameter - '0';

    // increase the read parameter pointer to read the key
    parameter += 2;
    stakey.type = DOT11_PRIV_WEP;
    stakey.length = strlen(parameter) > sizeof(stakey.key) ? sizeof(stakey.key) :
        strlen(parameter);
    if (copy_from_user( stakey.key, parameter, stakey.length))
    {
        return -EFAULT;
    }
    
#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "sta addr %02X:%02X:%02X:%02X:%02X:%02X key %i\n",
        (unsigned char) stakey.address[0], (unsigned char) stakey.address[1],
        (unsigned char) stakey.address[2], (unsigned char) stakey.address[3],
        (unsigned char) stakey.address[4], (unsigned char) stakey.address[5],
        (int) stakey.keyid );
#endif

    // send a get object to get the stations entry
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_STAKEY, 0,
        &stakey, sizeof(struct obj_stakey), 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_STAKEY, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;    
}

int isl_ioctl_getessid(struct net_device *nwdev, struct iw_point *erq)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    struct obj_ssid *essid_obj;
    int rvalue, operation;
    unsigned long dlen;

    // request the device for the essid
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
       DOT11_OID_SSID, 0, NULL, sizeof(struct obj_ssid), 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_SSID,
        &operation, (void **) &essid_obj, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "getessid: parameter at %p\n", (void *) essid_obj);
#endif

    // copy the data to the return structure
    erq->flags = 1;             // set ESSID to ON for Wireless Extensions
    erq->length = essid_obj->length + 1;
    if (erq->pointer)
        if (copy_to_user(erq->pointer, essid_obj->octets, erq->length))
        {
            islpci_mgt_release( private_config, entry );
            return -EFAULT;
        }
        
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setessid(struct net_device *nwdev, struct iw_point *erq)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    struct obj_ssid essid_obj;
    void *data;
    int rvalue, operation;
    unsigned long dlen;

    // prepare the structure for the set object
    essid_obj.length = erq->length - 1;

    if (copy_from_user(essid_obj.octets, erq->pointer, essid_obj.length))
        return -EFAULT;

    // request the device for the essid
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_SSID, 0, (void *) &essid_obj, sizeof(struct obj_ssid), 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_SSID, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;    
}

int isl_ioctl_getbssid(struct net_device *nwdev, char *id)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    void *data;
    int rvalue, operation;
    unsigned long dlen;

    // request the device for the essid
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_BSSID, 0, NULL, 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_BSSID, 
    	&operation, &data, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "getbssid: parameter at %p\n", data);
#endif

    // return the data if the destination buffer is defined
    if (id)
        if (copy_to_user(id, data, 6))
        {
            islpci_mgt_release( private_config, entry );
            return -EFAULT;
        }

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setbssid(struct net_device *nwdev, char *id)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char bssid[6];
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    // prepare the structure for the set object
    if (copy_from_user(&bssid[0], id, 6))
        return -EFAULT;

    // send a set object to set the bssid
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_BSSID, 0, &bssid[0], 6, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, DOT11_OID_BSSID, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getmode(struct net_device *nwdev, int *mode)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    int bsstype;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the bsstype
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_BSSTYPE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_BSSTYPE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    // returned the bsstype, store it
    bsstype = (int) *data;
    islpci_mgt_release( private_config, entry );

    // send a get object to get the mode
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        OID_INL_MODE, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, OID_INL_MODE,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    


#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "getmode: parameter at %p\n", data);
#endif

    // translate the cards mode to the wireless mode values as good as possible
    switch ((int) *data)
    {
        case INL_MODE_AP:
            *mode = IW_MODE_MASTER;
            break;

        case INL_MODE_CLIENT:
            if (bsstype == DOT11_BSSTYPE_IBSS)
                *mode = IW_MODE_ADHOC;
            else if (bsstype == DOT11_BSSTYPE_INFRA)
                *mode = IW_MODE_INFRA;
            else
                *mode = IW_MODE_AUTO;
            break;

        default:                    // DOT11_BSSTYPE_ANY
            *mode = IW_MODE_AUTO;
            break;
    }

    islpci_mgt_release( private_config, entry );
    return 0;
}


int isl_ioctl_setmode(struct net_device *nwdev, int *mode)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char cardmode[4] = { 0, 0, 0, 0 };
    char bsstype[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    switch (*mode)
    {
        case IW_MODE_ADHOC:
            bsstype[0] = DOT11_BSSTYPE_IBSS;
            cardmode[0] = INL_MODE_CLIENT;
            break;

        case IW_MODE_INFRA:
            bsstype[0] = DOT11_BSSTYPE_INFRA;
            cardmode[0] = INL_MODE_CLIENT;
            break;

        case IW_MODE_MASTER:
            bsstype[0] = DOT11_BSSTYPE_INFRA;
            cardmode[0] = INL_MODE_AP;
            break;

        case IW_MODE_AUTO:
            bsstype[0] = DOT11_BSSTYPE_ANY;
            cardmode[0] = INL_MODE_CLIENT;
            break;

        default:
            return -EOPNOTSUPP;
    }

    // send a set object to set the mode by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        OID_INL_MODE, 0, &cardmode[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, OID_INL_MODE, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( rvalue )
        return rvalue;
    else if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_BSSTYPE, 0, &bsstype[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_BSSTYPE, 
    	&operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getfreq(struct net_device *nwdev, struct iw_freq *frq)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    u32 *data, channel;
    unsigned long dlen;
    int rvalue, operation;

    // request the device for the channel
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_FREQUENCY, 0, NULL, 4, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_FREQUENCY,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // error in getting the response, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    channel = *data;

    // check whether it concern a channel number or a frequency
    // this becomes difficult because the frequency is in kHz
    if (channel < 1000)
    {
        // interpret the data as a channel
        frq->m = le32_to_cpu( channel );
        frq->e = 0;
    }
    else
    {
        // interpret the data as frequency
        frq->m = le32_to_cpu( channel );
        frq->e = 3;
    }

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setfreq(struct net_device *nwdev, struct iw_freq *frq)
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char channel[4] = { 0, 0, 0, 0 }, *data;
    long mantisse;
    long oid;
    int exponent;
    int rvalue, operation;
    unsigned long dlen;

    // prepare the structure for the set object
    if (frq->m < 1000)
    {
        // structure value contains a channel indication
	oid = DOT11_OID_CHANNEL;
        channel[0] = (char) frq->m;
        channel[1] = (char) (frq->m >> 8);
    }
    else
    {
        // structure contains a frequency indication
		oid = DOT11_OID_FREQUENCY;
	
        // mantisse is larger than 1000, so make it kHz
        mantisse = frq->m / 1000;
        exponent = frq->e;

        // process the exponent
        while (exponent > 0)
        {
            mantisse *= 10;
            exponent--;
        }

        // convert the frequency to the right byte order
        channel[0] = (char) mantisse;
        channel[1] = (char) (mantisse >> 8);
        channel[2] = (char) (mantisse >> 16);
        channel[3] = (char) (mantisse >> 24);
    }

    // send a set object to set the channel by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, oid, 0, &channel[0], 4, 0);

    // enter sleep mode after supplying oid and local buffers for reception
    // to the management queue receive mechanism
    rvalue = islpci_mgt_response(private_config, oid, &operation, 
    	(void **) &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;        
}

int isl_ioctl_getpsm(struct net_device *nwdev,
    struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *psm = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    long *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_PSM, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_PSM,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *psm = *data;

#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Get PSM 0x%08lx (%li)\n", *psm, *psm );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setpsm(struct net_device *nwdev,
    struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *psm = (long *) wrq->u.data.pointer;
    void *data;
    unsigned long dlen;
    int rvalue, operation;
	
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Set PSM: 0x%08lx\n", (long) *psm );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET,
        DOT11_OID_PSM, 0, psm, 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_PSM, &operation, 
    	&data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getnonerp(struct net_device *nwdev,
    struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *erp = (long *) wrq->u.data.pointer;
    int rvalue, operation;
    char *data;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET,
        DOT11_OID_NONERPPROTECTION, 0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_NONERPPROTECTION,
        &operation, (void **) &data, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    *erp = (long) *data;
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Get Non ERP Protection: %i\n", (int) *erp );
#endif

    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_setnonerp(struct net_device *nwdev,
    struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    long *nonerp = (long *) wrq->u.data.pointer;
    char erp[4] = { 0, 0, 0, 0 };
    void *data;
    unsigned long dlen;
    int rvalue, operation;

    erp[0] = (char) *nonerp;
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Set Non ERP Protection %i\n", (int) erp[0] );
#endif

    // send a set object to set the bsstype by placing it in the transmit queue
    islpci_mgt_queue(private_config, PIMFOR_OP_SET, DOT11_OID_NONERPPROTECTION,
        0, &erp[0], 4, 0);
    rvalue = islpci_mgt_response(private_config, DOT11_OID_NONERPPROTECTION,
        &operation, &data, &dlen, &entry );
    islpci_mgt_release( private_config, entry );
    if( operation == PIMFOR_OP_ERROR )
        return -EINVAL;
    return rvalue;
}

int isl_ioctl_getcountrystring(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    queue_entry *entry = NULL;
    char *cstring = (char *) wrq->u.data.pointer;
    char *rstring;
    int rvalue, operation;
    unsigned long dlen;

    // send a get object to get the filter setting
    islpci_mgt_queue(private_config, PIMFOR_OP_GET, DOT11_OID_COUNTRYSTRING, 
    	0, NULL, 4, 0);
    if (rvalue = islpci_mgt_response(private_config, DOT11_OID_COUNTRYSTRING, 
    	&operation, (void **) &rstring, &dlen, &entry ), rvalue != 0)
    {
        // an error occurred, return the value
        islpci_mgt_release( private_config, entry );
        return rvalue;
    }
    else if( operation == PIMFOR_OP_ERROR )
    {
        islpci_mgt_release( private_config, entry );
        return -EINVAL;
    }    

    if (copy_to_user( (char *) cstring, (void *) rstring, 4))
    {
        islpci_mgt_release( private_config, entry );
        return -EFAULT;
    }
    
#if VERBOSE > SHOW_ERROR_MESSAGES 
    DEBUG(SHOW_TRACING, "Get Country String: %02x %02x %02x %02x\n", 
    	rstring[0], rstring[1], rstring[2], rstring[3]  );
#endif
    
    wrq->u.data.length = 4;
    islpci_mgt_release( private_config, entry );
    return 0;
}

int isl_ioctl_resetdevice(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_TRACING, "Toggled the device reset flag\n");
#endif

    // set the kernel space reset flag
    private_config->resetdevice = 1;

    return 0;
}

int isl_ioctl_checktraps(struct net_device *nwdev, struct iwreq *wrq )
{
    islpci_private *private_config = nwdev->priv;
    char *frame = (char *) wrq->u.data.pointer;
    queue_entry *entry;
    int device_id, operation, flags, length;
    unsigned long oid;

    // check whether the trap queue is empty
    if( islpci_queue_size( private_config->remapped_device_base,
        &private_config->trap_rx_queue ) == 0 )
    {
        // no entries in the trap queue
#if VERBOSE > SHOW_ERROR_MESSAGES
        DEBUG(SHOW_TRACING, "TRAP receive queue empty\n");
#endif
        return -ENOMSG;
    }

    // read a frame from the queue and return it
    islpci_get_queue( private_config->remapped_device_base,
        &private_config->trap_rx_queue, &entry );

    // decode the PIMFOR frame for data length
    pimfor_decode_header( (pimfor_header *) entry->host_address, &operation,
        &oid, &device_id, &flags, &length );

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG( SHOW_TRACING, "TRAP: oid 0x%lx, device %i, flags 0x%x length %i\n",
            oid, device_id, flags, length );
#endif

    // copy the frame to the data buffer
    if (copy_to_user( frame, (void *) entry->host_address, PIMFOR_HEADER_SIZE+length ))
    {
        // free the entry to the freeq
        islpci_put_queue(private_config->remapped_device_base,
            &private_config->mgmt_rx_freeq, entry);

        return -EFAULT;
    }

    // free the entry to the freeq
    islpci_put_queue(private_config->remapped_device_base,
        &private_config->mgmt_rx_freeq, entry);

    return 0;
}

int isl_ioctl(struct net_device *nwdev, struct ifreq *rq, int cmd )
{
    struct iwreq *wrq = (struct iwreq *) rq;
    islpci_private *private_config = nwdev->priv;
    int error = 0;

#if VERBOSE > SHOW_ERROR_MESSAGES
    void *device = private_config->remapped_device_base;

    DEBUG(SHOW_FUNCTION_CALLS, "isl_ioctl cmd %x \n", cmd);

    DEBUG(SHOW_QUEUE_INDEXES, "Mgmt Tx Qs: free[%i] shdw[%i]\n",
        islpci_queue_size(device, &private_config->mgmt_tx_freeq),
        islpci_queue_size(device, &private_config->mgmt_tx_shadowq));
#endif

    switch (cmd)
    {
        case SIOCGIWNAME:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWNAME\n", nwdev->name);
#endif
            if( private_config->pci_dev_id == PCIDEVICE_ISL3877 )
                strncpy( wrq->u.name, "PRISM Indigo", IFNAMSIZ );
            else if( private_config->pci_dev_id == PCIDEVICE_ISL3890 )
                strncpy( wrq->u.name, "PRISM Duette", IFNAMSIZ );
            else
                strncpy( wrq->u.name, "Unsupported card type", IFNAMSIZ );
            break;

        case SIOCGIWRATE:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWRATE\n", nwdev->name);
#endif
            error = isl_ioctl_getrate(nwdev, &wrq->u.bitrate );
            break;

	case SIOCGIWRTS:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWRTS\n", nwdev->name);
#endif
            error = isl_ioctl_getrtsthresh(nwdev, &wrq->u.rts );
	    break;

	case SIOCSIWRTS:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWRTS\n", nwdev->name);
#endif
            error = isl_ioctl_setrtsthresh(nwdev, &wrq->u.rts );
	    break;

	case SIOCGIWFRAG:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWFRAG\n", nwdev->name);
#endif
            error = isl_ioctl_getfragthresh(nwdev, &wrq->u.frag );
	    break;

	case SIOCSIWFRAG:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWFRAG\n", nwdev->name);
#endif
            error = isl_ioctl_setfragthresh(nwdev, &wrq->u.frag );
	    break;

        case SIOCGIWRANGE:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWRANGE\n", nwdev->name);
#endif
            error = isl_ioctl_getrange(nwdev,
                (struct iw_range *) wrq->u.data.pointer );
            wrq->u.data.length = sizeof(struct iw_range);
            break;

#if WIRELESS_EXT > 11
//      case SIOCGIWSTATS:
//          DEBUG(SHOW_TRACING, "%s: SIOCGIWSTATS\n", nwdev->name);
//          error = isl_ioctl_getstats(nwdev,
//              (struct iw_statistics *) wrq->u.data.pointer );
//          break;
#endif

        case SIOCGIWESSID:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWESSID\n", nwdev->name);
#endif
            error = isl_ioctl_getessid(nwdev, &wrq->u.essid);
            break;

        case SIOCSIWESSID:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWESSID\n", nwdev->name);
#endif
            // check which operation is requied depending on the flag
            if (wrq->u.essid.flags == 0)
            {
            // disable essid checking (ESSID promiscuous)
                // is it supported by the card ?
            return -EOPNOTSUPP;
            }
            else
            {
                // set the essid to the value supplied
                error = isl_ioctl_setessid(nwdev, &wrq->u.essid);
            }

            break;

        case SIOCGIWAP:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWAP\n", nwdev->name);
#endif
            wrq->u.ap_addr.sa_family = ARPHRD_ETHER;
            error = isl_ioctl_getbssid(nwdev, &wrq->u.ap_addr.sa_data[0]);
            break;

        case SIOCSIWAP:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWAP\n", nwdev->name);
#endif
            error = isl_ioctl_setbssid(nwdev, &wrq->u.ap_addr.sa_data[0]);
            break;

        case SIOCSIWMODE:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWMODE\n", nwdev->name);
#endif
            error = isl_ioctl_setmode(nwdev, &wrq->u.mode);
            break;

        case SIOCGIWMODE:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWMODE\n", nwdev->name);
#endif
            error = isl_ioctl_getmode(nwdev, &wrq->u.mode);
            break;

        case SIOCGIWFREQ:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWFREQ\n", nwdev->name);
#endif
            error = isl_ioctl_getfreq(nwdev, &wrq->u.freq);
            break;

        case SIOCSIWFREQ:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCSIWFREQ\n", nwdev->name);
#endif
            error = isl_ioctl_setfreq(nwdev, &wrq->u.freq);
            break;

        case SIOCGIWPRIV:
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCGIWPRIV\n", nwdev->name);
#endif
            if (wrq->u.data.pointer)
            {
                if( error = verify_area(VERIFY_WRITE, wrq->u.data.pointer,
                    sizeof(privtab)), error )
                {
#if VERBOSE > SHOW_ERROR_MESSAGES
                    DEBUG(SHOW_TRACING, "verify_area error %i\n", error );
#endif
                    break;
                }

                wrq->u.data.length = sizeof(privtab) / sizeof(privtab[0]);
                if (copy_to_user(wrq->u.data.pointer, privtab, sizeof(privtab)))
                {
#if VERBOSE > SHOW_ERROR_MESSAGES
                    DEBUG(SHOW_TRACING, "copy_to_user error %i\n", error );
#endif
                    error = -EFAULT;
                }
            }
            break;

        case SIOCIWFIRSTPRIV + 0x00:                    // setauthenten
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+0 (setauthenten)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setauthenten(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x01:                    // getauthenten
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+1 (getauthenten)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getauthenten(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x02:                    // setunencrypted
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+2 (setunencrypt)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setprivfilter(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x03:                    // getunencrypted
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+3 (getunencrypt)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getprivfilter(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x04:                    // setprivinvoke
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+4 (setprivinvoke)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setprivinvoke(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x05:                    // getprivinvoke
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+5 (getprivinvoke)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getprivinvoke(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x06:                    // setdefkeyid
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+6 (setdefkeyid)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setdefkeyid(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x07:                    // getdefkeyid
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+7 (getdefkeyid)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getdefkeyid(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x08:                    // setdefkeyx
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+8 (setdefkeyx)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setdefkeyx(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x09:                    // getdefkeyx
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+9 (getdefkeyx)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getdefkeyx(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0A:                    // getprivstat
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+10 (getprivstat)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getprivstat(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0B:                    // getsuprates
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+11 (getsuprates)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getsuprates(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0C:                    // setdtimper
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+12 (setdtimper)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setdtimper(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0D:                    // getdtimper
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+13 (getdtimper)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getdtimper(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0E:                    // setbeaconper
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+14 (setbeaconper)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setbeaconper(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x0F:                    // getauthenten
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+15 (getbeaconper)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getbeaconper(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x10:                    // wdslinkadd
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+16 (wdslinkadd)\n",
                nwdev->name);
#endif
            error = isl_ioctl_wdslinkadd(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x11:                    // wdslinkdel
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+17 (wdslinkdel)\n",
                nwdev->name);
#endif
            error = isl_ioctl_wdslinkdel(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x12:                    // eapauthen
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+18 (eapauthen)\n",
                nwdev->name);
#endif
            error = isl_ioctl_eapauthen(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x13:                    // eapunauth
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+19 (eapunauth)\n",
                nwdev->name);
#endif
            error = isl_ioctl_eapunauth(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x14:                    // setdot1xen
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+20 (setdot1xen)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setdot1xen(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x15:                    // getdot1xen
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+21 (getdot1xen)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getdot1xen(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x16:                    // setstakeyx
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+22 (setstakeyx)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setstakeyx(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x17:                    // getstakeyx
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+23 (getstakeyx)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getstakeyx(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x18:                    // setfixedrate
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+24 (setfixedrate)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setfixedrate(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x19:                    // getfixedrate
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+25 (getfixedrate)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getfixedrate(nwdev, wrq);
            break;

/*
        case SIOCIWFIRSTPRIV + 0x18:                    // setfragthresh
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+24 (setfragthresh)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setfragthresh(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x19:                    // getfragthresh
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+25 (getfragthresh)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getfragthresh(nwdev, wrq);
            break;
*/
        case SIOCIWFIRSTPRIV + 0x1A:                    // setpsm
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+26 (setpsm)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setpsm(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x1B:                    // getpsm
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+27 (getpsm)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getpsm(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x1C:                    // setnonerp
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+28 (setnonerp)\n",
                nwdev->name);
#endif
            error = isl_ioctl_setnonerp(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x1D:                    // getnonerp
#if VERBOSE > SHOW_ERROR_MESSAGES 
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+29 (getnonerp)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getnonerp(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x1E:                    // getcountrystr
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+30 (getcountrystr)\n",
                nwdev->name);
#endif
            error = isl_ioctl_getcountrystring(nwdev, wrq);
            break;

        case SIOCIWFIRSTPRIV + 0x1F:                    // checktraps
#if VERBOSE > SHOW_ERROR_MESSAGES
            DEBUG(SHOW_TRACING, "%s: SIOCIWFIRSTPRIV+31 (checktraps)\n",
                nwdev->name);
#endif
            error = isl_ioctl_checktraps(nwdev, wrq);
            break;

        default:
            error = -EOPNOTSUPP;
    }

#if VERBOSE > SHOW_ERROR_MESSAGES
    DEBUG(SHOW_QUEUE_INDEXES,
        "Mgmt Rx Qs: f[%i] s[%i] i[%i] t[%i] p[%i]\n",
        islpci_queue_size(device, &private_config->mgmt_rx_freeq),
        islpci_queue_size(device, &private_config->mgmt_rx_shadowq),
        islpci_queue_size(device, &private_config->ioctl_rx_queue),
        islpci_queue_size(device, &private_config->trap_rx_queue),
        islpci_queue_size(device, &private_config->pimfor_rx_queue));
#endif

    return error;
}

#else   // no WIRELESS_IOCTLS

int isl_ioctl(struct net_device *nwdev, struct ifreq *rq, int cmd )
{
    int error = 0;

    switch (cmd)
    {
        default:
            error = -EOPNOTSUPP;
    }

    return error;
}

#endif  // WIRELESS_IOCTLS
