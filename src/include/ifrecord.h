/****************************************************************************
 *
 *                        Interface Records Module
 *
 *  ========================================================================
 *
 *    Version 1.0       Michael K Greene <greenemk@cox.net>
 *                      November 2005
 *
 *  ========================================================================
 *
 * Description:  When getifrecords( ) is called it will use os2_ioctrl( )
 *               to pull the system's interface information and return it
 *               in a linked-list of structures for each interface.
 *
 ***************************************************************************/

#ifndef _IFRECORD_H_
#define _IFRECORD_H_

#include <net\if.h>

#pragma pack(1)

struct ifrecord {
    short  ifIndex;              // interface number
    char   ifDescr[45];          // interface description
    short  ifType;               // ethernet, tokenring, etc
    char   ifPhysAddr[6];        // interface hardware address
    struct in_addr ifEthrAddr;   // ethernet address
    struct in_addr mask;         // interface mask
    struct in_addr broadcast;    // boadcast address
    char   ifName[IFNAMSIZ];     // interface name (lan0, ppp0...)
    short  ifMtu;                // maximum transmission unit
    short  iftOperStatus;        // SNMP Oper Status
    struct ifrecord *nextif;     // pointer to next interface structure
};

#pragma pack( )


/*
 I leave it up to the calling program to
 Defined in net\if.h:

HT_IP         1  IP
HT_ETHER      6  Ethernet          lan
HT_ISO88023   7  CSMA CD
HT_ISO88025   9  Token Ring
HT_PPP       24  PPP IP            sl
HT_SLIP      28  Serial Line IP    ppp

*/

__BEGIN_DECLS

// returns a pointer to the first ifrecord structure
struct ifrecord *getifrecords( void );

// pass the pointer from getifrecords( ) to free all records created
void if_freemem(struct ifrecord *pif_stlist);

__END_DECLS

#endif

