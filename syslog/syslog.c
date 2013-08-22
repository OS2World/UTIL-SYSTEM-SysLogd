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
 *         SYSLOGD/2 compiles with OpenWatom 1.4 (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  Main syslogd process. Sets up the program defaults and spawns
 *               threads (IPC, TCPIP, and NAMED PIPE) Reads incomming messages
 *               from the shared buffer and sends to the modified syslogd BSD
 *               module.
 *
 *  1.0 October 2004
 *  2.0 December 2005
 *      Feb 2006        - fix LogTag problem
 *
 ***************************************************************************/
/*
 * Copyright (c) 1983, 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *  ========================================================================
 *
 * Description:  glibc-2.3.3-stable syslog.c modified for use with OS/2 and
 *               OpenWatcom 1.3 or higher.  May use OS/2 API functions with no
 *               intent to remain portable.
 *               This syslog.c is based on: syslog.c 8.4 (Berkeley) 3/18/94
 *
 *               Major change: Will attempt to send message IPC (BSD) and then
 *               TCPIP (OS/2) - works with either syslogd.
 *
 *               Based on original OS/2 work by:
 *               Jochen Friedrich <jochen@audio.pfalz.de>
 *               Mikael St†ldal <d96-mst@nada.kth.se>
 *
 *               Michael K Greene <greenemk@cox.net>  September 2004
 *
 *               January 2005 - 1. modified to allow kern message logging
 *                              2. add additional functions, modified to
 *                                 allow manual send selection by logger.exe
 *                                 (lopenlog and lsyslog)
 *
 ****************************************************************************/

#define SLASHES  '\\'

#include <stdio.h>
#include <stdlib.h>
#include <types.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <process.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <nerrno.h>
#include "syslog.h"

static int  LogType;                // type of socket connection 0-None 1-IPC 2-TCP
static int  LogError;               // openlog error flag 1-IPC 2-TCP
static int  LogFile = -1;           // fd for log
static int  connected;              // have done connect
static int  LogStat = 0;            // status bits, set by openlog()
static int  LogFacility = LOG_USER; // default facility code
static int  LogMask = 0xff;         // mask of priorities to be logged

static const char *LogTag;          // string to tag the entry with

#ifdef __WATCOMC__
char *getprogname( void );          // get program name - internal
#endif


// syslog( )
//
//  Default call into syslog( ), openlog( ) should be called prior
//  to syslog( ) to set different options (LogStat).
//
void syslog(int pri, const char *fmt, ...)
{
    va_list ap;

    va_start(ap,fmt);
    vsyslog(0, pri, fmt, ap);
    va_end(ap);

} //$* end of syslog( )


// lsyslog( )
//
//  call into syslog from logger.exe
//
void lsyslog(int sendtype, int pri, const char *fmt, ...)
{
    va_list ap;

    va_start(ap,fmt);
    vsyslog(sendtype, pri, fmt, ap);
    va_end(ap);

} //$* end of syslog( )


// vsyslog( )
//
//  This function is not part of POSIX.  __USE_BSD must be defined
//  to call directly - see syslog.h
//
void vsyslog(int sendtype, int pri, const char *fmt, va_list ap)
{
    time_t TimeOfDay;

    char work_buf[256]= {0};      // working buffer size dictated by hostname retrieval
    char syslog_buf[1024] = {0};  // Max size of packet 1024 - RFC3164 (4.1)
    char content_buf[1024]= {0};  // buffer the CONTENT prior to appending to packet
    int  allowcontentsize;        // size of PRI, HEADER, and TAG field
    int  bufsize = 0;
    int  sendstat;
    int  nologtag = 0;

#define  INTERNALLOG     LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID   // default 35

    /* Check for invalid bits. */
    // Will error if FAC > 127 : Do not know why (mask 110000000000) resets
    // priority and continues, allows FAC from 0-127 and normal SEV 0-7
    if (pri & ~(LOG_PRIMASK|LOG_FACMASK))
    {
        syslog(INTERNALLOG, "syslog: unknown facility/priority: %x", pri);
        pri &= LOG_PRIMASK|LOG_FACMASK;
    }

    /* Check priority against setlogmask values. */
    if ((LOG_MASK (LOG_PRI (pri)) & LogMask) == 0) return;

    /* Set default facility if none specified. */
    // Changed to allow forwarded Klog messages to pass FAC = 0
    if((pri & 8) != 0) if ((pri & LOG_FACMASK) == 0) pri |= LogFacility;

    // insert PRI into packet buffer
    sprintf(syslog_buf,  "<%d>", pri);

    // HEADER Part (TIMESTAMP and HOSTNAME) RFC 3164 (4.1.2)

    // TIMESTAMP format "Mmm dd hh:mm:ss"
    TimeOfDay = time( NULL );
    strftime(work_buf, 15, "%b %d %T", localtime(&TimeOfDay));  // timestamp
    strncat(syslog_buf,  work_buf, 15);
    strcat(syslog_buf,  " ");

    // HOSTNAME hostname or IP address of the device, no size specs
    // Just using a quick and dirty gethostname( )
    gethostname(work_buf, 256);             // hostname must exist ?
    strcat(syslog_buf, work_buf);
    strcat(syslog_buf, " ");

    // MSG Part (TAG and CONTENT) RFC 3164 (4.1.3)

    // As in the original syslog.c, if no TAG set by calling modules then use
    // excutable program name.
    // TAG field MUST NOT exceed 32 characters
    if (LogTag == NULL)
    {
        // have entered without LogTag set - so remember so LogTag can be
        // set back to NULL - MKG Feb 2006
        nologtag = 1;

        // get the program name and strip off file type,
        // set TagLog strlwr( ) should not matter but might need to be removed
        strncpy(work_buf, __progname, 36);
        LogTag = strlwr(strtok(work_buf, "."));
    }
    if (LogTag != NULL) strncat(syslog_buf, LogTag, 32);

    // reset LogTag
    if (nologtag)
    {
        nologtag = 0;
        LogTag = NULL;
    }

    // MAX Packet size 1024 (RFC3164 (4.1)) subtract PRI, HEADER, and
    // TAG field size is max allowable CONTENT length
    if(LogStat & LOG_PID)
    {
        sprintf(work_buf, "[%d]: ", getpid( ));
        strcat(syslog_buf,  work_buf);
    }
    else strcat(syslog_buf,  ": ");

    // as a note, the ":" or "[" is start of CONTENT
    allowcontentsize = 1024 - (strlen(syslog_buf));

    // Print the user's format into the buffer.
    vsnprintf( content_buf, 1024, fmt, ap );

    strncat(syslog_buf, content_buf, allowcontentsize);
    bufsize = strlen(syslog_buf);

    // Note: at this point syslog_buf holds the complete syslog packet
    // formated IAW RFC 3164


    /* output to stderr if requested */
    if (LogStat & LOG_PERROR)
    {
        fputs(syslog_buf, stderr);
        fputs("\n",       stderr);
    }

    /* output the message to the local logger and close LogFile */
    if (!connected) if(sendtype) lopenlog(LogTag, LogStat | LOG_NDELAY, 0, sendtype);
                    else lopenlog(LogTag, LogStat | LOG_NDELAY, 0, 0);

    sendstat = send(LogFile, syslog_buf, bufsize, MSG_DONTROUTE);


    if((!connected) || (sendstat == -1) || (bufsize - sendstat != 0))
    {
        if (connected)
        {
            /* Try to reopen the syslog connection.  Maybe it went
               down.  */
            closelog( );
            openlog(LogTag, LogStat | LOG_NDELAY, 0);
        }
        sendstat = 0;
        sendstat = send(LogFile, syslog_buf, bufsize, MSG_DONTROUTE);

        if((!connected) || (sendstat == -1) || (bufsize - sendstat != 0))
        {
            // Close log and output the message to the console.
            if (LogStat & LOG_CONS)
            {
                fputs("SYSLOG ERROR", stdout);
                fputs("\n",           stdout);
                fputs(syslog_buf,     stdout);
                fputs("\n",           stdout);
            }
        }
    }
}  //$* end of vsyslog( )


// openlog( )
//
//  Open system log, required changes here. The stock OS/2 syslogd
//  only accepts TCPIP connection while the BSD port expects connections
//  LOCAL IPC. Work around is to try and connect IPC. If no connection
//  then try and connect TCPIP port 514.
//
//  openlog( ) calls to logger_openlog( ) with sendtype of 0 (auto)
//
void openlog(const char *ident, int logstat, int logfac)
{
    lopenlog(ident, logstat, logfac, 0);
}


// lopenlog( )
//
//  Open system log for logger - int sendtype to manually select connection
//  type (UDP or IPC) 0 -best  1-IPC  2-UDP
//
void lopenlog(const char *ident, int logstat, int logfac, int sendtype)
{
    struct sockaddr_un SyslogAddrIPC;  // IPC
    struct sockaddr_in SyslogAddrTCP;  // TCPIP

    LogType  = 0;
    LogError = 0;

    if (ident != NULL) LogTag = ident;

    LogStat = logstat;
    if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0) LogFacility = logfac;

    if(LogFile == -1 && (sendtype == 0 || sendtype == 1))
    {
        memset(&SyslogAddrIPC, 0, sizeof(SyslogAddrIPC)); // clear SyslogAddrIPC
        SyslogAddrIPC.sun_family = AF_UNIX;               // AF_UNIX or AF_OS2
        strcpy(SyslogAddrIPC.sun_path, _PATH_LOG);

        if (LogStat & LOG_NDELAY) // LogStat must be 0x08
        {
            if((LogFile = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) psock_errno(NULL);
        }
    }
    if (LogFile != -1 && !connected && (sendtype == 0 || sendtype == 1)) // IPC socket is open, now attempt connect
    {
        if((connect(LogFile, (struct sockaddr*) &SyslogAddrIPC, sizeof(SyslogAddrIPC))) == -1)
        {
            if(sock_errno( ) == SOCEADDRNOTAVAIL) // if equal then no syslogd IPC
            {
                soclose(LogFile);
                LogError =  1;
                LogFile  = -1;
            }
        }
        else
        {
            LogType   = 1;
            connected = 1;
        }
    }
    if((LogFile == -1) && (LogError == 1 || sendtype == 2)) // if no connect and IPC has failed
    {
        LogError = 1;
        memset(&SyslogAddrTCP, 0, sizeof(SyslogAddrTCP));

        SyslogAddrTCP.sin_len         = sizeof(SyslogAddrTCP);
        SyslogAddrTCP.sin_family      = AF_INET;
        SyslogAddrTCP.sin_addr.s_addr = htonl(gethostid(  )); // inet_addr("192.168.1.2");
        SyslogAddrTCP.sin_port        = htons(514);

        if (LogStat & LOG_NDELAY) // LogStat must be 0x08
        {
            if((LogFile = socket(AF_INET, SOCK_DGRAM, 0)) == -1) psock_errno(NULL);
        }
    }
    if((LogFile != -1) && (!connected) && (LogError == 1))  // TCP socket is open,
    {                                                       // now attempt connect
        if((connect(LogFile, (struct sockaddr*) &SyslogAddrTCP, sizeof(SyslogAddrTCP))) == -1)
        {
            // Big difference from IPC, TCP open the socket and connect -
            // unless the address is bad - will never get here.
            // The stock OS/2 TCP syslogd is a blind throw of the message
            if(sock_errno( ) == SOCEADDRNOTAVAIL)
            {
                soclose(LogFile);
                LogError =  2;
                LogFile  = -1;
            }
        }
        else
        {
            LogError  = 0;
            LogType   = 2;
            connected = 1;
        }
    }
}  //$* end of openlog( )


// closelog( )
//
// Close the system log and reset
//
void closelog(void)
{
    soclose(LogFile);

    LogFile = -1;
    LogTag = NULL;
    connected = 0;

}  //$* end of closelog( )


// setlogmask( )
//
//  Set the log mask level set LogMask and return old-LogMask
//
int setlogmask(int pmask)
{
    int omask;

    omask = LogMask;

    if (pmask != 0) LogMask = pmask;

    return (omask);

}  //$* end of setlogmask( )

#ifdef __WATCOMC__
// getprogname( )
//
//  get current program name to use as auto LogTag
//  uses Openwatcom __argv - what is GCC equiv?
//
//  gcc has to set LogTag manually
//
char *getprogname( void )
{
    char * argv_zero = __argv[0];
    char * prog_name;

    prog_name = strrchr(argv_zero, SLASHES);

    if( prog_name )
      prog_name++;
    else
      prog_name = argv_zero;

    return( prog_name );
//_execname ( )
}
#endif

