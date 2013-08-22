/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 2.0      Michael K Greene <greenemk@cox.net>
 *                     December 2005
 *
 *
 *
 *       SYSLOGD/2 compiles with OpenWatom 1.4 (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  The syslogd pipe thread module.  Handles syslog control
 *               messages received on named pipe from syslogctrl. Also
 *               contains log rotate and truncate functions
 *
 *
 ***************************************************************************/

#define INCL_DOSPROCESS
#define INCL_DOSNMPIPES
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INTERNAL_NOPRI   0x10    // the "no priority" priority mark "facility"
                                 // needed for conf dump and do not want SYSLOG_NAMES
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <io.h>
#include "syslogmsg.h"
#include "syslogd.h"


void syslogdpipe( PVOID );          // start thread entry

static void shutdownpipe(void);
static void dumpstats( void );
static void dumpconf( void );
static void dumpopts( void );
static void rotatelogs( void );
static void trunclogs(long truncsize);

extern struct filed        *HeadFiles;  // linked list containing logging filters
extern struct PROGRAMOPTS  *prgoptions; // program options
extern struct STATS        stats;       // holds messages received/dropped

// Shared memory buffer defined in syslogd.cpp
extern SHAREMEM  *pShareMem;
extern PSZ       szShareMem;
extern PSZ       MEMMUX;      // MUX to signal access to shared memory buffer
extern HMTX      mtxMEMMUX;

// SEMAPHOREs define in syslogd main
extern PSZ   PIPESEM1;
extern HEV   hevPIPESEM1;
extern ULONG PIPESEM1Ct;

extern HEV   hevPIPESEM2;
extern HEV   hevPIPESEM2;
extern ULONG PIPESEM2Ct;

extern PSZ   PIPESEM3;
extern HEV   hevPIPESEM3;
extern ULONG PIPESEM3Ct;

// SEMAPHOREs define in syslogd main
extern  PSZ    IDLESEM;       // SEMAPHORE to stop IDLE
extern  HEV    hevIDLESEM;
extern  ULONG  IDLESEMCt;

static HPIPE SyslogPipe;  // handle to pipe start requests are made through
static ULONG bytes_read;
static ULONG bytes_write;
static char  pipe_buffer[81] = {0};
static char  *TypeNames[4] = { "UNUSED", "FILE", "CONSOLE", "FORW" };
static long  truncsize;


// syslogpipe( )
//
//  Start thread from cron main.  A named pipe is openned and if no error
//  start main loop to read and write AT commands / query.
//
void syslogdpipe(  )
{
    bool firststart;

    // Open the pipe main will timeout if fails
    if(DosCreateNPipe(PIPE_NAME,
                      &SyslogPipe,
                      PIPE_OPEN_MODE,
                      PIPE_MODE,
                      sizeof(pipe_buffer)*3,
                      sizeof(pipe_buffer)*3,
                      PIPE_TIMEOUT))  _endthread();

    firststart = TRUE;   // Prevents pre-intialization wrap shutdown

    // Main working loop
    //
    while(TRUE)
    {
        // Just my play.  Set SEM1 (SYSLOGD is waiting), delay 1 sec
        // (let SYSLOGD reset), and start now FALSE - continue -
        // sleep 1.5 sec for SEM sets or thread will shutdown
        if(firststart) {
            DosPostEventSem(hevPIPESEM1); // tell main I am up!!
            DosSleep(1500);
            firststart = FALSE;
        }

        if(DosConnectNPipe(SyslogPipe)) handleinternalmsg(PIPEERROR_1, LOG_ERR);
        else
        {
            *pipe_buffer=0;

            if(DosRead(SyslogPipe,pipe_buffer, sizeof(pipe_buffer), &bytes_read)) {
                DosDisConnectNPipe(SyslogPipe);
                handleinternalmsg(PIPEERROR_2, LOG_ERR);
            }

            // Receive stats query
            if(strstr(pipe_buffer, "PIPESTATSEND")) dumpstats( );

            // receive conf query
            if(strstr(pipe_buffer, "PIPECONFSEND")) dumpconf( );

            // receive opt query
            if(strstr(pipe_buffer, "PIPEOPTSSEND")) dumpopts( );

            // rotate logs
            if(strstr(pipe_buffer, "ROTATE")) rotatelogs( );

            // manual truncate logs
            if(strstr(pipe_buffer, "TRUNCATE")) {
                if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_12, LOG_DEBUG);
                strcpy(pipe_buffer, "OK");
                if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_5, LOG_ERR);

                if(DosRead(SyslogPipe,pipe_buffer, sizeof(pipe_buffer), &bytes_read)) {
                    DosDisConnectNPipe(SyslogPipe);
                    handleinternalmsg(PIPEERROR_2, LOG_ERR);
                }

                truncsize = (long)atoi(pipe_buffer);

                if(truncsize < 10000) truncsize = 10000;

                trunclogs(truncsize);
            }

            // receive truncate from main
            if(strstr(pipe_buffer, "AUTOTRUNC")) {
                if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_11, LOG_DEBUG);
                trunclogs(prgoptions->truncatesize);
            }

            // receive shutdown from syslogctrl
            if(strstr(pipe_buffer, "PIPESHUTDOWN")) {
                DosPostEventSem(hevPIPESEM2); // signal shutdown
                DosPostEventSem(hevIDLESEM);  // wakeup MAIN if IDE
                shutdownpipe( );              // shutdown pipe thread
            }

            // Test signal to main
            if(strstr(pipe_buffer, "BINGO")) {
                logmsg(LOG_SYSLOG|LOG_DEBUG, "syslogd: Pipe received BINGO", ADDDATE | ADDHOST);

                // send ACK to syslogctrl
                strcpy(pipe_buffer, "BINGO");
                DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write);

                DosPostEventSem(hevPIPESEM3); // signal main for BINGO
                DosPostEventSem(hevIDLESEM);  // wakeup MAIN if IDE
            }

            // Query for timed truncate - reusing SEM1
            DosQueryEventSem(hevPIPESEM1, &PIPESEM1Ct);
            if(PIPESEM1Ct) {
                // cout << "Pipe trunc" << endl;
                trunclogs(prgoptions->truncatesize);
                DosResetEventSem(hevPIPESEM1, &PIPESEM1Ct);
            }
        }

        if(DosDisConnectNPipe(SyslogPipe)) handleinternalmsg(PIPEERROR_3, LOG_ERR);
    }

}  //$* end of syslogpipe( )


