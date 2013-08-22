#----------------------------------------------------------------------------
#
# Makefile for ECS-OS2 Syslogd/2 using OpenWatcom 1.4
#
#----------------------------------------------------------------------------

#DEBUG = 1

CC = wcc386
LD = wlink
RC = wrc

# Machine type -5r Pent -6r Pent Pro
MACHINE= -6r

#Optimization None -od  - Fastest possible -otexan
!ifndef DEBUG
LDDEBUG   = op el
CDEBUG    = -d0
OPT       = -otexan
!else
LDDEBUG   = d all op map op symf
CDEBUG    = -d3 -v
OPT       = -od
!endif

INCLUDE = ..\src\include;..\syslog;$(%watcom)\h;$(%watcom)\h\os2
CFLAGS  = -i=$(INCLUDE) $(CDEBUG) -w4 -e25 -zq $(OPT) $(MACHINE) -bm -bt=OS2 -mf
LDFLAGS = $(LDDEBUG) op maxe=25

.EXTENSIONS:.rc .res

LOGGER_OBJS = logger.obj syslog.obj

SYSLOGCTRL_OBJS = syslogctrl.obj syslog.obj

SYSLOGD_OBJS = syslogd.obj syslogdbsd.obj sysloginit.obj syslogpop.obj syslogpipe.obj &
               syslogdipc.obj syslogdtcp.obj ifrecord.obj

.c:..\src;..\syslog

.c.obj:
  $(CC) $(CFLAGS) $<

all: logger.exe syslogctrl.exe syslogd.exe inffile movefiles

logger.exe: $(LOGGER_OBJS)
  $(LD) NAME logger SYS os2v2 $(LDFLAGS) FILE {$(LOGGER_OBJS)}

syslogctrl.exe: $(SYSLOGCTRL_OBJS) syslogctrl.res
  $(LD) NAME syslogctrl SYS os2v2 $(LDFLAGS) FILE {$(SYSLOGCTRL_OBJS)}
  $(RC) syslogctrl.res syslogctrl.exe

syslogd.exe: $(SYSLOGD_OBJS) syslogd.res
  $(LD) NAME syslogd SYS os2v2 $(LDFLAGS) FILE {$(SYSLOGD_OBJS)}
  $(RC) syslogd.res syslogd.exe

syslogctrl.res:
  copy ..\src\syslogctrl.rc .
  $(RC) -r syslogctrl.rc

syslogd.res:
  copy ..\src\syslogd.rc .
  $(RC) -r syslogd.rc

pack: dolxlite movefiles .SYMBOLIC

dolxlite: .SYMBOLIC
  lxlite logger.exe
  lxlite syslogctrl.exe
  lxlite syslogd.exe

inffile : .SYMBOLIC
  ipfc -i ..\src\syslogd.ipf .\syslogd.inf

movefiles : .SYMBOLIC
  copy *.exe ..\
  copy *.inf ..\

clean : .SYMBOLIC
  -@rm *.obj
  -@rm *.exe
  -@rm *.err
  -@rm *.lst
  -@rm *.map
  -@rm *.res
  -@rm *.rc
  -@rm *.inf

