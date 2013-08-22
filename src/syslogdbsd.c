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
 *                      Also, OS/2 EMX changes by Mikael St†ldal
 *                      and Andrew Hood
 *
 *
 *            SYSLOGD/2 compiles with OpenWatom (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  Some of the original syslogd BSD code, some functions
 *               modified or removed.
 *
 ***************************************************************************/

/*      $NetBSD: syslogd.c,v 1.64 2004/03/06 14:41:59 itojun Exp $      */

/*
 * Copyright (c) 1983, 1988, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

//#include <sys/cdefs.h>
//#ifndef lint
// __COPYRIGHT("@(#) Copyright (c) 1983, 1988, 1993, 1994
//        The Regents of the University of California.  All rights reserved.\n");
//#endif /* not lint */

//#ifndef lint
//#if 0
//static char sccsid[] = "@(#)syslogd.c   8.3 (Berkeley) 4/4/94";
//#else
//__RCSID("$NetBSD: syslogd.c,v 1.64 2004/03/06 14:41:59 itojun Exp $");
//#endif
//#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 */

#define INCL_DOSMEMMGR

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <sys\types.h>
#include <direct.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys\stat.h>
#include <io.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <sys/cdefs.h>  // handle the __BEGIN_DECLS and __END_DECLS

#define _POSIX_MAX_PATH  _MAX_PATH
#define   SYSLOG_NAMES

#include "syslogmsg.h"
#include "syslogd.h"


void init( void );
void cfline(char *line, struct filed *f, int linenum); // decode conf file
void cfcmd( char *cmdline, struct  PROGRAMOPTS *prgoptions); // handle conf file prg options
int  decode(const char *name, CODE *codetab);          // decode syslog names to numbers
void domark(int mark);                                 // send mark / flush
void fprintlog(struct filed *f, int flags);            // output msg to file or IP
void logerror(int sev, const char *, ...);             // send internal message
void printline(char *, char *);                        // main routine for syslog msg
void printsys(char *msg);                              // Linux klog - use for popuplog

extern void handleinternalmsg(int msgnum, int sev);    // handle program messages
extern char *gettimestamp( void );                     // get timestap IAW RFC 3164

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */

// repeatinterval array of 3 int, size of int = 4 then (12/4)-1 = 2
// MAXREPEAT will always be 2 unless int size < 4 or add times
int     repeatinterval[] = { 30, 120, 600 };    /* # of secs before flush */
#define MAXREPEAT     ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
// f_time + repeatinterval[ ] (30 120 600)
#define REPEATTIME(f) ((f)->f_time + repeatinterval[(f)->f_repeatcount])
// increment f_repeatcount by 1 but do not increase above MAXREPEAT ( 2 )
#define BACKOFF(f)    {if (++(f)->f_repeatcount > MAXREPEAT) (f)->f_repeatcount = MAXREPEAT;}

extern  struct PROGRAMOPTS  *prgoptions;
extern  struct STATS  stats;
struct  filed  *HeadFiles;
struct  filed  consfile;

extern  struct CURRENTMSG currentmsg;

extern  char  LocalHostName[MAXHOSTNAMELEN+1];
extern  int   tcpsocket;
static  int   linenum;


// printline( )
//
//  Take a raw input line, decode the message, and print the message
//  on the appropriate log files.
//
//  No changes in this function from the BSD source, more comments added.
//  hname is hostname - passes through without change
//  msg is the raw message - priority is checked, if none or invalid tag
//  with default priority.
//  calls logmsg before returning.
//
void printline(char *hname, char *msg)
{
    int  c;

    char *p;
    char *q;

    unsigned int  pri;
    char line[MAXLINE + 1];  // 1024 + 1

    (void)hname;

    ++stats.bsdprocessmsg; // count of messages that got here

    p = msg;

    // check for start of pri - "<" if true then scan while digits
    // digit (* 10 to move left) - '0' to convert to int
    if (*p == '<')
    {
        pri = 0;
        while (isdigit(*++p)) pri = 10 * pri + (*p - '0');
        if (*p == '>') ++p;
    }
    else pri = DEFUPRI; // RFC3164 PRI not found (LOG_USER|LOG_NOTICE) will be used

    // my check, seems that I could get some hugh pri numbers through here
    // so I added additional checks max FAC 23 and SEV 7 or 191
    if(pri > 191)
    {
        handleinternalmsg(RUNNINGERR_5, LOG_ERR);
        pri = DEFUPRI;
    }

    /* don't allow users to log kernel messages */
    // if LOG_KERN then change FAC to LOG_USER
    // if (LOG_FAC(pri) == LOG_KERN) pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

    // remove any control characters 0-31 - I do a brut force check in the
    // receiving thread replacing ascii < 32 or > 126 with space (RFC3164)

    q = line;

    while ((c = *p++) != '\0' && q < &line[sizeof(line) - 2])
    {
        c &= 0177;
        if (iscntrl(c)) if (c == '\n') *q++ = ' ';
                        else if (c == '\t') *q++ = '\t';
                             else
                             {
                                 *q++ = '^';
                                 *q++ = c ^ 0100;
                             }
        else *q++ = c;
    }
    *q = '\0';

    // pass the clean message to logmsg with priority "<XX>" removed (line[])
    // and placed in int pri. hname is a pass through and flags 0
    logmsg(pri, line, 0);

}  //$* end of printline


// printsys( )
//
//  Take a raw input line from /dev/klog, split and format similar to syslog().
//
//  Takes a plain message and gives it some syslog format and send to logger.
//  It is used in UNIX for kernel messages, I am going to use it to pick-up
//  the POPUPLOG.OS2 and dump to syslog logger.
//  FAC 0 - LOG_KERNEL  SEV - 2 LOG_CRIT
//
void printsys(char *msg)
{
    int  c;
    int  pri;
    int  flags;
    char *lp;
    char *p;
    char *q;
    char line[MAXLINE + 1];

    (void)strlcpy(line, _PATH_UNIX, sizeof(line));
    (void)strlcat(line, ": ", sizeof(line));
    lp = line + strlen(line);
    for (p = msg; *p != '\0'; )
    {
        flags = ADDDATE | ADDHOST ; /* fsync file after write */
        pri = DEFSPRI;
        if (*p == '<')
        {
            pri = 0;
            while (isdigit(*++p)) pri = 10 * pri + (*p - '0');
            if (*p == '>') ++p;
        }
        else
        {
            /* kernel printf's come out on console */
            flags |= IGN_CONS;
        }
        if (pri &~ (LOG_FACMASK|LOG_PRIMASK)) pri = DEFSPRI;
        q = lp;
        while (*p != '\0' && (c = *p++) != '\n' && q < &line[MAXLINE]) *q++ = c;
        *q = '\0';

        logmsg(pri, line, flags);
    }

}  //$* end of printsys


// logerror( )
//
//  Print syslogd errors some place. Simple internal message handler.
//  Function from original BSD source, modified.
//
void logerror(int sev, const char *fmt, ...)
{
    int pri;

    va_list ap;
    char tmpbuf[BUFFER_SIZE];
    char buf[BUFFER_SIZE];

    ++stats.sysintrecvmsg;

    va_start(ap, fmt);

    (void)vsnprintf(tmpbuf, sizeof(tmpbuf), fmt, ap);

    va_end(ap);

    (void)snprintf(buf, sizeof(buf), "syslogd: %s", tmpbuf);

    if(sev) pri = LOG_SYSLOG | sev;
    else pri = LOG_SYSLOG|LOG_ERR;

    logmsg(pri, buf, ADDDATE | ADDHOST);

    return;

}  //$* end of logerror


// logmsg( )
//
//  Log a message to the appropriate log files, users, etc. based on
//  the priority.
//
void logmsg(int pri, char *msg, int flags)
{
    int  i;
    int  msglen;
    int  foundhost;
    int  fac;
    int  sev;

    char *timestamp;

    struct filed *f;

    // structure for more control over message logging format
    currentmsg.pri        = 0;
    *currentmsg.timestamp = 0;
    *currentmsg.hostname  = 0;
    *currentmsg.msg       = 0;

    // ***** handle TIMESTAMP *****
    // Check to see if msg has RFC3164 TIMESTAMP, if not set ADDDATE flag
    // with priority removed check for msglen < 16 and format:
    //
    // Example:
    // O c t   2 2   0 7 : 1 0 : 00
    //       _     _     :     :     _
    // 0 1 2 3 4 5 6 7 8 9 1 1 1 1 1 1
    //                     0 1 2 3 4 5
    msglen = strlen(msg);
    if (msglen < 16 || msg[3] != ' '  || msg[6] != ' ' || msg[9] != ':'
                    || msg[12] != ':' || msg[15] != ' ') flags |= ADDDATE;

    // if ADDDATE get current else strip message TIMESTAMP

    if (flags & ADDDATE) timestamp =  gettimestamp( );
    else
    {
        timestamp = msg; // when used later extra gets stripped
        msg += 16;
        msglen -= 16;
    }

    // set the timestamp in RFC3164 format <currentmsg.timestamp>
    strncpy(currentmsg.timestamp, timestamp, 15);
    // end of handle TIMESTAMP routines *****


    // ***** handle HOSTNAME *****
    // Try to find hostname, IAW RFC3164. Host should be prior to tag, if
    // ":" (ascii 58) or "[" (ascii 91) found before space assume no
    // hostname.

    foundhost = 0;

    // no reason to scan for hostname if flagged to ADDHOST
    if(!(flags & ADDHOST))
    {
        for(i = 0; i < strlen(msg); i++)
        {
            if((int)msg[i] == 58 || (int)msg[i] == 91) break;
            if((int)msg[i] == 32)
            {
                currentmsg.hostname[i] = '\0';
                foundhost = 1;
                break;
            }
            currentmsg.hostname[i] = msg[i];
        }
    }

    // hostname not found generate from current host and store for
    // logging <currentmsg.hostname>
    if(!foundhost || (flags & ADDHOST))
    {
        strlcpy(currentmsg.hostname, LocalHostName, sizeof(currentmsg.hostname));
        strlcpy(currentmsg.msg, msg, sizeof(currentmsg.msg));
    }
    else
    {
        char * p = msg;
        for(i = 0; i < (strlen(currentmsg.hostname)+1); i++) ++p;
        strlcpy(currentmsg.msg, p, sizeof(currentmsg.msg));
    }
    // end of handle HOSTNAME routines *****


    // ***** second phase PRI processing *****
    // extract facility and priority level, MARK gets FAC of 24
    sev = LOG_PRI(pri); // extract Severity

    if (flags & MARK) fac = LOG_NFACILITIES;  // LOG_NFACILITIES = 24
    else fac = LOG_FAC(pri);

    // all done! standard PRI, TIMESTAMP, HOSTNAME have been corrected by
    // this point, so make the logging priority <currentmsg.pri>
    currentmsg.pri = LOG_MAKEPRI(fac, sev);

    // end of handle PRI routines *****

    // update msglen to reflect changes
    msglen = strlen(currentmsg.msg);

    // message parts now stored:
    // currentmsg.pri        -  msg FAC and SEV
    // currentmsg.timestamp  -  TIMESTAMP
    // currentmsg.hostname   -  HOSTNAME
    // currentmsg.msg        -  CONTENT


    // ***** handle message output routines *****
    // log the message to the particular outputs

    // prgoptions->initialized == 0 program just started, is set to 1
    // in INIT( ) - surpress actual logging
    if (!prgoptions->initialized)
    {
        printf("%s %s", currentmsg.timestamp, msg);
        return;
    }


    // output message IAW syslog.conf settings
    for (f = HeadFiles; f; f = f->f_next)
    {
        if (f->f_pmask[fac] < sev || (f->f_pmask[fac] == INTERNAL_NOPRI)) continue;

        // if this is console type and IGN_CONS is true continue to next f structure
        if (f->f_type == F_CONSOLE && (flags & IGN_CONS)) continue;


        // suppress duplicate lines to this file unless NoRepeat
        //
        //  - not MARK
        //  - msg length == prevous msg length
        //  - repeat function active
        //  - msg == previous msg
        //  - hostname == previous hostname
        //
        if ( (flags & MARK) == 0
              && (f->f_type != F_FORW)
              && msglen == f->f_prevlen
              && currentmsg.pri == f->f_prevpri
              && !prgoptions->norepeat
              && !strcmp(currentmsg.msg, f->f_prevline)
              && !strcmp(currentmsg.hostname, f->f_prevhost) )
        {

            f->f_lasttime = time( NULL );
            f->f_prevcount++;

            // DEBUG routine
            if(prgoptions->DEBUG) printf("msg repeated %d times, %ld sec of %d\n",
                    f->f_prevcount, time( NULL ) - f->f_time, repeatinterval[f->f_repeatcount]);

            /*
             * If domark would have logged this by now,
             * flush it now (so we don't hold isolated messages),
             * but back off so we'll flush less often
             * in the future.
             */
            if (time( NULL ) > REPEATTIME(f))
            {
                fprintlog(f, flags);    // flush held message
                BACKOFF(f);             // inc f_repeatcount next 30 120 600
            }
        } // end of dupline
        else
        {
            /* new line, save it */
            if (f->f_prevcount) fprintlog(f, 0); // if previous messages flush

            f->f_repeatcount = 0;
            f->f_prevpri     = currentmsg.pri;
            f->f_lasttime    = time( NULL );

            (void)strncpy(f->f_prevhost, currentmsg.hostname, sizeof(currentmsg.hostname));

            if (msglen < MAXSVLINE)
            {
                f->f_prevlen = strlen(currentmsg.msg);
                (void)strlcpy(f->f_prevline, currentmsg.msg, sizeof(f->f_prevline));
                fprintlog(f, flags);
            }
            else
            {
                f->f_prevline[0] = 0;
                f->f_prevlen = 0;
                fprintlog(f, flags);
            }

        }  //$* check repeat

    }  //$* end of <for> output message

}  //$* end of logmsg


// dormark( )
//
//  output syslog MARK
//
void domark( int mark )
{
    struct filed *f;

    if(mark)
    {
        ++stats.markmsgs;
        logmsg(LOG_INFO, "syslogd: -- MARK --", ADDDATE|MARK);
    }

    if(mark == 2)
    {
        ++stats.markmsgs;
        logmsg(LOG_INFO, "syslogd: -- MIDNIGHT MARK --", ADDDATE|MARK);
    }

    for (f = HeadFiles; f; f = f->f_next)
    {
//        if(prgoptions->DEBUG) printf("flush %s: repeated %d times, %d sec.\n", TypeNames[f->f_type], f->f_prevcount, repeatinterval[f->f_repeatcount]);

        if (f->f_prevcount && time( NULL) >= REPEATTIME(f))
        {
            fprintlog(f, 0);
            BACKOFF(f);
        }
    }

}  //$* end of domark


// init( )
//
//  Initialize the BSD side, read and parse syslog.conf
//
void init( void )
{
    char   *p;
    char   cline[LINE_MAX];

    struct filed *f;
    struct filed *next;
    struct filed **nextp;

    FILE *cfgfile;

    stats.bsdprocessmsg = 0;
    stats.sysintrecvmsg = 0;
    stats.markmsgs      = 0;

    linenum   = 0;          // keep track of line number in syslog.conf being processed
    HeadFiles = NULL;       // Files is a pointer to a filed type structure - NULL tail
    nextp     = &HeadFiles; // nextp is a pointer to the pointer Files

    // flush any entries - second and after reads
    for (f = HeadFiles; f != NULL; f = next)
    {
        /* flush any pending output */
        if (f->f_prevcount) fprintlog(f, 0);

        switch (f->f_type)
        {
            case F_FILE:
            case F_CONSOLE:
            case F_FORW:    if (f->f_un.f_forw.f_hname) printf("\n");
                            break;
        }
        next = f->f_next;
        free((char *)f);
    }
    HeadFiles = NULL;
    nextp     = &HeadFiles;


    // if can not open file, so load a couple defaults send a message
    // so user can correct
    if(access(prgoptions->cfgfilefull, F_OK) != 0)
    {
        handleinternalmsg(STARTERROR_12, LOG_ERR);

        *nextp = (struct filed *)calloc(1, sizeof(*f));

        // setup static config
        cfline("*.ERR\t/dev/console", *nextp, 0);

        (*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));

        //setup static config
        cfline("*.PANIC\t*", (*nextp)->f_next, 0);

        prgoptions->initialized = 1;

        return;
    }
    else
    {
        /* open the configuration file */
        cfgfile = fopen(prgoptions->cfgfilefull, "rt");

        // Foreach line in the conf table, open that file.

        f = NULL;

        while(!feof(cfgfile))
        {
            fgets(cline, MAXLINE, cfgfile);

            ++linenum;

            // check for end-of-section, comments, strip off trailing
            // spaces and newline character.
            //
            for (p = cline; isspace(*p); ++p) continue;

            // if blank or comment continue
            if (*p == '\0' || *p == '#') continue;

            // new for OS/2: if % then command handle (cfcmd()) and continue
            if (*p == '%')
            {
                ++p;
                cfcmd(p, prgoptions);
                continue;
            }


            for (p = strchr(cline, '\0'); isspace(*--p);) continue;
            *++p = '\0';

            // allocate new structure f
            f = (struct filed *)calloc(1, sizeof(*f));

            *nextp = f;
            nextp = &f->f_next;

            cfline(cline, f, linenum); // call cfline to decode cfg line and store
        }

        /* close the configuration file */
        fclose(cfgfile);

        prgoptions->initialized = 1;

    }

}  //$* end of init


