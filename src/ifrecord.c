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
 * Description:  When getifrecords( ) is called it will use os2_ioctl( )
 *               and ioctl( ) to pull the system's interface information
 *               and return it in a linked-list of structures for each
 *               interface. See ifrecord.h for use.
 *
 ***************************************************************************/

#include <string.h>
#include <types.h>
#include <malloc.h>
#include <unistd.h>
#include <sys\ioctl.h>
#include <sys\socket.h>
#include <net\if.h>
#include "ifrecord.h"


// using SIOSTATIF42 so make buffer size to hold a max
// of 42 plus 1 for index
#define IFRECSIZE  111
#define IPRECSIZE  14
#define IFBUFFER   (IFMIB_ENTRIES * IFRECSIZE) + 1
#define IPBUFFER   (IFMIB_ENTRIES * IPRECSIZE) + 1
#define IFNAMESZ   1024


#pragma pack(1)
// I know it is overkill but need to make effort to have
// enough room for 42 entries
struct ipmib {
    short  ipNumber;
    struct statatreq iptable[IFMIB_ENTRIES];
};

// this is the data structure for pulling the interface name
// when the record is AF_INET
struct AFData {
    char   if_name[IFNAMSIZ];
    struct sockaddr_in  if_addr;
};

// structure the interface data is transfered to for look up
struct AFLookup {
    char   if_name[IFNAMSIZ];
    struct in_addr ifethaddr;
    struct AFLookup *nextif;
};
#pragma pack( )


// getifrecords( )
//
//  Use 2 calls to os2_ioctl( ) and one to ioctl( ) to get interface
//  information and combine the results in a linked list of ifrecord
//  structures.
//
struct ifrecord *getifrecords( void )
{
    int  rc;
    int  i1;
    int  i2;
    int  sock;
    int  size;
    int  rec_count = 0;

    char *ifbuffer = NULL;
    char *poscount = NULL;

    struct ifmib    *if_info     = NULL;
    struct ipmib    *ip_info     = NULL;
    struct iftable  *if_table    = NULL;
    struct ifrecord *pif_record  = NULL;
    struct ifrecord *pif_recprev = NULL;
    struct ifrecord *pif_head    = NULL;
    struct AFData   *ifrecord    = NULL;
    struct AFLookup *iflookup    = NULL;
    struct AFLookup *aftmp       = NULL;
    struct AFLookup *afprev      = NULL;
    struct ifconf   ifc;

