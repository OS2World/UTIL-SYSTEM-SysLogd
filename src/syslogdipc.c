/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 1.0       Michael K Greene <greenemk@cox.net>
 *                      September 2004
 *
 *                      Based on original BSD Syslogd
 *
 *
 *            SYSLOGD/2 compiles with OpenWatom (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  The syslogd IPC thread module.  Handles syslog messages
 *               received on \socket\syslog and signals syslogd for
 *               processing.
 *
 ***************************************************************************/

#define  INCL_DOSPROCESS
#define  INCL_DOSSEMAPHORES

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <sys\time.h>
#include <sys\socket.h>
#include <sys\un.h>
#include <unistd.h>
#include <process.h>
#include "syslogd.h"


void syslogdipc( PVOID );                            // main start of thread

static int  setupipc( void );                        // set up IPC socket
static void closeipc( void );                        // close up and end
extern void handleinternalmsg(int msgnum, int sev);  // handle errors

extern struct STATS stats;

static int ipcsocket;

// Shared memory buffer defined in syslogd.cpp
extern SHAREMEM *pShareMem;
extern PSZ      szShareMem;
extern PSZ      MEMMUX;      // MUX to signal access to shared memory buffer
extern HMTX     mtxMEMMUX;

// SEMAPHOREs define in syslogd main
extern PSZ   IPCSEM1;       // SEMAPHORE to signal IPC start / stop
extern HEV   hevIPCSEM1;
extern ULONG IPCSEM1Ct;

// SEMAPHOREs define in syslogd main
extern PSZ   IPCSEM2;       // SEMAPHORE to signal
extern HEV   hevIPCSEM2;
extern ULONG IPCSEM2Ct;

// SEMAPHOREs define in syslogd main
extern  PSZ    IDLESEM;       // SEMAPHORE to stop IDLE
extern  HEV    hevIDLESEM;
extern  ULONG  IDLESEMCt;


// syslogdipc( )
//
//  Entry into syslogdipc.cpp to setup and start IPC listening
//
void syslogdipc( )   //PVOID
{
    bool firststart = TRUE;
    int  msgrecv;
    int  seterr;
    int  i;
    char recv_buffer[1024];
    char *ptr_recv_buffer = recv_buffer;

    // setup for running, if error bug out - don't worry about main
    // it will timeout waiting for IPCSEM1.
    seterr = setupipc( );

    if(seterr) {
        if(seterr == 1) {
            psock_errno( NULL );
            closeipc( );
        }
        if(seterr == 2) printf("SYSLOGIPC: DosGetSharedMem failed\n");
    }

    // main working loop **********************
    while(TRUE)
    {

        // Just my play.  Set SEM1 (SYSLOGD is waiting), delay 1 sec
        // (let SYSLOGD reset), and start now FALSE - continue -
        // sleep 1.5 sec for SEM sets or thread will shutdown
        if(firststart)
        {
            DosPostEventSem(hevIPCSEM1); // tell main I am up!!
            DosSleep(1500);
            firststart = FALSE;
        }

        // clear recv_buffer
        for(i = 0; i < 1024; i++) recv_buffer[i] = 0;

        msgrecv = recv(ipcsocket, ptr_recv_buffer, 1024, 0);

        // have a message msgrecv > 0
        if(msgrecv > 0)
        {
            if(DosRequestMutexSem(mtxMEMMUX, 60000L) != 0) printf("SYSLOGIPC: Timeout waiting for MUX\n");
            else
            {
                if(pShareMem->slotstat[pShareMem->index])
                {
                    printf("Buffer full!!\n");
                    stats.ipcoverflows++;
                }
                else
                {

                    // clear the slot
                    strnset(pShareMem->data[pShareMem->index], '\0 ', sizeof(pShareMem->data[pShareMem->index]));

                    // write message into slot
                    // replace any character < 32 or > 126 with a space
                    // RFC 3164 characters must be in this range
                    for(i = 0; i < strlen(recv_buffer); i++) {
                        if(((int)recv_buffer[i] < 32) || ((int)recv_buffer[i] > 126))
                             pShareMem->data[pShareMem->index][i] = ' ';
                        else pShareMem->data[pShareMem->index][i] = recv_buffer[i];
                    }

                    // set flag slot has message
                    pShareMem->slotstat[pShareMem->index] = 1;

                    // increment index to next slot
                    ++pShareMem->index;
                    stats.sysipcrecvmsg++;

                    // if reached top of buffer reset index to 0 or bottom
                    if(pShareMem->index == (BUFFER_SLOTS)) pShareMem->index = 0;

                    DosPostEventSem(hevIDLESEM); // wakeup MAIN if IDE
                }
                DosReleaseMutexSem(mtxMEMMUX);
            }
        }

        // check for shutdown signal from main
        DosQueryEventSem(hevIPCSEM2, &IPCSEM2Ct);
        if(IPCSEM2Ct && !firststart) {
            DosResetEventSem(hevIPCSEM1, &IPCSEM1Ct);
            closeipc( );
        }

    }
}  //$* end of syslogdipc


// setupipc( )
//
//  Setup socket, bind, and set socket to timeout recv after 2 seconds
//
int setupipc( )
{
    // int    retcode = 0;
    struct timeval      recvtimeout;
    struct sockaddr_un  ipc_un;

    // Setup socket - descriptor ipcsocket
    ipcsocket = socket(PF_OS2, SOCK_DGRAM, 0);
    if(ipcsocket == -1) return(1);

    // clear ipc_un and set values
    memset(&ipc_un, 0, sizeof(ipc_un));
    ipc_un.sun_len    = sizeof(ipc_un);
    ipc_un.sun_family = AF_OS2;
    strcpy(ipc_un.sun_path, IPC_NAME);

    // setup recv 2 minute timeout
    recvtimeout.tv_sec  = 1;
    recvtimeout.tv_usec = 0;

    // set socket recv timeout
    if(setsockopt(ipcsocket,
                  SOL_SOCKET,
                  SO_RCVTIMEO,
                  (char *)&recvtimeout,
                  sizeof(struct timeval)) == -1) return(1);

    // bind to socket
    if(bind(ipcsocket, (struct sockaddr *)&ipc_un, sizeof(ipc_un))) return(1);

    // get access to the buffer
    if(DosGetNamedSharedMem((PPVOID)&pShareMem,
                  SHMEM_NAME, PAG_READ | PAG_WRITE) != 0) return(2);

    return 0;

}  //$* end of setupipc


// closeipc( )
//
//  Close the socket, set IPCSEM1, and end thread
//
void closeipc( )
{
    soclose(ipcsocket);
    DosFreeMem(pShareMem);       // free the buffer memory
    DosPostEventSem(hevIPCSEM1); // signal syslogd main thread shutdown
    _endthread();

}  //$* end of setupipc


