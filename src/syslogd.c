/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 2.0       Michael K Greene <greenemk@cox.net>
 *                      December 2005
 *
 *                      Based on original BSD Syslogd
 *
 *
 *         SYSLOGD/2 compiles with Open Watcom (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  Main syslogd process. Sets up the program defaults and spawns
 *               threads (IPC, TCPIP, and NAMED PIPE) Reads incomming messages
 *               from the shared buffer and sends to the modified syslogd BSD
 *               module.
 *
 *  1.0  October  2004
 *  2.0  December 2005
 *  2.01 January  2007
 *
 ***************************************************************************/

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_DOSDATETIME
#define INCL_DOSNMPIPES

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <types.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys\un.h>
#include <netinet/in.h>
#include <arpa\inet.h>
#include <signal.h>
#include <version.h>
#include "syslogmsg.h"
#include "syslogd.h"
#include "ifrecord.h"


extern void syslogdipc( PVOID );   /* spawn IPC thread  */
extern void syslogdtcp( PVOID );   /* spawn TCP thread  */
extern void syslogdpipe( PVOID );  /* spawn PIPE thread */
static void usage(void);
static void sendIPCtest( void );
static void sendUDPtest( void );
static void sendTrunc( void );
static int  startup( void );       /* main syslogd startup function */

void logerror(int sev, const char *, ...);
void printline(char *hname, char *msg);
void domark(int mark);
char *gettimestamp( void );        /* get timestap IAW RFC 3164 */
void initialize(struct  PROGRAMOPTS *prgoptions);
void poplog(struct PROGRAMOPTS *prgoptions);
void getpopcfg(struct PROGRAMOPTS *prgoptions);
void sysshutdown(int signo);       /* main syslogd shutdown function */


/********** Semaphores **********/

// MUX to signal buffer access
PSZ    MEMMUX      = MUX_NAME;
HMTX   mtxMEMMUX   = 0;
BOOL32 MUXSTATE    = FALSE;

// SEM to signal IPC start / stop (SEM1)
PSZ    IPCSEM1    = "\\SEM32\\SYSLOGD\\IPCSEM1";
HEV    hevIPCSEM1 = 0;
ULONG  IPCSEM1Ct  = 0;

// SEM to signal IPC start / stop (SEM2)
PSZ    IPCSEM2    = "\\SEM32\\SYSLOGD\\IPCSEM2";
HEV    hevIPCSEM2 = 0;
ULONG  IPCSEM2Ct  = 0;

// SEM to signal TCP start / stop (SEM3)
PSZ    TCPSEM1    = "\\SEM32\\SYSLOGD\\TCPSEM1";
HEV    hevTCPSEM1 = 0;
ULONG  TCPSEM1Ct  = 0;

// SEM to signal TCP start / stop (SEM4)
PSZ    TCPSEM2    = "\\SEM32\\SYSLOGD\\TCPSEM2";
HEV    hevTCPSEM2 = 0;
ULONG  TCPSEM2Ct  = 0;

// SEM to sign SYSLOGPIPE start / stop (SEM5)
PSZ    PIPESEM1    = "\\SEM32\\SYSLOGD\\PIPESEM1";
HEV    hevPIPESEM1 = 0;
ULONG  PIPESEM1Ct  = 0;

// SEM to signal SYSLOGPIPE start / stop (SEM6)
PSZ    PIPESEM2    = "\\SEM32\\SYSLOGD\\PIPESEM2";
HEV    hevPIPESEM2 = 0;
ULONG  PIPESEM2Ct  = 0;

// SEM to signal CMDSHUTDOWN (SEM7)
PSZ    PIPESEM3    = "\\SEM32\\SYSLOGD\\PIPESEM3";
HEV    hevPIPESEM3 = 0;
ULONG  PIPESEM3Ct  = 0;

// SEM to signal IDLESEM (SEM8), added v2.01
PSZ    IDLESEM    = "\\SEM32\\SYSLOGD\\IDLESEM";
HEV    hevIDLESEM = 0;
ULONG  IDLESEMCt  = 0;

// SEM to signal MARK timer (DosTimer1)
static  PSZ    MARKSEM     = "\\SEM32\\SYSLDMP\\MARKTIMER";
static  HEV    hevMARKSEM  = 0;
static  ULONG  MARKSEMCt   = 0;
static  HTIMER hMARKEvent  = 0;

// SEM to signal FLUSH timer (DosTimer2)
static  PSZ    FLUSHSEM     = "\\SEM32\\SYSLDMP\\FLUSHTIMER";
static  HEV    hevFLUSHSEM  = 0;
static  ULONG  FLUSHSEMCt   = 0;
static  HTIMER hFLUSHEvent  = 0;


/********** Program Structures **********/

struct  STATS       stats;             // struct holds messages received/dropped
struct  PROGRAMOPTS *prgoptions;       // pointer to struct holds program options
struct  CURRENTMSG currentmsg;

extern  HPIPE       SyslogPipe;
static  int  UseNameService;           // make domain name queries

PSZ szShareMem = SHMEM_NAME;           // Name of shared memory.
SHAREMEM  *pShareMem;                  // will point to start (base) of SMEM

char  LocalHostName[MAXHOSTNAMELEN+1]; // our hostname
char  timebuffer[15];


/*
 *  main( ) Start ****************************************************
 */
int main(int argc,char *argv[])
{
    int  i;
    int  x;
    int  ch;

    // intialize the program options and parse the syslog.conf file
    prgoptions = (struct  PROGRAMOPTS *)malloc(sizeof(struct  PROGRAMOPTS));

    initialize(prgoptions);

    // Get program arguments using getopt()
    // Any command line options will override defaults and options found in
    // syslog.conf
    while ((ch = getopt(argc, argv, "c:df:hkm:nprstuz:")) != -1) {
        switch (ch) {

            case 'c':  prgoptions->truncatemode++;
                       prgoptions->truncatesize = (ULONG)atoi(optarg);
                       if(prgoptions->truncatesize < 10000) prgoptions->truncatesize = 10000;
                       break;

            case 'd':  prgoptions->DEBUG++;
                       break;

            case 'f':  strcpy(prgoptions->cfgfilefull, optarg);
                       break;

            case 'h':  usage( );
                       exit(0);
                       break;

            case 'k':  prgoptions->klog++;
                       break;

            case 'm':  prgoptions->marktimer = (long)atoi(optarg);  // mark interval
                       break;

            case 'n':  UseNameService = 0;       // turn off DNS queries
                       break;

            case 'p':  prgoptions->logpriority++; // add msg priority to msg logging
                       break;

            case 'r':  prgoptions->norepeat++;    // disable "repeated" compression
                       break;

            case 's':  prgoptions->securemode++;  // no network listen mode */
                       break;

            case 't':  prgoptions->forwardmode++;
                       break;

            case 'u':  prgoptions->highmemmode++;
                       break;

            case 'z':  strlcpy(prgoptions->bindto, optarg, 15);  // specific NIC to bind
                       break;

            default:   usage( );
                       exit(0);
                       break;
        }

    }
    if ((argc -= optind) != 0) {
        usage( );
        exit(1);
    }
    // done with args

    // allocate shared memory and start IPC and TCP thread
    (void)startup( );

    // setup signals
    (void)signal(SIGTERM,  sysshutdown);
    (void)signal(SIGINT,   sysshutdown);
    (void)signal(SIGBREAK, sysshutdown);
    (void)signal(SIGABRT,  sysshutdown);

    for(x = 0; x < 1024; x++) currentmsg.orgmsg[x] = 0;
    currentmsg.orglen = 0;

    // setup mark timer if prgoptions->marktimer > 0 (0 diabled)
    // default 20 minutes DosTimer1
    if(prgoptions->marktimer) {
        if(prgoptions->DEBUG) if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_4, LOG_DEBUG);
        DosStartTimer(((ULONG)prgoptions->marktimer * 60L * 1000L),
                      (HSEM)hevMARKSEM, &hMARKEvent);
    }

    // setup flush timer DosTimer2 - start at 2 minutes to get initial trucate
    // if prgoptions->truncatemode is true then reset timer to alarm 60 minute interval
    if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_6, LOG_DEBUG);
    DosStartTimer((2L * 60L * 1000L), (HSEM)hevFLUSHSEM, &hFLUSHEvent);   // 30L

    // get klog options
    if(prgoptions->klog) getpopcfg(prgoptions);

    prgoptions->initialized = 1;

    // if klog option then do initial processing of the system log files
    // and log them
    if(prgoptions->klog) poplog(prgoptions);

    prgoptions->idlecount = 0;

    // send first message, internal that syslogd restarted
    logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", ADDDATE | ADDHOST);

    // working loop *************************************************
    // syslogd execute from here to end until exit
    while(1) {
        // sleep here to keep load off system - does it work?
        //DosSleep(900);

        // v2.01 replaced with wait SEM for 1 minute. SEM triggered by IPC or UDP
        // threads (msg placed in buffer) or PIPE sends shutdown
        if(DosWaitEventSem(hevIDLESEM, (prgoptions->idlemulti * 60000)) != ERROR_TIMEOUT) {
            prgoptions->idlemulti = 1;
            DosResetEventSem(hevIDLESEM, &IDLESEMCt);
        } else {
            //if(prgoptions->DEBUG) logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: idle timeout", ADDDATE | ADDHOST);
            if(prgoptions->idlecount<31) ++prgoptions->idlecount;
            if((prgoptions->idlecount == 30) && !prgoptions->marktimer) {
                prgoptions->idlemulti = 30;
                if(prgoptions->DEBUG) logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: idle timeout bump", ADDDATE | ADDHOST);
            }
        }

        // set MEMMUX and wait turn to access the buffer
        if(DosRequestMutexSem(mtxMEMMUX, 60000L) != 0)
                          handleinternalmsg(RUNNINGERR_1, LOG_ERR);
        else {
            // scan the buffer for messages
            for(i = 0; i < (BUFFER_SLOTS); i++) {
                // if slot marked as full ==> process
                if(pShareMem->slotstat[i]) {
                    stats.sysdmprecvmsg++;  // number of msgs received
                    printline(LocalHostName, pShareMem->data[i]);
                    pShareMem->slotstat[i] = 0;
                    for(x = 0; x < 1024; x++) pShareMem->data[i][x] = 0;
                }
            }
            // release the MUX
            DosReleaseMutexSem(mtxMEMMUX);
        }

        // Query for command shutdown
        DosQueryEventSem(hevPIPESEM2, &PIPESEM2Ct);
        if(PIPESEM2Ct) sysshutdown(0);

        // Query test signal
        DosQueryEventSem(hevPIPESEM3, &PIPESEM3Ct);
        if(PIPESEM3Ct) {
            DosResetEventSem(hevPIPESEM3, &PIPESEM3Ct);

            logmsg(LOG_SYSLOG|LOG_DEBUG, "syslogd: Main received BINGO", ADDDATE | ADDHOST);

            sendIPCtest( ); // send IPC test

            if(prgoptions->securemode) sendUDPtest( ); //send UDP test
        }

        // Query TCP thread for blocked
        DosQueryEventSem(hevTCPSEM1, &TCPSEM1Ct);
        if(TCPSEM1Ct) {
            printf("TCP THREAD BLOCKED\n");
            DosResetEventSem(hevTCPSEM1, &TCPSEM1Ct);
        }  //$* endof Query TCP thread

        // Replaces the unix mark timer if -m is 0 disable MARK
        // check mark timer, if time then domark and restart timer
        if(prgoptions->marktimer) {
            DosQueryEventSem(hevMARKSEM, &MARKSEMCt);
            if(MARKSEMCt) {
                DosStopTimer(hMARKEvent);
                domark( 1 );
                DosResetEventSem(hevMARKSEM, &MARKSEMCt);
                DosStartTimer(((ULONG)prgoptions->marktimer * 60L * 1000L),
                              (HSEM)hevMARKSEM, &hMARKEvent);
            }

        }  //$* end of check mark timer

        // 30 minute timer to flush, check popuplog.os2, and truncate if enabled
        DosQueryEventSem(hevFLUSHSEM, &FLUSHSEMCt);
        if(FLUSHSEMCt) {
            if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_7, LOG_DEBUG);
            DosStopTimer(hFLUSHEvent);

            // flush without MARK
            domark( 0 );

            // if klog option then process the system log files and log them
            if(prgoptions->klog) poplog(prgoptions);

            // if prgoptions->truncatemode then truncate logs
            if(prgoptions->truncatemode) sendTrunc( );

            DosResetEventSem(hevFLUSHSEM, &FLUSHSEMCt);
            DosStartTimer((30L * 60L * 1000L), (HSEM)hevFLUSHSEM, &hFLUSHEvent);

        }  //$* end of flush timer

    }  //$* end of working loop

    return 0; // make OW happy

}  //$* end of main