/* shutdowpipe()
 *
 *  This function is used by SYSLOGPIPE process is being
 *  shut down.  We need to close the pipe.
 */
void shutdownpipe(void)
{
    DosClose(SyslogPipe);
    DosPostEventSem(hevPIPESEM1); // signal syslogd main thread shutdown
    DosPostEventSem(hevIDLESEM);  // wakeup MAIN if IDE
    _endthread( );

}  //$* end of shutdown_cronat


/* dumpcstats ( )
 *
 */
void dumpstats( void )
{
    sprintf(pipe_buffer, "Thread messages received  - UDP: %d  IPC: %d  Total: %d",
                          stats.systcprecvmsg, stats.sysipcrecvmsg,
                          (stats.systcprecvmsg + stats.sysipcrecvmsg));
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    sprintf(pipe_buffer, "Messages received by main - %d", stats.sysdmprecvmsg);
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    sprintf(pipe_buffer, "Messages processed -        %d \n", stats.bsdprocessmsg);
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    sprintf(pipe_buffer, "Thread overflows -          UDP: %d  IPC: %d  Total: %d",
                          stats.tcpoverflows, stats.ipcoverflows,
                          (stats.tcpoverflows + stats.ipcoverflows));
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    sprintf(pipe_buffer, "Mark messages processed -   %d", stats.markmsgs);
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    strcpy(pipe_buffer, "DONE");
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    // Just wait here until syslogctrl finish and sends OK
    DosRead(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_read);

}  //$* end of statdump


/* dumpconf ( )
 *
 *  Dumps the current program logging configuration loaded from syslog.conf
 *  original routine was located in init( ) Removed to an independant function
 *  so can query via named pipe
 */
