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
 * Description:  logger.exe for Syslogd/2 package. Allows manual sending
 *               syslog messages to the deamon by IPC or UDP.
 *
 *          v1.2 - cleanup and convert from my C++ short-hand to C
 *
 *
 ***************************************************************************/

#define SYSLOG_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <version.h>
#include "syslog.h"

static void  usage(void);

struct PROGOPTS {
    int send;
    int facility;
    int severity;
    int logflags;
    int logtag;
} progopts;

/*
**  LOGGER -- read and log utility
**
**      This routine reads from an input and arranges to write the
**      result on the system log, along with a useful tag.
*/
int main(int argc, char **argv)
{
    int  i;                // tmp for FOR statement
    int  ch;               // for getopt
    int  priority;         // message PRI (FAC || SEV)
    char buf[1024];        // message buffer
    char ident[32] = {0};  // send logtag option

    progopts.send     = 0;            // default send BEST
    progopts.facility = LOG_USER;     // default FAC
    progopts.severity = LOG_NOTICE;   // default PRI
    progopts.logflags = 0;            // default NONE
    progopts.logtag   = 0;

    // if no args display usage and exit
    if(argc == 1)
    {
        usage( );
        exit(0);
    }

    // get args
    while ((ch = getopt(argc, argv, "ha:f:ip:st:")) != -1)
    {
        switch (ch)
        {
            case 'a':  progopts.send     = atoi(optarg);      // 0-best 1-UDP 2-IPC
                       break;

            case 'f':  progopts.facility = (atoi(optarg)<<3); // shift FAC 3bits here
                       break;

            case 'i':  progopts.logflags |= LOG_PID;          // log process id
                       break;

            case 'p':  progopts.severity = atoi(optarg);      // log PRI
                       break;

            case 's':  progopts.logflags |= LOG_PERROR;       // log to standard error
                       break;

            case 't':  ++progopts.logtag;
                       strcpy(ident, optarg);                 // user requested LOGTAG
                       break;

            case 'h':

            default:   usage( );
                       exit(1);
        }
    } // done with args

    // combine FAC and SEV for message PRI
    priority = ( progopts.facility | progopts.severity);

    // send method (UDP/IPC) bounds check
    if(progopts.send < 0 || progopts.send > 2) progopts.send = 0;

    // If sending options, call syslog.c lopenlog( ) to set the options
    // before calling syslog( ). The lopenlog( ) function is a OS/2-ECS call
    // that allows program to pick send method (UDP/IPC). In this line if
    // progopts.logflags is true then we want to set options before logging.
    // Also, if progopts.send is true we want to choose send method.
    if(progopts.logflags || progopts.send || progopts.logtag)
    {
        if(progopts.logtag) lopenlog(ident, progopts.logflags, 0, progopts.send);
        else lopenlog(NULL, progopts.logflags, 0, progopts.send);
    }

    // clear the message buffer
    strnset(buf, '\0', sizeof(buf));

    // log input line if appropriate, test remaining args
    if (argc > 0)
    {
        for(i = optind; i < argc; i++)
        {
            strcat(buf, argv[i]);
            strcat(buf, " ");
        }
        lsyslog(progopts.send, priority, "%s", buf);
    }
    else
    {
        printf("Error: No message to send\n");
        exit(1);
    }

    return(0);
}


void usage(void)
{
    printf("\nlogger v%s for OS/2-ECS  %s\n", SYSLVERSION, __DATE__);
    printf("Compiled with OpenWatcom %d.%d Michael Greene <greenemk@cox.net>\n",
                                                                   OWMAJOR, OWMINOR);
    printf("logger is a utilitity for use with Syslogd/2.\n\n");
    printf("logger: [-a] [-i] [-s] [-t LogTag] [-f fac] [-p pri] [ message ... ]\n\n"  );
    printf("        -a  send method 0 - best (default)  1 - IPC  2 - UDP\n");
    printf("        -f  Facility 1 - 23                                 \n");
    printf("        -p  Priority 0-7                                    \n");
    printf("        -i  Logs logger PID rather than executable name     \n");
    printf("        -s  echos message to console logger run from        \n");
    printf("        -t <TAG>                                            \n");
    printf("            allows user to send custom LogTag rather than   \n");
    printf("            the set LogTag of logger                        \n");
    printf("        message ... Mesaage to send.                        \n");
}

