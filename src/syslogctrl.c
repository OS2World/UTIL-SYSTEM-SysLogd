/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 1.2       Michael K Greene <greenemk@cox.net>
 *                      October 2005
 *
 *            SYSLOGD/2 compiles with OpenWatom 1.4 (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  Patch together syslogctrl from cron at.exe source. This
 *               executable is used to send commands to syslogd named pipe.
 *
 * Changes:
 *   1.0  Initial working version using OW13 (Summer 2004)
 *   1.1  Clean up and use OW14 getopt and other provided calls
 *   1.2  Add rotate and truncate commands, convert to C source
 *
 ***************************************************************************/

#define  INCL_DOSFILEMGR
#define  INCL_DOSNMPIPES
#define  INCL_WINWINDOWMGR

#define  Message_1     1
#define  Message_2     2
#define  Message_3     3
#define  Message_4     4
#define  Message_5     5

#include  <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <version.h>

#define  PIPE_NAME       "\\PIPE\\SYSLOGD"
#define  SC_PIPE_MODE    OPEN_ACCESS_READWRITE    |  \
                         OPEN_SHARE_DENYREADWRITE |  \
                         OPEN_FLAGS_FAIL_ON_ERROR
#define  SC_PIPE_OPEN    OPEN_ACTION_OPEN_IF_EXISTS

HPIPE  SyslogPipe;  // handle to pipe start requests are made through

struct PROGRAMOPTS {
    unsigned  int   DEBUG;
    unsigned  int   klog;
    unsigned  int   norepeat;
    unsigned  int   initialized;
    unsigned  long  marktimer;
    unsigned  long  logpriority;
    unsigned  int   securemode;
    unsigned  int   forwardmode;
    unsigned  int   truncatemode;
    long  truncatesize;
    char  cfgfilename[12];
    char  cfgfilefull[_MAX_PATH];
    char  popupfile[20];
    char  ft2exfile[20];
    char  sendIP[15];
    char  sendIF[15];
    char  bindto[15];
    char  flashlog[_MAX_PATH];
    bool  threadipc;    // flag to indicate IPC thread start/running
    bool  threadtcp;    // flag to indicate TCP thread start/running
};

void  usage(void);
void  handleinternalmsg(int msgnum);
int   open_pipe( void );
void  dumpstats( void );
void  dumpconf( void );
void  dumpopts( void );
void  dumpreap( void );
void  sendtest( void );
void  sendrotate( void );
void  sendtrunc(int truncsize);
void  send_shutdown(void);

ULONG bytes_read;
ULONG bytes_written;
char  buf_data[81];


// main() ***********************************************************
//
int main(int argc, char *argv[])
{
    int ch = 0;
    int truncsize;

    // if no args display usage and exit
    if(argc == 1)
    {
        usage( );
        exit(0);
    }

    // get args
    while ((ch = getopt(argc, argv, "hbrst:d:")) != -1)
    {
        switch (ch)
        {
            case 'h': usage( );
                      exit(0);
                      break;

            case 'b': sendtest( );
                      exit(0);
                      break;

            case 'r': sendrotate( );
                      exit(0);
                      break;

            case 't': truncsize = atoi(optarg);
                      if(truncsize < 1000) truncsize = 1000;
                      sendtrunc(truncsize);
                      exit(0);
                      break;

            case 's': send_shutdown( );
                      exit(0);
                      break;

            case 'd': if(stricmp(optarg, "stats") == 0) dumpstats( );
                      else if(stricmp(optarg, "conf") == 0) dumpconf( );
                           else if(stricmp(optarg, "reap") == 0) dumpreap( );
                                else
                                {
                                    printf("Error: Bad -d option\n");
                                    usage( );
                                }
                      exit(0);
                      break;

            default:  handleinternalmsg(Message_1);
                      break;
        }
    }

    handleinternalmsg(Message_1);  // If prg gets here then options were totally crap

    return(0);

}  //$* End of Main() ****


// open_pipe ()
//
//  The DosWaitNPipe was wrong in original - wait up to 5 seconds for busy pipe.
//
int open_pipe(void)
{
    ULONG   ulAction;
    int     statflag   = 0;
    int     error_stat = 0;

    while(DosOpen( PIPE_NAME, (PHFILE)&SyslogPipe,
                   &ulAction, 0L, 0L,
                   SC_PIPE_OPEN ,
                   SC_PIPE_MODE, NULL))
    {
        printf("%d\n", ulAction);
        if(DosWaitNPipe(PIPE_NAME, 1000)) ++error_stat;
        if(error_stat == 10)
        {
            statflag = 1;
            break;
        }
    }

    return(statflag);

}  //$* end of open_pipe


// usage( )
//
//  Display usage to console
//
void usage(void)
{
    printf("\nsyslogctrl v%s for OS/2-ECS  %s\n", SYSLVERSION, __DATE__);
    printf("Compiled with OpenWatcom %d.%d Michael Greene <greenemk@cox.net>\n",
                                                                   OWMAJOR, OWMINOR);
    printf("syslogctrl is a utilitity for use with Syslogd/2.\n\n");
    printf("syslogctrl [-b] [-d stats | conf] [-r] [-s] [-t #]\n\n");
    printf("-b   start syslogd test sequence\n");
    printf("-d stats | conf\n");
    printf("     stats - return the message receive and processed stats\n");
    printf("     conf  - return the current syslogd message filtering as read\n");
    printf("             from current syslog.conf\n");
    printf("-r   rotate all log files\n");
    printf("-s   send syslogd shutdown signal\n");
    printf("-t # truncate the log files when size reaches #. The min log file\n");
    printf("     size is 10000 bytes.\n");
}


// handleinternalmsg( )
//
//  Message numbers passed are pulled from stringtable and send to stderr.
//  If DEBUG - FALSE and message number > 199 just return.
//
void handleinternalmsg(int msgnum)
{
    char    StringBuf[81];
    HAB     hab = 0;   // assign value to shutup compiler

    (void)WinLoadString(hab, NULLHANDLE, msgnum, sizeof(StringBuf), StringBuf);
    printf("\n%s\n", StringBuf);

    if(msgnum < 4)
    {
        handleinternalmsg(Message_5);
        exit(msgnum);
    }

}  //$* end of handleinternalmsg


// dumpstats ( )
//
//  Open the named pipe and send "stats" command. Received data until
//  receive "DONE".
//
//  Provides a dump of messages syslogd has received and processed.
//
void dumpstats( )
{

    if(open_pipe( )) handleinternalmsg(Message_2);

    strcpy(buf_data,"PIPESTATSEND");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
    {
        DosClose(SyslogPipe);
        handleinternalmsg(Message_3);
    }

    printf("\nCurrent syslogd message stats:\n\n");

    while(TRUE)
    {

        if(DosRead(SyslogPipe, buf_data, sizeof(buf_data), &bytes_read))
        {
            handleinternalmsg(Message_4);
            break;
        }

        if(strcmp(buf_data, "DONE") == 0) break;
        printf("%s\n", buf_data);
    }

    DosClose(SyslogPipe);

}  //$* end of dumpstats


void dumpreap( )
{

    if(open_pipe( )) handleinternalmsg(Message_2);

    strcpy(buf_data, "PIPEREAPSEND");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
    {
        DosClose(SyslogPipe);
        handleinternalmsg(Message_3);
    }

    while(TRUE)
    {
        if(DosRead(SyslogPipe, buf_data, sizeof(buf_data), &bytes_read))
        {
            handleinternalmsg(Message_4);
            break;
        }

        if(strncmp(buf_data, "DONE", 4) == 0) break;

        printf("%s\n", buf_data);
    }

    strcpy(buf_data, "OK");
    DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written);

    DosClose(SyslogPipe);

}  //$* end of dumpconf