// usage( )
//
//  Display usage to console
//
void usage(void)
{
    printf("\nSyslogd/2 v%s for OS/2-ECS  %s\n", SYSLVERSION, __DATE__);
    printf("Compiled with OpenWatcom %d.%d Michael Greene <greenemk@cox.net>\n\n",
                                                                   OWMAJOR, OWMINOR);
    printf("usage: syslogd [-d][-c size][-h][-k][-m mark_interval][-p][-r][-s][-t]\n\n");
    printf("   -c #    truncate the log files when size reaches #. The min log file\n");
    printf("           size is 10000 bytes. [disabled by default]\n");
    printf("   -d      turn on debug information.\n");
    printf("   -f XXX  Alternate config file and path.\n");
    printf("   -k      klog - process popuplog.os2 with FAC 0 and SEV 2. Initial\n");
    printf("           check of popuplog.os2 at syslogd start and check for new\n");
    printf("           entries every 30 minutes\n");
    printf("   -n      turn off DNS queries\n");
    printf("   -m #    mark interval, where X is the interval (in minutes) a mark\n");
    printf("           event is written to log files [disabled by default]\n");
    printf("   -p      priority will be logged in file and console\n");
    printf("   -r      disable message repeating\n");
    printf("   -s      enable listen on port 514 for UDP messages\n");
    printf("   -t      enable message forwarding as configured in syslog.conf\n");
    printf("   -u      place message buffer in high memory\n");
    printf("   -z XXX  Bind to adapter XXX (ex: lan0)\n");
}  //$* end of usage


