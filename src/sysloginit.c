/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 2.0       Michael K Greene <greenemk@cox.net>
 *                      December 2005
 *
 *
 *            SYSLOGD/2 compiles with OpenWatom (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <types.h>
#include <string.h>
#include <unistd.h>
#include <direct.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys\un.h>
#include <netinet/in.h>
#include <arpa\inet.h>
#include "syslogmsg.h"
#include "syslogd.h"
#include "ifrecord.h"

extern struct  STATS  stats;

void getaddrinfo(int add_type, char *option, struct  PROGRAMOPTS *prgoptions);
void init( void );

// initalize(  )
//
//  Setup the default options and call the BSD code to process the syslog.conf
//  file.  Any program options found in syslog.conf will override the defaults.
//
void initialize(struct  PROGRAMOPTS *prgoptions)
{
    // setup default options
    prgoptions->DEBUG               = 0;
    prgoptions->marktimer           = 0;
    prgoptions->logpriority         = 0;
    prgoptions->securemode          = 0;
    prgoptions->forwardmode         = 0;
    prgoptions->norepeat            = 0;
    prgoptions->initialized         = 0;
    prgoptions->truncatemode        = 0;
    prgoptions->noiffound           = 0;
    prgoptions->networkchk          = 0;
    prgoptions->highmemmode         = 0;
    prgoptions->idlemulti           = 1;

    strcpy(prgoptions->cfgfilename, SYSLOGDCONF);
    memset(prgoptions->cfgfilefull, 0, sizeof(prgoptions->cfgfilefull));
    sprintf(prgoptions->cfgfilefull, "%s\\%s", getenv("ETC"), SYSLOGDCONF);
    prgoptions->klog                = 0;
    memset(prgoptions->popupfile, 0, 20);    //           = NULL;
    prgoptions->threadipc           = true;  // flag to indicate IPC thread start/running
    prgoptions->threadtcp           = true;  // flag to indicate TCP thread start/running

    strcpy(prgoptions->sendIP, "0.0.0.0");
    strcpy(prgoptions->sendIF, "all");

    // DEBUG: INIT MSG
    if(prgoptions->DEBUG) handleinternalmsg(DEBUGMSG_1, LOG_DEBUG);

    // fallback logging path - try TMP - then TEMP - last CURRENT
    // this is for an alternate if the logging path in cfg file is crap
    memset(prgoptions->fallbackpath, 0, _MAX_PATH);
    if(getenv("TMP") != NULL) strcpy(prgoptions->fallbackpath, getenv("TMP"));
    else if(getenv("TEMP") != NULL) strcpy(prgoptions->fallbackpath, getenv("TEMP"));
         else getcwd(prgoptions->fallbackpath, _MAX_PATH);

    // call to the BSD function
    init( );

    // if listenon not in cfg then still needs if check
    if(!prgoptions->networkchk) {
        struct ifrecord *head = getifrecords( );

        if(head == NULL) ++prgoptions->noiffound;

        ++prgoptions->networkchk; // a check for tcpip has been performed
    }

}  //$* end of initialize


// cfcmd( )
//
//  Call back from the BSD function to process any program options found in
//  the syslog.conf file.  This is an OS/2 feature to include options in the
//  file.
//
void cfcmd(char *cmdline, struct  PROGRAMOPTS *prgoptions)
{
    int  add_type;

    char *command;
    char *option;

    char listen[15];

    command = strtok(cmdline, "=");
    option  = strtok(NULL, " ");

    strlwr(command);
    if(strstr(command, "tcpiplisten")) ++prgoptions->securemode;
    if(strstr(command, "tcpipforward")) ++prgoptions->forwardmode;
    if(strstr(command, "logpriority")) ++prgoptions->logpriority;
    if(strstr(command, "norepeat")) ++prgoptions->norepeat;
    if(strstr(command, "klog")) ++prgoptions->klog;

    if(option != NULL)  {
        if(strstr(command, "marktimer")) prgoptions->marktimer = atoi(option);
        if(strstr(command, "truncate")) {
            ++prgoptions->truncatemode;
            prgoptions->truncatesize = atoi(option);
            if(prgoptions->truncatesize < 10000) prgoptions->truncatesize = 10000;
        }

        if(strstr(command, "listenon")) {
            strlcpy(listen, option, 15);
            if(strlen(listen) > 5) add_type = 1;   // IP address
            else add_type = 2;                     // IF name
            getaddrinfo(add_type, option, prgoptions);
        }

    }

}  //$* end of cfcmd


void getaddrinfo(int add_type, char *option, struct  PROGRAMOPTS *prgoptions)
{
    char ifnamein[IFNAMSIZ] ={0};

    struct ifrecord *head = NULL;
    struct ifrecord *next = NULL;

    strlcpy(ifnamein, strlwr(option), strlen(option));

    head = getifrecords( );

    next = head;

    if(head == NULL) {
        ++prgoptions->networkchk; // a check for tcpip has been performed
        ++prgoptions->noiffound;  // this will disable UDP thread from running
    } else {
        while(next->nextif != NULL) {
            if((add_type == 1) && (next->ifEthrAddr.s_addr == inet_addr(option))) {
                strcpy(prgoptions->sendIP, inet_ntoa(next->ifEthrAddr));
                strcpy(prgoptions->sendIF, next->ifName);
                ++prgoptions->networkchk;
                break;
            }

            if((add_type == 2) && (strstr(next->ifName, ifnamein)!=NULL)) {
                strcpy(prgoptions->sendIP, inet_ntoa(next->ifEthrAddr));
                strcpy(prgoptions->sendIF, next->ifName);
                ++prgoptions->networkchk;
                break;
            }

            next = next->nextif;
        }

        if_freemem(head);

    }

}

