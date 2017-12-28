/*
 * tcpConn data access header
 *
 * $Id: tcpConn.h,v 1.1 2004/09/16 15:31:48 rstory Exp $
 */
#ifndef NETSNMP_ACCESS_TCPCONN_H
#define NETSNMP_ACCESS_TCPCONN_H

# ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */

    /*
     * tcpconnLocalAddress(2)/IPADDR/ASN_IPADDRESS/u_long(u_long)//l/A/w/e/r/d/h
     * tcpconnLocalPort(3)/INTEGER/ASN_INTEGER/long(long)//l/A/w/e/R/d/h
     * tcpconnRemAddress(4)/IPADDR/ASN_IPADDRESS/u_long(u_long)//l/A/w/e/r/d/h
     * tcpconnRemPort(5)/INTEGER/ASN_INTEGER/long(long)//l/A/w/e/R/d/h
     */
#define NETSNMP_TCPCONN_IDX_LOCAL_ADDR       0
#define NETSNMP_TCPCONN_IDX_LOCAL_PORT       1
#define NETSNMP_TCPCONN_IDX_REMOTE_ADDR      2
#define NETSNMP_TCPCONN_IDX_REMOTE_PORT      3

/*
 * netsnmp_tcpconn_entry
 *   - primary tcpconn structure for both ipv4 & ipv6
 */
typedef struct netsnmp_tcpconn_s {

   netsnmp_index oid_index;   /* MUST BE FIRST!! for container use */
   oid           indexes[4];

   int       flags; /* for net-snmp use */

   /*
    * mib related data (considered for
    *  netsnmp_access_tcpconn_entry_update)
    */
   
   /*
    * tcpconnState(1)/INTEGER/ASN_INTEGER/long(u_long)//l/A/W/E/r/d/h
    */
   u_long          tcpConnState;
   
   netsnmp_data_list *arch_data;      /* arch specific data */
   
} netsnmp_tcpconn_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * ifcontainer init
 */
netsnmp_container * netsnmp_access_tcpconn_container_init(u_int init_flags);
#define NETSNMP_ACCESS_TCPCONN_INIT_NOFLAGS               0x0000
#define NETSNMP_ACCESS_TCPCONN_INIT_ADDL_IDX_BY_ADDR      0x0001

/*
 * ifcontainer load and free
 */
netsnmp_container*
netsnmp_access_tcpconn_container_load(netsnmp_container* container,
                                      u_int load_flags);
#define NETSNMP_ACCESS_TCPCONN_LOAD_NOFLAGS               0x0000

void netsnmp_access_tcpconn_container_free(netsnmp_container *container,
                                           u_int free_flags);
#define NETSNMP_ACCESS_TCPCONN_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_TCPCONN_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free a tcpconn entry
 */
netsnmp_tcpconn_entry *
netsnmp_access_tcpconn_entry_create(void);

void netsnmp_access_tcpconn_entry_free(netsnmp_tcpconn_entry * entry);

/*
 * update/compare
 */
int
netsnmp_access_tcpconn_entry_update(netsnmp_tcpconn_entry *old, 
                                      netsnmp_tcpconn_entry *new);

/*
 * find entry in container
 */
/** not yet */

/*
 * create/change/delete
 */
int
netsnmp_access_tcpconn_entry_set(netsnmp_tcpconn_entry * entry);


/*
 * tcpconn flags
 *   upper bits for internal use
 *   lower bits indicate changed fields. see FLAG_TCPCONN* definitions in
 *         tcpConnTable_constants.h
 */
#define NETSNMP_ACCESS_TCPCONN_CREATE     0x80000000
#define NETSNMP_ACCESS_TCPCONN_DELETE     0x40000000
#define NETSNMP_ACCESS_TCPCONN_CHANGE     0x20000000


/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_TCPCONN_H */