// handleinternalmsg( )
//
//  Message numbers passed are pulled from stringtable and send to stderr.
//  If DEBUG - FALSE and message number > 199 just return.
//
void handleinternalmsg(int msgnum, int sev)
{
    char    StringBuf[256];
    HAB     hab = NULL;

    if(!prgoptions->DEBUG && msgnum > 199) return;
    (void)WinLoadString(hab, NULLHANDLE, msgnum, sizeof(StringBuf), StringBuf);
    strcat(StringBuf, "\n");
    logerror(sev, StringBuf); // toss my error message off to the BSD module

}  //$* end of handleinternalmsg


// startup()
//
//  Handles syslogd startup. Allocate shared memory, clear it, and setup
//  MUXSEM to control access to the memory buffer.  Setup the named pipe.
//  Start the threads based on value of threadicp and threadtcp.
//
int startup( void )
{
    int flags;
    int ipcthread;
    int tcpthread;
    int pipethread;


    prgoptions->initialized = 0;  // MKG: 101505 Why here ?

    if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_3, LOG_DEBUG);

    // get local machine hostname
    (void)gethostname(LocalHostName, sizeof(LocalHostName));
    LocalHostName[sizeof(LocalHostName) - 1] = '\0';
    if(prgoptions->DEBUG) logerror(LOG_DEBUG, "SYSLOGDMP: Hostname: %s\n", LocalHostName);

    // allocate shared memory for passing messages from IPC and TCP messages
    // and setup SEM to access
    //
    // pShareMem -pointer to start of allocated memory
    // SHMEM_NAME - shared mem name and SMEM_SIZE - size to allocate
    flags =  (PAG_COMMIT | PAG_READ | PAG_WRITE);

    /* This might work or might not - if -u option then create buffer
     *  in HIGHMEM
     */
    if(prgoptions->highmemmode) {
        flags |= OBJ_ANY;
    }

    if(DosAllocSharedMem((PPVOID)&pShareMem, SHMEM_NAME, SMEM_SIZE, flags) != 0) {
        handleinternalmsg(STARTERROR_3, LOG_ERR);
        return(STARTERROR_3);
    }
    if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_5, LOG_DEBUG);

    /* clear memory chunk */
    memset(pShareMem, '\0', SMEM_SIZE);

    // create the muxsem
    if(DosCreateMutexSem(MEMMUX, &mtxMEMMUX, DC_SEM_SHARED, MUXSTATE)) {
        handleinternalmsg(STARTERROR_10, LOG_ERR);
        return(STARTERROR_10);
    }

    // DosTimer1
    // MARK timer - will output a MARK to configured logs in BSD like
    // manner
    if(DosCreateEventSem(MARKSEM, &hevMARKSEM, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_13, LOG_ERR);
        return(STARTERROR_13);
    }

    // DosTimer2
    // FLUSH, KLOG, TRUNCATE timer
    if(DosCreateEventSem(FLUSHSEM, &hevFLUSHSEM, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_14, LOG_ERR);
        return(STARTERROR_14);
    }

    // ***** IPC Thread - setup and start *****
    //
    // Function to setup and start ICP thread.  Create SEM1/SEM2,
    // start thread, and wait for thread to set SEM1 signaling thread
    // running. SEM1 reset on exit/return
    if(DosCreateEventSem(IPCSEM1, &hevIPCSEM1, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_4, LOG_ERR);
        return(STARTERROR_5);
    }

    if(DosCreateEventSem(IPCSEM2, &hevIPCSEM2, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_5, LOG_ERR);
        return(STARTERROR_5);
    }

    // start the IPC thread
    ipcthread = _beginthread(syslogdipc, NULL, (20*1024), NULL);

    // wait for IPC thread signal that it is started, reset and update IPCSEM1Ct
    if(DosWaitEventSem(hevIPCSEM1, 5000)) {
        handleinternalmsg(STARTERROR_6, LOG_ERR);
        raise(SIGTERM);
    } else handleinternalmsg(STARTMSG_1, LOG_DEBUG);

    DosResetEventSem(hevIPCSEM1, &IPCSEM1Ct);

    // ***** UDP Thread - setup and start *****
    //
    // Function to setup and start TCP thread.  Create SEM1/SEM2,
    // start thread, and wait for thread to set SEM1 signaling thread
    // running. SEM1 reset on exit/return
    //
    //  *** IF initialize( ) finds no interface- don't start UDP thread ***
    //
    if(!prgoptions->noiffound) {
        if(DosCreateEventSem(TCPSEM1, &hevTCPSEM1, DC_SEM_SHARED, FALSE)) {
            handleinternalmsg(STARTERROR_7, LOG_ERR);
            return(STARTERROR_7);
        }

        if(DosCreateEventSem(TCPSEM2, &hevTCPSEM2, DC_SEM_SHARED, FALSE)) {
            handleinternalmsg(STARTERROR_8, LOG_ERR);
            return(STARTERROR_8);
        }

        // start the TCP thread
        tcpthread = _beginthread(syslogdtcp, NULL, (20*1024), NULL);

        // wait for TCP thread signal that it is started, reset and update IPCSEM1Ct
        if(DosWaitEventSem(hevTCPSEM1, 5000)) handleinternalmsg(STARTERROR_9, LOG_ERR);
        else if(prgoptions->DEBUG) handleinternalmsg(STARTMSG_2, LOG_DEBUG);

        DosResetEventSem(hevTCPSEM1, &TCPSEM1Ct);
    }

    // ***** PIPE Thread - setup and start *****
    //
    // Function to setup and start PIPE thread.  Create SEM1/SEM2/SEM3,
    // start thread, and wait for thread to set SEM1 signaling thread
    // running. SEM1 reset on exit/return
    if(DosCreateEventSem(PIPESEM1, &hevPIPESEM1, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_15, LOG_ERR);
        return(STARTERROR_15);
    }

    if(DosCreateEventSem(PIPESEM2, &hevPIPESEM2, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_16, LOG_ERR);
        return(STARTERROR_16);
    }

    if(DosCreateEventSem(PIPESEM3, &hevPIPESEM3, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_16, LOG_ERR);
        return(STARTERROR_16);
    }

    // start the PIPE thread
    pipethread = _beginthread(syslogdpipe, NULL, (20*1024), NULL);

    // idle main process, replaces DosSleep in 2.01
    if(DosCreateEventSem(IDLESEM, &hevIDLESEM, DC_SEM_SHARED, FALSE)) {
        handleinternalmsg(STARTERROR_18, LOG_ERR);
        return(STARTERROR_18);
    }

    // wait for PIPE thread signal that it is started, reset and update IPCSEM1Ct
    if(DosWaitEventSem(hevPIPESEM1, 5000)) handleinternalmsg(STARTERROR_17, LOG_ERR);
    else if(prgoptions->DEBUG) handleinternalmsg(STARTMSG_3, LOG_DEBUG);

    DosResetEventSem(hevPIPESEM1, &PIPESEM1Ct);

    return(0);

}  //$* end of startup


