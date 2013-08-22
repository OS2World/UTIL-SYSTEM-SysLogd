/****************************************************************************
 *
 *           S Y S L O G D / 2    A UN*X SYSLOGD clone for OS/2
 *
 *  ========================================================================
 *
 *    Version 2.0       Michael K Greene <greenemk@cox.net>
 *                      October 2004
 *
 *
 *         SYSLOGD/2 compiles with OpenWatom 1.4 (www.openwatcom.org).
 *
 *  ========================================================================
 *
 * Description:  If SUPPRESSPOPUPS=<drive> is set in config.sys will take
 *               popuplog.os2 and log as a kernel message via printsys( ).
 *               Requires -k option to be set.
 *
 * Changes:
 *               December 2005 - convert from C++ to C
 *
 ***************************************************************************/

#define INCL_DOSMISC
#define INCL_DOSERRORS
#define INCL_DOSPROCESS

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <ctype.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <io.h>
#include <fcntl.h>
#include "syslogd.h"


// poplog( )
//
//  Open the popuplog and sent to printsys( )
//
void poplog(struct PROGRAMOPTS *prgoptions)
{
    int start = 1;

    char buffer[256];

    FILE *popfile;

    if(access(prgoptions->popupfile, F_OK) == 0)
    {
        popfile = fopen(prgoptions->popupfile, "rt");

        while(!feof(popfile))
        {
            fgets(buffer, 256, popfile);

            if(start == 1)
            {
                printsys(" --< START POPUPLOG ENTRY >-- ");
                start = 0;
            }

            if((int)buffer[0] == 45)
            {
                printsys(" --<  END POPUPLOG ENTRY  >-- ");
                start = 1;
            }
            else if((int)buffer[0] != 0) printsys( buffer );
            DosSleep(20L);
        }

        printsys(" --<  END POPUPLOG ENTRY  >-- ");

        fclose(popfile);

        remove(prgoptions->popupfile);
    }

    if(access(prgoptions->flashlog, F_OK) == 0)
    {
        popfile = fopen(prgoptions->flashlog, "rt");

        while(!feof(popfile))
        {
            fgets(buffer, 256, popfile);
            printsys( buffer );
            DosSleep(20L);
        }

        fclose(popfile);

        remove(prgoptions->flashlog);
    }
}


// getpopcfg( )
//
//  Scan config.sys and setup the path to popuplog.os2
//
void getpopcfg(struct PROGRAMOPTS *prgoptions)
{
    int  i;
    int  found = 0;

    char buffer[256];
    char bootcfg[15];
    char popfile[15];
    char *odinlog;

    FILE *configsys;

    ULONG  aulSysInfo[QSV_MAX] = {0};

    // New - flash error log location
    odinlog = getenv("MOZ_PLUGIN_PATH");
    if(odinlog != NULL)
    {
        strcpy(prgoptions->flashlog, odinlog);
        strcat(prgoptions->flashlog, "\\except.log");
    }

    DosQuerySysInfo(1L, QSV_MAX, (PVOID)aulSysInfo, sizeof(ULONG)*QSV_MAX);

    // get popuplog.os2 location
    sprintf(bootcfg, "%c%s", (char)(aulSysInfo[QSV_BOOT_DRIVE -1]+64), ":\\CONFIG.SYS");

    configsys = fopen(bootcfg, "rt");

    while(!feof(configsys))
    {
        fgets(buffer, 256, configsys);
        if(strstr(strupr(buffer), "SUPPRESSPOPUPS"))
        {
            found = 1;
            break;
        }
    }
    fclose(configsys);

    for(i = 0; i < strlen(buffer); i++) if(buffer[i] == '=') break;
    for(;i < strlen(buffer); ++i)
    {
        if(isalpha(buffer[i]))
        {
            sprintf(popfile, "%c%s", buffer[i], ":\\POPUPLOG.OS2");
            strcpy(prgoptions->popupfile, popfile);
            break;
        }
    }

    if(found == 0)
    {
        --prgoptions->klog;
        logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: SUPPRESSPOPUPS not found - klog disabled", ADDDATE | ADDHOST);
    }
}