// fprintlog( )
//
//  This function sends the the msg/entry to console, file, or TCPIP forwards.
//  I changed this - all that is needed is to print to screen, save to passed
//  file ( filed f->), or toss it to another receiver.
//
void fprintlog(struct filed *f, int flags)
{
    char outmessage[1024];   // message to output buffer

    FILE *outfile;

    if (f->f_prevcount==0)
    {
        if(prgoptions->logpriority  || (f->f_type == F_FORW))
                                   sprintf(outmessage, "<%d>%s %s ",
                                           currentmsg.pri,
                                           currentmsg.timestamp,
                                           currentmsg.hostname);

        else sprintf(outmessage, "%s %s ", currentmsg.timestamp, currentmsg.hostname);

        strlcat(outmessage, currentmsg.msg, (1024 - strlen(outmessage)));
        strlcat(outmessage, "\n", (1024 - strlen(outmessage)));
    }
    else if (f->f_prevcount > 0)
         {
             sprintf(outmessage, "last message repeated %d times\n", f->f_prevcount);
         }
         else
         {
             if(prgoptions->logpriority  || (f->f_type == F_FORW))
                               sprintf(outmessage, "<%d>%s %s ",
                                       currentmsg.pri,
                                       currentmsg.timestamp,
                                       currentmsg.hostname);

             else sprintf(outmessage, "%s %s ",
                          currentmsg.timestamp,
                          currentmsg.hostname);

             strlcat(outmessage, f->f_prevline, (1024 - strlen(f->f_prevline)));
             strlcat(outmessage, "\n", (1024 - strlen(outmessage)));
         }

    f->f_time = time( NULL );

    // TypeNames valid: UNUSED, FILE, CONSOLE, FORW
    switch (f->f_type)
    {
        case F_UNUSED:  break;

        case F_FORW:    if(prgoptions->forwardmode && !prgoptions->noiffound)
                        {
                            if (sendto(tcpsocket, outmessage, strlen(outmessage),
                                       0, (struct sockaddr *)&f->f_un.f_forw.f_addr,
                                       sizeof f->f_un.f_forw.f_addr) != strlen(outmessage))
                            {
                                logerror(LOG_ERR, "sendto");
                            }
                        }
                        break;

        case F_CONSOLE: if ((flags & IGN_CONS)) break;
                        printf("%s", outmessage);
                        break;

        case F_FILE:    f->f_prevcount = 0;
                        outfile = fopen(f->f_un.f_fname, "atc");
                        if(outfile != NULL)
                        {
                            fputs(outmessage, outfile);
                            fclose(outfile);
                        }
                        else printf("I got here - but bad file\n");

                        break;
    }

    f->f_prevcount = 0;

}  //$* end of fprintlog