// shutdown( )
//
//  Full shutdown threads, named pipe, kill all the SEMs, and
//  free shared memory.
//
void sysshutdown(int signo)
{
    // send shutdown message, internal that syslogd stopping
    logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: stopping", ADDDATE | ADDHOST);

    // Received ctrl+c so signal pipe thread shutdown
    if(signo == 4) {
        HPIPE  SDSyslogPipe;
        ULONG  ulAction;
        ULONG  bytes_written;
        char   buf_data[ ] = "PIPESHUTDOWN";

        DosOpen(PIPE_NAME, (PHFILE)&SDSyslogPipe, &ulAction, 0L, 0L,
                            SC_PIPE_OPEN , SC_PIPE_MODE, NULL);
        DosWrite(SDSyslogPipe, buf_data, sizeof(buf_data), &bytes_written);
        DosClose(SDSyslogPipe);
    }

    prgoptions->initialized = 0;
    free(prgoptions);

    // post IPCSEM2 IPC thread shutdown
    DosPostEventSem(hevIPCSEM2);
    if(DosWaitEventSem(hevIPCSEM1, 10000)) if(prgoptions->DEBUG)
                                            handleinternalmsg(SHUTDOWNMSG_1, LOG_ERR);
    else if(prgoptions->DEBUG) handleinternalmsg(SHUTDOWNMSG_2, LOG_DEBUG);

    if(!prgoptions->noiffound) {
        // post IPCSEM2 UDP thread shutdown
        DosPostEventSem(hevTCPSEM2);
        if(DosWaitEventSem(hevTCPSEM1, 10000)) if(prgoptions->DEBUG)
                                            handleinternalmsg(SHUTDOWNMSG_3, LOG_ERR);
        else if(prgoptions->DEBUG) handleinternalmsg(SHUTDOWNMSG_4, LOG_DEBUG);
    }

    // check Pipe shutdown
    if(DosWaitEventSem(hevPIPESEM1, 10000)) if(prgoptions->DEBUG)
                                             handleinternalmsg(SHUTDOWNMSG_5, LOG_ERR);
    else if(prgoptions->DEBUG) handleinternalmsg(SHUTDOWNMSG_6, LOG_DEBUG);

    // kill the SEMs
    DosCloseEventSem(hevIPCSEM1);
    DosCloseEventSem(hevIPCSEM2);
    DosCloseEventSem(hevTCPSEM1);
    DosCloseEventSem(hevTCPSEM2);
    DosCloseEventSem(hevPIPESEM1);
    DosCloseEventSem(hevPIPESEM2);
    DosCloseEventSem(hevPIPESEM3);
    DosCloseMutexSem(mtxMEMMUX);

    // free the shared memory
    DosFreeMem(pShareMem);

    exit(signo);

}  //$* end of shutdown


