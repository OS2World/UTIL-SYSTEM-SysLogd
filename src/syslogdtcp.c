/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 1.0a      Michael K Greene <greenemk@cox.net>
 *                      January 2005
 *
 *                      Based on original BSD Syslogd
 *
 *
 *            SYSLOGD/2 compiles with OpenWatom (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  The syslogd UDP thread module.  Handles syslog messages
 *               received on Port 514 and signals syslogd for processing.
 *
 *               Changes from v1.0 September 2004:
 *               - clean-up used code
 *               - remove TCPIP timer
 *
 ***************************************************************************/

#define  INCL_DOSPROCESS
#define  INCL_DOSSEMAPHORES
#define  INCL_DOSMEMMGR

#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <sys\time.h>
#include <sys\socket.h>
#include <arpa\inet.h>
#include <netinet\in.h>
#include <netdb.h>
#include <unistd.h>
#include <process.h>
#include "syslogd.h"


void syslogdtcp( PVOID );       // main start of thread

static  int   setuptcp( void );   // set up TCP socket
static  void  closetcp( void );   // close up and end
extern  void  handleinternalmsg(int msgnum, int sev);
extern struct STATS stats;

extern struct PROGRAMOPTS *prgoptions;

int  tcpsocket;

// Shared memory buffer defined in syslogd.cpp
extern SHAREMEM  *pShareMem;
extern PSZ       szShareMem;
extern PSZ       MEMMUX;      // MUX to signal access to shared memory buffer
extern HMTX      mtxMEMMUX;

// SEMAPHOREs define in syslogd main
extern  PSZ    TCPSEM1;       // SEMAPHORE to signal TCP start / stop
extern  HEV    hevTCPSEM1;
extern  ULONG  TCPSEM1Ct;

// SEMAPHOREs define in syslogd main
extern  PSZ    TCPSEM2;       // SEMAPHORE to signal
extern  HEV    hevTCPSEM2;
extern  ULONG  TCPSEM2Ct;

// SEMAPHOREs define in syslogd main
extern  PSZ    IDLESEM;       // SEMAPHORE to stop IDLE
extern  HEV    hevIDLESEM;
extern  ULONG  IDLESEMCt;


// syslogdtcp( )
//
//  Entry into syslogdipc.cpp to setup and start UDP listening
//
void syslogdtcp(  )
{
    bool firststart = TRUE;
    int  msgrecv;
    int  seterr;
    int  i;
    char recv_buffer[1024];
    char *ptr_recv_buffer = recv_buffer;

    // setup for running, if error bug out - don't worry about main
    // will timeout waiting for TCPSEM1.
    seterr = setuptcp( );
    if(seterr)
    {
        if(seterr == 1)
        {
            psock_errno( NULL );
            closetcp( );
        }
        if(seterr == 3) printf("DosGetSharedMem failed\n");
    }

    // main working loop **********************
    while(TRUE)
    {
        // Just my play.  Set SEM1 (SYSLOGD is waiting), delay 1 sec
        // (let SYSLOGD reset), and start now FALSE - continue -
        // sleep 1.5 sec for SEM sets or thread will shutdown
        if(firststart)
        {
            DosPostEventSem(hevTCPSEM1); // tell main I am up!!
            DosSleep(1500);
            firststart = FALSE;
        }

        // clear recv_buffer
        for(i = 0; i < 1024; i++) recv_buffer[i] = 0;

        msgrecv = recv(tcpsocket, ptr_recv_buffer, 1024, 0);  // read socket

        // have a message msgrecv > 0 and not blocked
        if(msgrecv > 0 && prgoptions->securemode)
        {
            if(DosRequestMutexSem(mtxMEMMUX, 60000L) != 0)
            {
                printf("SYSLOGTCP: Timeout waiting for MUX\n");
            }
            else
            {
                if(pShareMem->slotstat[pShareMem->index])
                {
                    printf("SYSLOGTCP: Buffer full!!\n");
                    stats.tcpoverflows++;
                }
                else
                {
                    // clear the slot
                    strnset(pShareMem->data[pShareMem->index], '\0 ',
                                     sizeof(pShareMem->data[pShareMem->index]));

                    // write message into slot replace any character < 32 or > 126
                    // with a space RFC 3164 characters must be in this range
                    for(i = 0; i < strlen(recv_buffer); i++)
                    {
                        if(((int)recv_buffer[i] < 32) || ((int)recv_buffer[i] > 126))
                             pShareMem->data[pShareMem->index][i] = ' ';
                        else pShareMem->data[pShareMem->index][i] = recv_buffer[i];
                    }

                    // set flag slot has message
                    pShareMem->slotstat[pShareMem->index] = 1;

                    ++pShareMem->index;  // increment index to next slot
                    ++stats.systcprecvmsg;

                    // ++tcpatcount;

                    // if reached top of buffer reset index to 0 or bottom
                    if(pShareMem->index == (BUFFER_SLOTS)) pShareMem->index = 0;

                    DosPostEventSem(hevIDLESEM); // wakeup MAIN if IDE
                }
                DosReleaseMutexSem(mtxMEMMUX);
            }

        }  // end of msg recv


        // check for shutdown signal from main
        DosQueryEventSem(hevTCPSEM2, &TCPSEM2Ct);
        if(TCPSEM2Ct && !firststart)
        {
            DosResetEventSem(hevTCPSEM1, &TCPSEM1Ct);
            closetcp( );
        }

        // sleep 1 minute if blocked so do not suckup  the CPU
        if(!prgoptions->securemode) DosSleep(60000L);

    }  //$* end working loop

}  //$* end of syslogdtcp


// setuptcp( )
//
//  Setup socket, bind, and set socket to timeout recv after 2 seconds
//
int setuptcp( )
{
//    int  retcode = 0;
    int  flag =1;
    int  syslport;

    struct timeval      recvtimeout;
    struct sockaddr_in  tcp_in;
    struct servent      *sp;

    sp = getservbyname("syslog"  , "udp");
    syslport = htons(sp->s_port);

    // Setup socket - descriptor tcpsocket
    tcpsocket = socket(PF_INET, SOCK_DGRAM, 0);

    if(tcpsocket == -1) return(1);

    // clear tcp_in and set values
    memset(&tcp_in, 0, sizeof(tcp_in));
    tcp_in.sin_len         = sizeof(tcp_in);
    tcp_in.sin_family      = AF_INET;
    tcp_in.sin_addr.s_addr = inet_addr(prgoptions->sendIP);//htonl(INADDR_ANY);
    tcp_in.sin_port        = syslport;

    // setup recv 2 minute timeout
    recvtimeout.tv_sec  = 1;
    recvtimeout.tv_usec = 0;


    // set socket recv timeout and reuse
    if(setsockopt(tcpsocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvtimeout,
                  sizeof(struct timeval)) == -1) return(1);

    if (setsockopt(tcpsocket, SOL_SOCKET, SO_REUSEPORT,(const char*)&flag,
                  sizeof(flag))) return(1);

    // bind to socket
    if(bind(tcpsocket, (struct sockaddr *)&tcp_in, sizeof(tcp_in))) return(1);

    // get access to the buffer
    if(DosGetNamedSharedMem((PPVOID)&pShareMem,
                  SHMEM_NAME, PAG_READ | PAG_WRITE) != 0) return(3);

    return 0;

}  //$* end of setuptcp


// closetcp( )
//
//  Close the socket, set TCPSEM1, and end thread
//
void closetcp( )
{
    soclose(tcpsocket);
    DosFreeMem(pShareMem);       // free the buffer memory
    DosPostEventSem(hevTCPSEM1); // signal syslogd main thread shutdown
    _endthread();

}  //$* end of setupudp