// cfline( )
//
//  Crack a configuration file line. Pass config line and file struct to
//  fill with options.
//
void cfline(char *line, struct filed *f, int linenum)
{
    int  i;
    int  pri;
    int  sp_err;

    char *bp;
    char *p;
    char *q;
    char buf[MAXLINE];
    char drive[ _MAX_DRIVE] = {0};
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];
    char logpath[_MAX_PATH];

    struct hostent *hp;

    /* clear out file entry */
    memset(f, 0, sizeof(*f));
    // fill f_pmask with INTERNAL_NOPRI  0x10 as default
    for (i = 0; i <= LOG_NFACILITIES; i++) f->f_pmask[i] = INTERNAL_NOPRI;

    // strip any spaces or tabs prior to the selector
    q = line;
    if (isblank((unsigned char)*line))
    {
        while (*q++ && isblank((unsigned char)*q));
        line = q;
    }

    /*
     * q is now at the first char of the log facility
     * There should be at least one tab after the log facility
     * Check this is okay, and complain and fix if it is not.
     */
    q = line + strlen(line);
    while (!isblank((unsigned char)*q) && (q != line)) q--;
    if ((q == line) && strlen(line))
    {
        /* No tabs or space in a non empty line: complain */
        logerror(LOG_ERR, "Error: Line %d - log facility or log target missing", linenum);
    }

    /* q is at the end of the blank between the two fields */
    sp_err = 0;
    while (isblank((unsigned char)*q) && (q != line)) if (*q-- == ' ') sp_err = 1;

    if (sp_err)
    {
        /*
         * A space somewhere between the log facility
         * and the log target: complain
         */
        // logerror(LOG_ERR, "Warning: Line %d - space found where tab is expected", linenum);
        /* ... and fix the problem: replace all spaces by tabs */
        while (*++q && isblank((unsigned char)*q)) if (*q == ' ') *q='\t';
    }

    /* scan through the list of selectors */
    for (p = line; *p && *p != '\t';)
    {
        /* find the end of this facility name list */
        for (q = p; *q && *q != '\t' && *q++ != '.'; ) continue;

        /* collect priority name */
        for (bp = buf; *q && !strchr("\t,;", *q); ) *bp++ = *q++;

        *bp = '\0';

        /* skip cruft */
        while (strchr(", ;", *q)) q++;

        /* decode priority name */

        // * = 00000111 + 1 -> 00001111  all priorities
        if (*buf == '*') pri = LOG_PRIMASK; // + 1;
        else
        {
            pri = decode(buf, prioritynames);
            if (pri < 0)
            {
                logerror(LOG_ERR, "Line %d - Unknown priority name `%s'", linenum, buf);
                return;
            }
        }

        /* scan facilities */
        while (*p && !strchr("\t.;", *p))
        {
            for (bp = buf; *p && !strchr("\t,;.", *p); ) *bp++ = *p++;

            *bp = '\0';

            if (*buf == '*') for (i = 0; i < LOG_NFACILITIES; i++)
                                                f->f_pmask[i] = LOG_PRIMASK; //pri;
            else
            {
                int i = decode(buf, facilitynames);
                if (i < 0)
                {
                    logerror(LOG_ERR, "Unknown facility name `%s'", buf);
                    return;
                }
                f->f_pmask[i >> 3] = pri;  // shift FAC right 3 bits
            }

            while (*p == ',' || *p == ' ') p++;
        }
        p = q;
    }

    /* skip to action part */
    sp_err = 0;

    // skip tab and spaces between selector and action
    while ((*p == '\t') || (*p == ' ')) p++;

    // handle actions
    switch (*p)
    {
        // handle ip address / forward
        case '@':  (void)strcpy(f->f_un.f_forw.f_hname, ++p);

                   hp = gethostbyname(p);
                   if (hp == NULL)
                   {
                       logerror(LOG_ERR, "Error resolving hostname");
                   }
                   else
                   {
                       memset(&f->f_un.f_forw.f_addr, 0, sizeof(f->f_un.f_forw.f_addr));
                       f->f_un.f_forw.f_addr.sin_len    = sizeof(f->f_un.f_forw.f_addr);
                       f->f_un.f_forw.f_addr.sin_family = AF_INET;
                       f->f_un.f_forw.f_addr.sin_port   = htons(514);
                       memmove(&f->f_un.f_forw.f_addr.sin_addr, hp->h_addr, hp->h_length);
                       f->f_type = F_FORW;
                   }
                   break;

        case '-':  f->f_type = F_CONSOLE;
                   (void)strcpy(f->f_un.f_fname, ++p);
                   break;

        // check for path and file exists - add version 1.1
        // updated in v2.0
        case '=':  f->f_type = F_FILE;

                   (void)strcpy(f->f_un.f_fname, ++p);

                   // split up the log file to get the path, check if exists
                   _splitpath(f->f_un.f_fname, drive, dir, fname, ext);
                   strlcpy(logpath, drive, strlen(drive)+1);
                   strlcat(logpath, dir, strlen(dir)+strlen(drive));

                   // if can not find path then fall back to alt logging path
                   if(access(logpath, F_OK) != 0)
                   {
                       sprintf(logpath, "%s\\%s%s", prgoptions->fallbackpath, fname, ext);
                       logerror(LOG_ERR, "Error: logging to %s\n", logpath);
                   }

                   break;

        default:   f->f_type = F_UNUSED;
                   break;
    }

}  //$* end of cfline


// decode( )
//
//  Decode a symbolic name to a numeric value
//
//  pass string to convert and CODE passed facilitynames or prioritynames
//  return FAC unshifted or SEV int value
//  Remains as in original source, so code numbers can be used in syslog.conf
//
int decode(const char *name, CODE *codetab)
{
    CODE *c;
    char *p;
    char buf[40];

    if (isdigit(*name)) return (atoi(name));

    for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++)
    {
        if (isupper(*name)) *p = tolower(*name);
        else *p = *name;
    }

    *p = '\0';
    for (c = codetab; c->c_name; c++) if (!strcmp(buf, c->c_name)) return (c->c_val);

    return (-1);

}  //$* end of decode