/*  gettimestamp( )
 *
 *  return the date / time RFC3164 format
 */
char *gettimestamp( void )
{
    // TIMESTAMP format "Mmm dd hh:mm:ss"
    time_t TimeOfDay;
    char   *p;

    TimeOfDay = time( NULL );

    strftime(timebuffer, 15, "%b %d %T", localtime(&TimeOfDay));  // timestamp
    p = timebuffer;

    return(p);

}  //$* end  of gettimestamp


/*  sendIPCtest( )
 *
 *  Opens IPC connection and sends test message
 */
void sendIPCtest( void )
{
    char   TestMsg[ ] = "<47>syslogd: IPC  received BINGO";
    int    testsock   = -1;
    struct sockaddr_un testIPC;

    memset(&testIPC, 0, sizeof(testIPC));

    testIPC.sun_family = AF_UNIX;
    strcpy(testIPC.sun_path, "\\socket\\syslog");

    testsock = socket(AF_UNIX, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &testIPC, sizeof(testIPC));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);

}  //$* end of sendIPCtest


/*  sendUDPtest( )
 *
 *  Opens UDP connection and sends test message
 */
void sendUDPtest( void )
{
    char TestMsg[ ] = "<47>syslogd: UDP  received BINGO";
    int  testsock   = -1;
    struct sockaddr_in testUDP;

    memset(&testUDP, 0, sizeof(testUDP));

    testUDP.sin_len         = sizeof(testUDP);
    testUDP.sin_family      = AF_INET;
    testUDP.sin_addr.s_addr = htonl(gethostid(  )); // inet_addr("192.168.1.2");
    testUDP.sin_port        = htons(514);

    testsock = socket(AF_INET, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &testUDP, sizeof(testUDP));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);

}  //$* end of sendUDPtest


/*  sendTrunc( )
 *
 *  Will send command to pipe for log truncate
 */
void sendTrunc(void)
{
    HPIPE  TRSyslogPipe;
    ULONG  ulAction;
    ULONG  bytes_written;
    char   autotrunc[81];

    strcpy(autotrunc, "AUTOTRUNC");

    if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_10, LOG_DEBUG);

    DosOpen(PIPE_NAME, (PHFILE)&TRSyslogPipe, &ulAction, 0L, 0L,
                        SC_PIPE_OPEN , SC_PIPE_MODE, NULL);
    DosWrite(TRSyslogPipe, autotrunc, sizeof(autotrunc), &bytes_written);

    DosClose(TRSyslogPipe);

}  //$* end of sendTrunc