// dumpconf ()
//
//  Open the named pipe and send "conf" command. Received data until
//  receive "DONE".
//
//  Provides a formatted dump of configuration syslogd is using.
//
void dumpconf( )
{
    if(open_pipe( )) handleinternalmsg(Message_2);

    strcpy(buf_data, "PIPECONFSEND");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
    {
        DosClose(SyslogPipe);
        handleinternalmsg(Message_3);
    }

    printf("\nCurrent syslog.conf filtering:\n");
    printf("FAC ==>             1 1 1 1 1 1 1 1 1 1 2 2 2 2 2\n");
    printf("0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4\n");
    printf("-------------------------------------------------\n");

    while(TRUE)
    {
        if(DosRead(SyslogPipe, buf_data, sizeof(buf_data), &bytes_read))
        {
            handleinternalmsg(Message_4);
            break;
        }

        if(strncmp(buf_data, "DONE", 4) == 0) break;
        printf("%s\n", buf_data);
    }

    strcpy(buf_data, "OK");
    DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written);
    DosClose(SyslogPipe);

}  //$* end of dumpconf


// send_shutdown( )
//
//  Open the named pipe and send shutdown command.
//
void send_shutdown( void )
{
    if(open_pipe()) handleinternalmsg(Message_2);

    strcpy(buf_data,"PIPESHUTDOWN");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
                                       handleinternalmsg(Message_3);
    DosClose(SyslogPipe);

}  //$* end of send_shutdown