void dumpconf( void )
{
    int  i;

    char   tmp_buffer[2];
    char   msg_buffer[81] = {0};

    struct filed *f;

    for (f = HeadFiles; f; f = f->f_next) {
        for (i = 0; i <= LOG_NFACILITIES; i++)
                         if (f->f_pmask[i] == INTERNAL_NOPRI) strcat(msg_buffer, "X ");
                         else {
                             _itoa(f->f_pmask[i], tmp_buffer, 10);
                             strcat(msg_buffer, tmp_buffer);
                             strcat(msg_buffer, " ");
                         }

        strcat(msg_buffer, TypeNames[f->f_type]);
        strcat(msg_buffer, ": ");

        switch (f->f_type) {
            case F_FILE:
            case F_CONSOLE: strcat(msg_buffer, f->f_un.f_fname);
                            break;
            case F_FORW:    strcat(msg_buffer, f->f_un.f_forw.f_hname);
                            break;
        }

        strcpy(pipe_buffer, msg_buffer);

        *msg_buffer = 0;

        if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);
    }

    strcpy(pipe_buffer, "DONE");
    if(DosWrite(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_write))
                                             handleinternalmsg(PIPEERROR_4, LOG_ERR);

    // Just wait here until syslogctrl finish and sends OK
    DosRead(SyslogPipe, pipe_buffer, sizeof(pipe_buffer), &bytes_read);

}  //$* end of confdump


void dumpopts( void )
{



}  //$* end of dumpopts


/* rotatelogs( )
 *
 *  Rotate all log files moving to a new file with month, day, year, hhmmss
 *  added to old log name.
 */
void rotatelogs( void )
{
    struct filed *f;
    char full_path[_MAX_PATH];
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR ];
    char fname[_MAX_FNAME ];
    char ext[_MAX_EXT];
    char rotate_ext[12];

    time_t time_of_day;
    time_of_day = time( NULL );

    strftime(rotate_ext, 12,"%m%d%y%H%M%S ",localtime( &time_of_day ));

    // get in line to control the message buffer to prevent write while
    // rotating
    if(DosRequestMutexSem(mtxMEMMUX, 60000L) != 0) {
        printf("SYSLOGPIPE: Timeout waiting for MUX\n");
    } else {
        for (f = HeadFiles; f; f = f->f_next) {
            if(f->f_type == F_FILE) {
                _splitpath(f->f_un.f_fname, drive, dir, fname, ext );
                sprintf(full_path, "%s%s%s", drive, dir, fname);
                strncat(full_path, rotate_ext, 12);
                strcat(full_path, ext);

                (void)rename(f->f_un.f_fname, full_path);
            }
        }
    }

    DosReleaseMutexSem(mtxMEMMUX);

    logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: log files rotated", ADDDATE | ADDHOST);

}  //$* end of rotatelogs


/* trunclogs( )
 *
 *  Truncates log files to a min size of 10000 bytes to user provided
 */
void trunclogs(long truncsize)
{
    int  handle;

    long count;
    long shave;

    char t_buffer[_MAX_PATH];

    FILE *fpi;
    FILE *fpo;

    char buffer[1024];
    char tmpfile[_MAX_PATH];
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];

    struct filed *f;

    // get in line to control the message buffer to prevent write while
    // truncating logs
    if(DosRequestMutexSem(mtxMEMMUX, 60000L) != 0)  handleinternalmsg(PIPEERROR_6, LOG_ERR);
    else {
        for (f = HeadFiles; f; f = f->f_next) {
            if(f->f_type == F_FILE) {
                handle = open(f->f_un.f_fname, O_RDWR | O_TEXT );

                if(handle != -1) {
                    if(filelength(handle) > truncsize) {
                        // shave will be min that needs to be removed
                        shave = filelength(handle) - truncsize;

                        close(handle);

                        // setup temp file
                        _splitpath(f->f_un.f_fname, drive, dir, NULL, NULL);
                        strcpy(tmpfile, drive);
                        strcat(tmpfile, dir);
                        strcat(tmpfile, "trunctmp.log");

                        fpi = fopen(f->f_un.f_fname, "r");
                        fpo = fopen(tmpfile, "w");

                        // read log file to point of trunc
                        for(count=0;count<shave;count+=strlen(buffer)) fgets(buffer,1024,fpi);

                        while(fgets(buffer, 1024, fpi) != NULL) fputs(buffer, fpo);

                        fclose(fpi);
                        fclose(fpo);

                        remove(f->f_un.f_fname);
                        rename(tmpfile, f->f_un.f_fname);

                        strcpy(t_buffer, "syslogd: log files truncated ");
                        strcat(t_buffer, f->f_un.f_fname);
                        logmsg(LOG_SYSLOG|LOG_INFO, t_buffer, ADDDATE | ADDHOST);

                    } else close(handle);
                }
            }
        }
    }

    DosReleaseMutexSem(mtxMEMMUX);

}  //$* end of trunclogs