    // open socket to use for ioctl calls
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == -1) {
        psock_errno("socket");
        return(NULL);
    }

    // allocate memory, clear it, and point to the iftable array (ifmib)for
    // all address
    if_info = malloc(IFBUFFER);
    memset(if_info, 0, IFBUFFER);

    if_table = if_info->iftable;

    rc = os2_ioctl(sock, SIOSTATIF42, (char *)if_info, sizeof(IFBUFFER));

    // if os2_ioctl error or no address in system return NULL
    if((rc == -1) || (if_info->ifNumber == 0)) {
        soclose(sock);
        free(if_info);
        if(rc == -1) psock_errno("os2_ioctl(1)");
        return(NULL);
    }

    // allocate memory and clear it
    ip_info = malloc(IPBUFFER);
    memset(ip_info, 0, IPBUFFER);

    // os2_ioctl call 2: gets IP address, IF index, netmask, and
    // broadcast address for all system addresses
    rc = os2_ioctl(sock, SIOSTATAT, (char *)ip_info, IPBUFFER);

    // if os2_ioctl error or no address in system return NULL
    if((rc == -1) || (ip_info->ipNumber == 0)) {
        soclose(sock);
        free(if_info);
        free(ip_info);
        if(rc == -1) psock_errno("os2_ioctl(2)");
        return(NULL);
    }

    // get memory and setup for ioctl - SIOCGIFCONF call which
    // retrieve the IF Name and address
    ifbuffer    = malloc(IFNAMESZ);
    ifc.ifc_len = IFNAMESZ;
    ifc.ifc_buf = ifbuffer;

    if(ioctl(sock, SIOCGIFCONF, (char *)&ifc) == -1) {
        close(sock);
        free(if_info);
        free(ip_info);
        free(ifbuffer);
        psock_errno("ioctl");
        return(NULL);
    }

    // close socket - no longer needed
    close(sock);

    poscount = ifbuffer;

    // run through the buffer from previous ioctl( ) call and for each AF_INET
    // place the interface name and address in a AFData struct
    while(rec_count != ifc.ifc_len) {
        ifrecord = (struct AFData *)poscount;      //buffer;
        if(ifrecord->if_addr.sin_family == AF_INET) {
            aftmp = malloc(sizeof(struct AFLookup));

            if(iflookup  == NULL) iflookup  = aftmp;

            if(afprev != NULL) afprev->nextif = aftmp;

            strcpy(aftmp->if_name, ifrecord->if_name);
            aftmp->ifethaddr.s_addr = ifrecord->if_addr.sin_addr.s_addr;
            aftmp->nextif = NULL;

            afprev = aftmp;
        }

        size = ifrecord->if_addr.sin_len + sizeof(ifrecord->if_name);
        rec_count+=size;

        for(i1 = 0; i1 < size; i1++) poscount++;
    }

    // *****!!! free ifbuffer
    free(ifbuffer);

    // combine all 3 of the previous results into one record with the info
    // required into ifrecord structures.
    pif_record = malloc(sizeof(struct ifrecord));
    memset(pif_record, 0, sizeof(struct ifrecord));

    for(i1 = 0; i1 < ip_info->ipNumber; i1++) {
        if(i1 == 0) pif_head = pif_record; // linked list head

        // Run through the ip_info list and transfer data to from
        // ip_info and if_info structs to a ifrecord.
        pif_record->ifIndex = ip_info->iptable[i1].interface;
        pif_record->ifEthrAddr.s_addr = ip_info->iptable[i1].addr;
        pif_record->mask.s_addr       = ntohl(ip_info->iptable[i1].mask);
        pif_record->broadcast.s_addr  = ip_info->iptable[i1].broadcast;
        pif_record->ifMtu             = (if_table[pif_record->ifIndex]).iftMtu;
        pif_record->iftOperStatus     = (if_table[pif_record->ifIndex]).iftOperStatus;
        pif_record->ifType            = (if_table[pif_record->ifIndex]).iftType;
        pif_record->nextif            = NULL ;

        // The loopback interface returns a description of Ethernet-Csmacd
        // but I want it to return "Software Loopback"
        if((pif_record->ifType == 24) && (pif_record->iftOperStatus == 0))
                           strlcpy(pif_record->ifDescr, "Software Loopback", 18);

        if(pif_record->ifType == 6) strlcpy(pif_record->ifDescr,
                           if_table->iftDescr, strlen(if_table->iftDescr)+1);

        // transfer the HW address (ethernet) to ifrecord
        for(i2 = 0; i2 < 6; i2++) pif_record->ifPhysAddr[i2] =
                                  (if_table[pif_record->ifIndex]).iftPhysAddr[i2];

        aftmp = iflookup;

        // Run through the if_record linked list and if the IP address matches
        // transfer the interface name as known to the system.
        while(1) {
            if(aftmp->ifethaddr.s_addr==pif_record->ifEthrAddr.s_addr) {
                strlcpy(pif_record->ifName, aftmp->if_name, IFNAMSIZ);
                break;
            } else if(aftmp->nextif == NULL) {
                strlcpy(pif_record->ifName, "UNKNOWN", IFNAMSIZ);
                break;
            }
            aftmp = aftmp->nextif;
        }

        pif_recprev = pif_record; // set pif_recprevious to pif_record
                                  // for up coming link
        pif_record = malloc(sizeof(struct ifrecord));
        memset(pif_record, 0, sizeof(struct ifrecord));
        pif_recprev->nextif = pif_record; // set up link to new record
    }

    // free the memory and return
    free(if_info);
    free(ip_info);

    while(1) {
        if(iflookup->nextif == NULL) {
            free(iflookup);
            break;
        }
        aftmp = iflookup;
        iflookup = iflookup->nextif;
        free(aftmp);
    }

    return(pif_head);

} // end of getifrecords( )


// if_freemem( )
//
//  Walks the linked-list of interfaces and frees memory
//
void if_freemem(struct ifrecord *head)
{
    struct ifrecord *ptmp = NULL;

    while(1) {
        if(head->nextif == NULL) {
            free(head);
            head = NULL;
            break;
        }
        ptmp = head;
        head = head->nextif;
        free(ptmp);
    }

} // end of if_freemem( )