// sendrotate( )
//
//  Send rotate command to syslogd
//
void sendrotate( )
{
    if(open_pipe( )) handleinternalmsg(Message_2);

    strcpy(buf_data, "ROTATE");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
                                       handleinternalmsg(Message_3);
    DosClose(SyslogPipe);

}  //$* end of sendrotate


// sendtrunc( )
//
//  Send truncate logfiles to syslogd
//
void sendtrunc(int truncsize)
{
    if(open_pipe()) handleinternalmsg(Message_2);

    strcpy(buf_data, "TRUNCATE");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
                                       handleinternalmsg(Message_3);

    while(TRUE)
    {
        if(DosRead(SyslogPipe, buf_data, sizeof(buf_data), &bytes_read))
        {
            handleinternalmsg(Message_4);
            break;
        }

        if(strncmp(buf_data, "OK", 2) == 0) break;
    }

    itoa(truncsize, buf_data, 10);

    DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written);

    DosClose(SyslogPipe);

}  //$* end of sendtrunc


void sendtest( )
{
    if(open_pipe( )) handleinternalmsg(Message_2);
    strcpy(buf_data, "BINGO");

    if (DosWrite(SyslogPipe, buf_data, sizeof(buf_data), &bytes_written))
                                       handleinternalmsg(Message_3);

    DosRead(SyslogPipe, buf_data, sizeof(buf_data), &bytes_read);
    printf("Ack %s\n", buf_data);
    DosClose(SyslogPipe);

}


void dumpprgopts(struct  PROGRAMOPTS *prgoptions)
{
    printf("SYSLOGD Running Options:\n\n");
    printf("send IP      %s\n", prgoptions->sendIP);
    printf("send IF      %s\n", prgoptions->sendIF);
    printf("debug        %s\n", prgoptions->DEBUG?"ON":"OFF");
    printf("klog         %s\n", prgoptions->klog?"ON":"OFF");
    printf("norepeat     %s\n", prgoptions->norepeat?"ON":"OFF");
    printf("initialized  %s\n", prgoptions->initialized?"ON":"OFF");
    printf("logpriority  %s\n", prgoptions->logpriority?"ON":"OFF");
    printf("securemode   %s\n", prgoptions->securemode?"ON":"OFF");
    printf("forwardmode  %s\n", prgoptions->forwardmode?"ON":"OFF");
    printf("norepeat     %s\n", prgoptions->norepeat?"ON":"OFF");
    printf("truncatemode %s",   prgoptions->truncatemode?"ON":"OFF");
    if(prgoptions->truncatemode) printf("  size  %d\n", prgoptions->truncatesize);
    else printf("\n");
    printf("cfgfilefull  %s\n", prgoptions->cfgfilefull);
    printf("popupfile    %s\n", prgoptions->popupfile);
    printf("flashfile    %s\n", prgoptions->flashlog);
    printf("threadipc    %s\n", prgoptions->threadipc?"ON":"OFF");
    printf("threadtcp    %s\n", prgoptions->threadtcp?"ON":"OFF");
    printf("marktimer    %s  ", prgoptions->marktimer?"ON":"OFF");
    if(prgoptions->marktimer) printf("  time  %d\n", prgoptions->marktimer);
    else printf("\n");

}

