:userdoc.
:docprof toc=1234.
:title.Syslogd /2 for OS/2 and ECS

:h1.Introduction
:p.
:hp2.
.ce Syslogd/2 v1.2  -  October 2005
:ehp2.
:p.This is a version of syslogd I did for my own use and it is based on the BSD version of syslogd. I wanted to have options provided by the BSD and OS/2 version in one program.

:p.Syslogd/2 provides the following options&colon.
:ol.
:li.Receive both IPC and UDP messages, so is compatible with both BSD and stock OS/2 programs which log to syslogd.
:li.The ability to log the message priority.
:li.Parse the popuplog.os2 to a log file and remove original.
:li.Allow command line options to be included in syslog.conf.
:li.Allow manual rotation of log files.
:li.Allow truncating log files manually or automatically.
:eol.
:p.This is my first attempt to write docs for Syslogd/2 and I am sure I missed much. If you find
 mistakes please email me and I will correct them.


:h2.Background
:p.I originally uploaded it to hobbes and it seemed to generate some interest over the last year.
 This was my second OS/2 project and it suites my needs, if you like it great!! If you do not,
 email me and I might change it or use another version. I welcome any comments, good or bad&colon. greenemk@cox.net
:p.Syslogd/2 is freeware and the source might not be pretty but it is included. The
source compiles with OpenWatcom 1.4 only because I now use some of the new included functions.
(strlcpy, strlcat, getopt as a few examples)

:p.Syslogd/2 should work on all versions of Warp4, MCP, and ECS. I do not have a system loaded with
 Warp3, so I do not know if it works. Programs that log to syslogd which I have tested (that I
 know of) or have been emailed to me&colon.
:ul.
:li.OS/2 telnetd
:li.ftpserver 1.26 (by Peter Moylan)
:li.identd 1.3
:li.cron/2 (private version)
:li.LinkSys Access Point
:li.HP Print Server
:li.BIND (dns) 8.2.4
:eul.
:p.and I am sure many others...
.br

:h2.What is syslogd
:p.Ok, what is syslogd? In its most simplistic terms, the syslog protocol provides a
 transport to allow a machine to send event notification messages across IP networks
 to event message collectors - also known as syslog servers.  Since each process,
 application and operating system was written somewhat independently, there is little
 uniformity to the content of syslog messages.  For this reason, no assumption is made
 upon the formatting or contents of the messages.  The protocol is simply designed to
 transport these event messages.  In all cases, there is one device that originates
 the message.  The syslog process on that machine may send the message to a collector.
 No acknowledgement of the receipt is made. Sound simple? The previous is from RFC 3164.
 Here are a couple of webpages that maybe a help&colon.

:ul.
:li.RFC 3164 - The BSD Syslog Protocol (http&colon.//www.faqs.org/rfcs/rfc3164.html)
:li.Syslog.org (http&colon.//www.syslog.org)
:eul.

:p.Anyway, I could go on giving a detailed description of syslogd but it works almost exactly
 like the BSD version. In brief, if a program is able to send a UDP message to port 514 or to
 connect and send IPC a message then Syslogd/2 can format and filter to a user defined log file.
 Also, if configured correctly messages can be forwarded to another syslogd server.

:p.I try to provide more details in a number of appendix:
:ul.
:li.Appendix A - Syslog Message Format
:li.Appendix B - Detailed description of Syslogd/2 operations
:li.Appendix C - Using syslogd with your programs
:li.Appendix D - Syslogd/2 Changes
:eul.

:h1.Installation
:p.Installation is easy:
:ol.
:li.Find the OS/2 syslogd.exe in TCPIP\bin directory and rename (you figure a new name).
:li.Copy all Syslogd/2 executables to TCPIP\bin
:li.Edit the syslog.conf example for :hp3.your system:ehp3. and save in the ETC directory
as defined in the config.sys which in most cases is MPTN\ETC directory. (Thanks Dave Saville for that correction)
:li.You can start manually, place an entry in startup.cmd, or create an icon in the startup folder.
:eol.
:p.For example, I have all my options in the syslog.conf and in the first line in my startup.cmd is "detach syslogd"
 which starts Syslogd/2 in the background.

:h1.syslogd.exe
:p.This is the syslogd executable.
.br
:lines align=left.
Syslogd/2 v1.2 for OS/2  Oct 09 2005
Compiled with OpenWatcom 1.40 Michael Greene <greenemk@cox.net>

usage: syslogd [-d][-c size][-h][-k][-m mark_interval][-p][-r][-s][-t]

   -d    turn on debug information.
   -c #  truncate the log files when size reaches #. The min log file
         size is 10000 bytes. [disabled by default]
   -k    klog - process popuplog.os2 with FAC 0 and SEV 2. Initial
         check of popuplog.os2 at syslogd start and check for new
         entries every 30 minutes
   -m #  mark interval, where X is the interval (in minutes) a mark
         event is written to log files [disabled by default]
   -p    priority will be logged in file and console
   -r    disable message repeating
   -s    enable listen on port 514 for UDP messages
   -t    enable message forwarding as configured in syslog.conf
:elines.

:h2.Debug
:p.This option is disabled but can be enabled via commandline option&colon.
:p.Command Line &colon.  -d
:p.Several messages will printed and logged which are of no use to the user, be safe and leave disabled.

:h2.Truncate
:p.The truncate option will truncate the log files to a size defined by the user. This option is
 disabled by default but can be enabled via commandline option or in syslog.conf&colon.
:p.Command Line &colon.  -c #
.br
syslog.conf  &colon.  %truncate=#
:p.Where # is the max size the log file should be allowed to grow. The minumum size is 10000 bytes.

:h2.Klog
:p.The klog option is something I added and is not in the OS/2 or BSD syslogd versions.
 By default klog is disabled but can be enabled via commandline option or in syslog.conf&colon.
:p.Command Line &colon.  -k
.br
syslog.conf  &colon.  %klog
:p.When this option is enabled on syslogd startup it will scan your config.sys file looking
 for the SUPPRESSPOPUPS entry. If SUPPRESSPOPUPS is not found klog will be disabled, but if
 found syslogd will proccess the POPUPLOG.OS2 file. For example, the following is an entry
 proccessed and logged as Facility 0 (kernel)&colon.
.br
:lines align=left.
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon.  --< START POPUPLOG ENTRY >--
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. 10-28-2004  16&colon.09&colon.29  SYS3171  PID 003e  TID 001b  Slot 0054
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. C&colon.\OS2\PMSHELL.EXE
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. c0000005
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. 1e93aff3
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. P1=00000002  P2=01d3fffc  P3=XXXXXXXX  P4=XXXXXXXX
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. EAX=1b427fb0  EBX=01d4018c  ECX=00000000  EDX=00000000
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. ESI=01d40084  EDI=1b428006
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. DS=0053  DSACC=f0f3  DSLIM=ffffffff
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. ES=0053  ESACC=f0f3  ESLIM=ffffffff
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. FS=150b  FSACC=00f3  FSLIM=00000030
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. GS=0000  GSACC=****  GSLIM=********
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. CS&colon.EIP=005b&colon.1e93aff3  CSACC=f0df  CSLIM=ffffffff
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. SS&colon.ESP=0053&colon.01d40000  SSACC=f0f3  SSLIM=ffffffff
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. EBP=01d40048  FLG=00010202
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon. PMMERGE.DLL 0004&colon.0010aff3
<2>Oct 28 16&colon.12&colon.26 bigturd os2kernel&colon.  --<  END POPUPLOG ENTRY  >--
:elines.
:p.After proccessing the POPUPLOG.OS2 will be deleted and a check for new POPUPLOG.OS2 will
 be performed every 30 minutes while syslogd is running (see Appendix B - DosTimers).

:p.New: If moz_plug is set the Innotek log will also get processed.

:p.NOTE: KLOG function has a delay built in due if the messages are being forwarded. This is done
because if a large logfile will flood the receiver.

:h2.Mark Interval
:p.The mark option is left over from the old BSD code.  I guess it must have been useful for
 someone but it fills the logs up more than any thing else. By default mark is disabled but
 can be enabled via commandline option or in syslog.conf&colon.
:p.Command Line &colon.  -m #
.br
syslog.conf  &colon.  %marktimer=#
:p.Where # is the number in minutes to log a mark. The Facilities must be configured in syslog.conf
 to log a mark. If configured for a mark then the log file will get a mark entry every #
 minutes, for example&colon.
:p.<198>Oct 30 03&colon.21&colon.49 bigturd syslogd&colon. -- MARK --
:p.The mark option works but I do not use it, but if someone does and finds it needs
 correction feel free to email me.

:h2.Repeat-Compression
:p.Just as a note, Repeat-Compression was a real pain in the butt to understand and get
 working. By default Repeat-Compression is enabled and can be disabled via commandline option
 or in syslog.conf&colon.
:p.Command Line &colon.  -r
.br
syslog.conf  &colon.  %norepeat
:p.I think the best way to explain this option is by an example.  With syslogd.exe running I
 use logger.exe to send the following message 3 times&colon.
:p.logger test
.br
logger test
.br
logger test
:p.and then a different message&colon.
:p.logger test x
:p.Syslogd will log and display the following&colon.
:p.
:lines align=left.
<13>Oct 09 11&colon.33&colon.24 bigturd logger&colon. test
last message repeated 2 times
<13>Oct 09 11&colon.33&colon.30 bigturd logger&colon. test x
:elines.
:p.So, as shown if syslogd were to receive 50 identical messages then it would display the
 initial message and just count up the following 49.  The "last message repeated X times"
 will be issued when a different message is received or an internel flush is received (see
 Appendix B - DosTimers).
:note.Repeat-Compression is never enabled for forwarded messages. I expect the receiving
 syslogd to handle repeated messages on its own.
:p.If you are really reading this I will describe something I read in a syslog.org form. Here is
 the post:
:hp1.
:p.Hi,
:p.I get these log messages in my log file:
:p.Message 1
.br
Message 2
.br
Message 1
.br
Last message repeated n times
.br
Message 2
.br
Message 1
.br
Message 1
.br
Message 2
:p.Problem is that Message 1 and Message 2 get repeated too often - much more than what I need. For example, between their alternations, they get repeated 100+ times a minute.
:p.What I would like to see is something like: Message 1 repeated 50 times in the last one minute or similar.
:p.If I understand correctly, syslogd compresses last n repeated messages, but if two messages keep alternating like in the above example, this compression won't take place. Is this right? If so, is there a solution to my problem? Or a workaround?
:p.Thanks.
:ehp1.
:p.Ok, the reply was along the lines of that's a good idea... maybe we should try this
:p.Not good! Syslogd is a simple app or utility and (in my opinion) 100+ messages a minute is the senders problem.
 All I am saying is 100+ messages from 2 programs is a bit over the top, fix the problem don't just cope.
.br

:h2.Log Priority
:p.The logpriority option is an addition not available in the BSD or OS/2 versions. If enabled
 it logs the Priority number with the message entry. By default logpriority is disabled but
 can be enabled via commandline option or in syslog.conf&colon.
:p.Command Line &colon.  -p
.br
syslog.conf  &colon.  %logpriority
:p.For example, if I issue the following command&colon.
:p.logger test
:lines align=left.
:p.then without the logpriority command the log entry will be&colon.
:p.Oct 09 18&colon.07&colon.06 bigturd logger&colon. test
:p.However, with logpriority enabled the same command entry would be&colon.
:p.<13>Oct 09 18&colon.08&colon.14 bigturd logger&colon. test
:elines.
:p.I added this in case anyone wants to do log analysis and would need the original message
 Priority.

:h2.Listen Port 514
:p.The tcpiplisten option allows syslogd to monitor port 514 for incoming UDP messages. By
 default tcpiplisten is disabled but can be enabled via commandline option or in
 syslog.conf&colon.
:p.Command Line &colon.  -s
.br
syslog.conf  &colon.  %tcpiplisten
:p.By default syslogd always receives and processes IPC messages and with the tcpiplisten
 option will accept and proccess both IPC and UDP messages via a shared buffer.
.br
:note.Syslogd/2 always connects to Port 514, if this is a problem email me and I can change it.
 This option just causes UDP messages to be processed rather than dumped.
.br

:h2.Msg Forwarding
:p.The tcpipforward option allows syslogd to forward messages configured in syslog.conf. By
 default tcpipforward is disabled but can be enabled via commandline option or in
 syslog.conf&colon.
:p.Command Line &colon.  -t
.br
syslog.conf  &colon.  %tcpipforward
:note.Although a forward logging entry is configured in the syslog.conf, no
 messages will be forwarded until this option is enabled.

:h1.syslog.conf
:p.The syslog.conf is located in the MPTN\ETC directory and read at syslogd start to configure operation.
:p.As an example, I provide a sample syslog.conf below. In the following sections I will do my
best to explain how to configure Syslogd/2.
:p.:hp2.THIS IS ONLY A SAMPLE SYSLOG.CONF FILE!!!:ehp2.
:p.# *******************************************************************
.br
# Configuration file for Syslogd/2
.br
#
.br
#
.br
# Valid facility names:
.br
#  auth, authpriv, cron, daemon, ftp, kern, lpr, mail, mark,
.br
#  news, security, syslog, user, uucp, local0, local1, local2,
.br
#  local3, local4, local5, local6, local7
.br
#
.br
# Valid severity names:
.br
#   alert, crit, debug, emerg, err, error, info, none,
.br
#   notice, panic, warn, warning
.br
#
.br
# Using * for facility or severity is a wildcard for all.
.br
#
.br
# Example:
.br
# facility.severity    action
.br
#                   ^ should be separated with TAB, but spaces will
.br
#                     work - ignore error
.br
#
.br
# lines beginning with:
.br
#  # - comments
.br
#  % - alternate to starting with command line options
.br
#
.br
# actions beginning with:
.br
#  -  send to console
.br
#  =  file to log messages
.br
#  @  IP address to forward messages
.br
#
.br
#
.br
#  Allowable % commands and equivalent commandline option:
.br
#
.br
#    marktimer=#        -m #
.br
#    tcpiplisten        -s
.br
#    tcpipforward       -t
.br
#    logpriority        -p
.br
#    norepeat           -r
.br
#    klog               -k
.br
#    truncate=#         -c #
.br
#
.br
# *******************************************************************
.br
.br
%tcpiplisten
.br
%tcpipforward
.br
#%logpriority
.br
%klog
.br
.br
*.*             -CON
.br
*.*             @192.168.1.1
.br
auth.*          =d&colon.\logs\identd.log
.br
authpriv.*      =d&colon.\logs\authpriv.log
.br
cron.*          =d&colon.\logs\cron.log
.br
daemon.*        =d&colon.\logs\daemon.log
.br
ftp.*           =d&colon.\logs\ftp.log
.br
snmp.*          =d&colon.\logs\snmp.log
.br
snmptrap.*      =d&colon.\logs\snmptrap.log
.br
kern.*          =d&colon.\logs\kernel.log
.br
mail.*          =d&colon.\logs\mail.log
.br
lpr.*           =d&colon.\logs\lpr.log
.br
mail.*          =d&colon.\logs\news.log
.br
security.*      =d&colon.\logs\security.log
.br
syslog.*        =d&colon.\logs\syslog.log
.br
user.*          =d&colon.\logs\user.log
.br
uucp.*          =d&colon.\logs\uucp.log
.br
local0,local1,local2,local3,local4,local5,local6,local7.*       =d&colon.\logs\local.log
.br

:h2.Options
:p.Options are proceeded with a % symbol in the first column. The following lists the syslog.conf
command and the command line equivalent. See the command line explaination for description.

:table cols='15 5 5' rules=none frame=none.
:row.
:c.%marktimer=#
:c.-m #
:row.
:c.%tcpiplisten
:c.-s
:row.
:c.%tcpipforward
:c.-t
:row.
:c.%logpriority
:c.-p
:row.
:c.%norepeat
:c.-r
:row.
:c.%klog
:c.-k
:row.
:c.%truncate=#
:c.-c #
:etable.
:note text='NEW'. The following option can only be configured in syslog.conf:
:p.listenon&colon.lan0
.br
or
.br
listenon&colon.192.168.1.1
.br
:p.Syslogd/2 will bind and listen only on this interface. If this option is not present
the program will bind to 0.0.0.0 and listen on all interfaces.
.br

:h2.Entries




:h1.syslogctrl.exe
:p.This utility is a multi-purpose program. I used it for checking syslogd status because
 I run syslogd detached. Now it has grown to not only dump syslogd status but also to send
 commands to alter running options. Communications between syslogctrl and syslogd are via
 the syslogd named-pipe (syslogpipe.cpp module). The syslogctrl options (syslogctrl -h) are&colon.

:p.syslogctrl v1.2 for OS/2  Oct 08 2005
.br
Compiled with OpenWatcom 1.40 Michael Greene <greenemk@cox.net>
.br
syslogctrl is a utilitity for use with Syslogd/2.
.br
:p.syslogctrl [-b] [-d stats | conf] [-r] [-s] [-t #]
.br
:p.-t # truncate the log files when size reaches #. The min log file
.br
     size is 10000 bytes.
.br
-d stats | conf
.br
     stats - return the message receive and processed stats
.br
     conf  - return the current syslogd message filtering as read
.br
             from current syslog.conf
.br
-r   rotate all log files
.br
-s   send syslogd shutdown signal
.br
-b   start syslogd test sequence
.br

:h2.-d stats
:p.This option will return something like the following&colon.
:p.Current syslogd message stats&colon.
:p.Thread messages received  - UDP&colon. 1  IPC&colon. 4  Total&colon. 5
.br
Messages received by main - 5
.br
Messages processed -        5
:p.Thread overflows -          UDP&colon. 0  IPC&colon. 0  Total&colon. 0
.br
Mark messages processed -   0
:p.I used this to ensure all the messages being received were processed with no message
 buffer overflows. The "Thread messages received" line lists the number of messages received
 by UDP (syslogtcp.cpp module), by IPC (syslogipc.cpp module), and a total of all messages
 received. In the example it shows the following&colon.
:ol.
:li.One message was received by the UDP thread and 4 by the IPC thread for a total of 5 messages received.
:li.The "Messages received by main" indicates that 5 messages were received in the shared buffer by main (syslogd.cpp module).
:li.The "Messages processed" indicates that the main module processed 5 messages. This should be the same as #2 or else something is very wrong.
:li."Thread overflows" should always be 0 unless the UDP or IPC modules receive more messages than they can handle.
 At this point I do not believe overflows should happen.
:li."Mark messages processed" just count the number of MARKS processed. Remember, I left the MARK processing in
 because it is in the BSD code but I don't use it (see MARK Message). Also, at this point I can not remember if the MARKS will effect the total in #3.
:eol.
:h2.-d conf
:p.This option will return the current configuration syslogd read from syslog.conf and is using. This is a dump from my workstation&colon.
:lines align=left.
:p.Current syslog.conf filtering&colon.
FAC ==>             1 1 1 1 1 1 1 1 1 1 2 2 2 2 2
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4
-------------------------------------------------
7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 X CONSOLE&colon. CON
7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 X FORW&colon. 192.168.1.1
X X X X 7 X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\identd.log
X X X X X X X X X X 7 X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\authpriv.log
X X X X X X X X X 7 X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\cron.log
X X X 7 X X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\daemon.log
X X X X X X X X X X X 7 X X X X X X X X X X X X X FILE&colon. d&colon.\logs\ftp.log
X X X X X X X X X X X X X 7 X X X X X X X X X X X FILE&colon. d&colon.\logs\snmp.log
X X X X X X X X X X X X X X 7 X X X X X X X X X X FILE&colon. d&colon.\logs\snmptrap.log
7 X X X X X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\kernel.log
X X 7 X X X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\mail.log
X X X X X X 7 X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\lpr.log
X X 7 X X X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\news.log
X X X X 7 X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\security.log
X X X X X 7 X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\syslog.log
X 7 X X X X X X X X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\user.log
X X X X X X X X 7 X X X X X X X X X X X X X X X X FILE&colon. d&colon.\logs\uucp.log
X X X X X X X X X X X X X X X X 7 7 7 7 7 7 7 7 X FILE&colon. d&colon.\logs\local.log
:elines.
:p.The above is a bad example because it does not line up when compiled as INF file.
 Across the top are the Facility codes. Each row is a log file, console, or IP forward
 address. An X indicates nothing with the Facility number will be logged. However, the
 numbers represent a Priority mask for items to be logged. In my example above the 7
 represents all Priorities of this Facility will be logged. For example, the file
 d&colon.\logs\identd.log will contain all message Priorities with the Facility code LOG_AUTH.
 Please see the syslog.conf section and appendix A for more details.

:h2.-r
:p.This option will cause all log files to be rotated. For example, if I am logging to
 d&colon.\logs\identd.log and the -r option is issued on 10 October 2005 at 21&colon.10&colon.15&colon.
:ol.
:li.The current log file will be renamed d&colon.\logs\identd101005211015.log
:li.A new log file will be started when a new message to d&colon.\logs\identd.log is received.
:eol.

:h2.-s
:p.This option will force syslogd to do a clean shutdown. I use it when working on the source with
 syslogd running detached and I am not sure what use it would be to the normal user.

:h2.-t XXXX
:p.This option will truncate the log files to a size no greater than XXXX. The minumum size is 10000 bytes.

:h2.-b
:p.This option starts a test sequence which can be used to test Syslogd/2 operation. When
 executed the following will be displayed on the console or the syslog log file&colon.
:p.<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: Pipe received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: Main received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: IPC  received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: UDP  received BINGO
:note.The last line will only be available if tcpiplisten is enabled by the -s command line option or
 %tcpiplisten is present in syslog.conf.
:p.Please see Appendix B - Test Sequence for a detailed description.

:h1.logger.exe
:p.logger is a utility used to send custom messages to syslogd. Some of its uses include sending
syslog messages from REXX scripts and troubleshooting Syslogd/2 configuration.
:p.logger v1.2 for OS/2-ECS  Oct 16 2005
.br
Compiled with OpenWatcom 1.40 Michael Greene <greenemk@cox.net>
.br
logger is a utilitity for use with Syslogd/2.
:p.logger: [-a] [-i] [-s] [-t LogTag] [-f fac] [-p pri] [ message ... ]
:p.        -a  send method 0 - best (default)  1 - IPC  2 - UDP
.br
        -f  Facility 1 - 23
.br
        -p  Severity 0-7
.br
        -i  Logs logger PID rather than executable name
.br
        -s  echos message to console logger run from
.br
        -t <TAG>
.br
            allows user to send custom LogTag rather than
.br
            the set LogTag of logger
.br
        message ... Mesaage to send.
.br

:h2.-a
:p.This option allows the user to manually select the method of message transmission. The following
are the transmission methods&colon.
:p.0 - Best method (default)
.br
1 - IPC only
.br
2 - UDP only
:p.I use this when doing debug but I believe users could use this option to verify their configuration.
:p.Because logger uses my modified syslog.c and syslog.h it will work with any syslogd.
The best method attempts to connect IPC, if it can not connect it will transmit the message UDP.
:p.For more information see Appendix C.
.br
:h2.-f Facility
:p.Normally logger sends messages with the default Priority (PRI) of 13 which equates to a FAC of LOG_USER and SEV of LOG_NOTICE.
This option allows the user to send the message with a custom FAC (0-23), see Appendix A for a list of FACs.
.br
:h2.-p Severity
:p.Just as the -f option allowed a custom FAC, this option allows the user to use a custom SEV (0-7). See Appendix A for a list of SEVs.
.br
:h2.-i
:p.Will log the process PID in the message.
.br
:h2.-s
:p.The message, in RFC format, that is being transmitted will be echoed to the console.
.br
:h2.-t LogTag
:p.Normally logger's TAG field will be the name of the program or process that generated the message or logger.
This option allows the user to transmit a custom TAG. For example, if logger is being executed from a REXX script the user might want to
transmit the name of the REXX script as the TAG.
.br
:h2.message
:p.This is the message that will be transmitted to syslogd.
.br

:h1.Troubleshooting
:p.So you are having problems with Syslogd/2? Alright, here are some tips if it is not working:
:p.:hp2.Program X or hardware X is configured to send messages but they are not getting to Syslogd/2.:ehp2.
:p.Run the syslogctrl utility with the -b option (syslogctrl -b). See Syslogd/2 Files - syslogctrl.exe
 for an explanation of the option and Appendix B - Test Sequence for a detailed description of what is being done.
:p.If the results from the test are good then I suggest you verify that the program or hardware is configured correctly.

:p.:hp2.Messages are not being displayed to the console or are not being logged correctly.:ehp2.
:p.Make sure you have read and understand the syslog.conf options and format. Next, read Appendix A so that
 you understand syslogd message format. Last, use the logger utility to send test messages to syslogd
 and verify that they are being logged.

:p.:hp2.The "Death-Loop"....:ehp2.
:p.Yes, I have had a couple people email me that their logs were filling up with the same message
 continuously. The problem turned out to be they had syslogd listening for UDP messages on port 514
 and had an entry in syslog.conf to forward messages back to the same system. So, the first message
 received by syslogd UDP would start a continuous loop. I fixed this by checking the forward IP address
 against the current system. If they are the same I drop the message and send an internal syslog message
 informing of this drop. I think it works but the best fix is to understand the syslog.conf entries
 and do not make this mistake.
:p.One situation that I can not fix is if a syslog.conf mistake is made between 2 or more systems. For example,
 system 1 is listening on port 514 and configured to forward to system 2. System 2 is listening on port 514
 and an incorrect syslog.conf entry forwards some or all messages back to system 1. I believe you
 can see that this will cause a continuous loop between system 1 and system 2. Once again, the best fix is to understand the syslog.conf entries
 and do not make this mistake.

:h1.Appdendix A-D
:ul.
:li.Appendix A - Syslog Message Format
:li.Appendix B - Detailed description of Syslogd/2 operations
:li.Appendix C - Using syslogd with your programs
:li.Appendix D - Syslogd/2 Changes
:eul.

:h2.Appendix A - Syslog Message Format
:p.The following is a summary of what can be found in RFC 3164.
:p.A valid syslog message (or packet) consists of 3 main parts&colon.
:p.
:hp2.
.ce    PRI      HEADER      MSG
:ehp2.
:note.The maximum size of syslog message is 1024 bytes.
:p.For example, the following is a valid RFC 3164 message (note spaces and lack of spaces)&colon.
:p.<13>Oct 16 19&colon.51&colon.00 host logger&colon. test message
:p.The PRI is <13>, HEADER is Oct 16 19&colon.51&colon.00 host, and MSG is logger&colon. test message.
:p.The next three sections describe each of the parts. Please see Appendix B - Msg Processing for a detailed description of message handling.
.br
:h3.PRI
:p.The Priority (PRI) is a decimal code number which is a combination of Facility (FAC) and Severity (SEV) codes.
The Priority value is calculated by first multiplying the Facility number by 8 and then adding the numerical value of the Severity.
To be more technical, the binary FAC is shifted left 3 times and or'd with the SEV.
:p.So as a simple example using the tables below, the PRI of a message with a FAC of 11 (FTP daemon) and a SEV of 5 (Notice) would be&colon.
:p.(11 * 8) + 5 = 93
:p.So the PRI format would be <93> and the HEADER follows directly after (no space).
:p.Those Facilities that have been designated are shown in the following table along with their numerical code values.
:table cols='6 45' rules=both frame=box.
:row.
:c.Code
:c.Facility
:row.
:c.0
:c.kernel messages
:row.
:c.1
:c.user-level messages
:row.
:c.2
:c.mail system
:row.
:c.3
:c.system daemons
:row.
:c.4
:c.security/authorization messages (note 1)
:row.
:c.5
:c.messages generated internally by syslogd
:row.
:c.6
:c.line printer subsystem
:row.
:c.7
:c.network news subsystem
:row.
:c.8
:c.UUCP subsystem
:row.
:c.9
:c.clock daemon (note 2)
:row.
:c.10
:c.security/authorization messages (note 1)
:row.
:c.11
:c.FTP daemon
:row.
:c.12
:c.NTP subsystem
:row.
:c.13
:c.log audit (note 1)
:row.
:c.14
:c.log alert (note 1)
:row.
:c.15
:c.clock daemon (note 2)
:row.
:c.16
:c.local use 0  (local0)
:row.
:c.17
:c.local use 1  (local1)
:row.
:c.18
:c.local use 2  (local2)
:row.
:c.19
:c.local use 3  (local3)
:row.
:c.20
:c.local use 4  (local4)
:row.
:c.21
:c.local use 5  (local5)
:row.
:c.22
:c.local use 6  (local6)
:row.
:c.23
:c.local use 7  (local7)
:etable.
Note 1 - Various operating systems have been found to utilize Facilities 4, 10, 13 and 14 for security/authorization, audit, and alert messages which seem to be similar.
.br
Note 2 - Various operating systems have been found to utilize both Facilities 9 and 15 for clock (cron/at) messages.
:p.Each message Priority also has a decimal Severity level indicator. These are described in the following table along with their numerical values.
:table cols='6 45' rules=both frame=box.
:row.
:c.Code
:c.Severity
:row.
:c.0
:c.Emergency&colon. system is unusable
:row.
:c.1
:c.Alert&colon. action must be taken immediately
:row.
:c.2
:c.Critical&colon. critical conditions
:row.
:c.3
:c.Error&colon. error conditions
:row.
:c.4:c.Warning&colon. warning conditions
:row.
:c.5
:c.Notice&colon. normal but significant condition
:row.
:c.6
:c.Informational&colon. informational messages
:row.
:c.7
:c.Debug&colon. debug-level messages
:etable.

:h3.HEADER
:p.The HEADER part contains a timestamp and an indication of the hostname or IP address of the device. The HEADER part of the syslog packet MUST contain visible (printing) characters.
:p.The HEADER contains two fields called the TIMESTAMP and the HOSTNAME. The TIMESTAMP will immediately follow the trailing ">" from the PRI part and single space characters MUST follow each of the TIMESTAMP
and HOSTNAME fields.  HOSTNAME will contain the hostname, as it knows itself.
:p.The TIMESTAMP will be it the format - Mmm dd hh&colon.mm&colon.ss
:p.The HOSTNAME MUST NOT contain any embedded spaces.

:h3.MSG
:p.The MSG part has two fields known as the TAG field and the CONTENT field.
The value in the TAG field will be the name of the program or process that generated the message. The CONTENT contains the details of the message.
:p.The TAG is a string of alphanumeric characters that MUST NOT exceed 32 characters.
Any non-alphanumeric character will terminate the TAG field and will be assumed to be the starting character of the CONTENT field.
Most commonly, the first character of the CONTENT field that signifies the conclusion of the TAG field has been seen to be the left square
bracket character ("["), a colon character ("&colon."), or a space character.


:h2.Appendix B - Detailed description of Syslogd/2 operations
:p.
:artwork name='..\src\diagram1.bmp' align=center.
At this point I am not sure how much to put here. The graphic above (poor as it might be) shows
 the major parts of Syslogd/2. The following explains each block and the source modules that make up
 the function.
:p.The UDP block is active if syslogd is started with the -s option or %tcpiplisten is present
 in syslog.conf. UDP is started as a thread by the main process during program start. The syslogdtcp.cpp
 module is the only source for UDP.

:p.The IPC block is always active. IPC is started as a thread by the main process during program
 start. The syslogdipc.cpp module is the only source for IPC.

:p.The PIPE block is always active. PIPE is started as a thread by the main process during program
 start. The syslogpipe.cpp module is the only source for PIPE. The PIPE thread opens a named pipe
 and waits for commands from the syslogctrl.exe utility.

:p.The MAIN block is the main process that handles program start and spawns the UDP, IPC, and PIPE
 threads. The source for the MAIN block consists of syslogd.cpp, syslogbsd.cpp, and syslogpop.cpp.

:h3.Startup sequence
:p.I will give a summary of Syslogd/2 startup.

:h3.Main loop sequence
:p.Once the program is initialized the following loop will be executed until shutdown:
:ol.
:li.Set MEMMUX and wait for access to shared buffer.
:li.Access buffer and process all messages.
:li.Query SEM for syslogctrl.exe shutdown command.
:li.Query SEM for test signal (syslogctrl -b).
:li.Query SEM UDP blocked (not used - needs to be removed).
:li.Query SEM for marktimer (DosTimer1).
:li.Query SEM for flush and pop processing (DosTimer2).
:eol.

:h4.Test Sequence
:p.When the syslogctrl -b command is issued the following will happen in syslogd processes:
:ol.
:li.PIPE receives command which it considers BINGO (I don't remember why I called it this...) then
 logs "syslogd: Pipe received BINGO" message and sets PIPE SEM3.
:li.MAIN detects the SEM is set then resets the SEM and logs "syslogd: Main received BINGO" message.
:li.MAIN then opens an IPC connection to syslogdipc process and sends "<47>syslogd: IPC  received BINGO".
:li.If tcpiplisten option is true then MAIN sends "<47>syslogd: UDP  received BINGO" to the syslogdtcp process.
:li.The previous 2 messages will then be processed through syslogd. (ie the IPC and UDP threads will
 process and place in shared buffer)
:eol.
:p.If everything is working correctly the following will be displayed on the console or the syslog log file&colon.
:p.<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: Pipe received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: Main received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: IPC  received BINGO
.br
<47>Oct 15 11&colon.21&colon.59 bigturd syslogd: UDP  received BINGO
:note.The last line will only be available if tcpiplisten is enabled by the -s command line option or
 %tcpiplisten is present in syslog.conf.
:p.In general, positive result of this test will prove that Syslogd/2 is working correctly.

:h4.DosTimers
:p.The BSD code used alarms to trigger some events, so I decide to use DosTimers in Syslogd/2.
:p.:hp2.DosTimer1:ehp2. is used for the mark function. It is configured to alarm at the value given by command
 line option or in syslog.conf. When the set interval is reached, the mark function is called and
 all entries configured for mark receive a mark entry. As I stated prior to now, I do not know who
 would use this option but it is there.
:p.:hp2.DosTimer2:ehp2. is a bit more useful. It alarms every 30 minutes after syslogd is started.
At alarm, a flush is performed to clear Repeat-Compression and if klog is enabled a check
and processing of the popuplog.os2 will be performed.
.br


:h2.Appendix C - Using syslog.c with your programs
:p.This section contains some examples of how you could use Syslogd/2 with your programs. The examples include&colon.
:ol.
:li.compiling with syslog.c and syslog.h
:li.barebones UDP send
:li.barebones IPC send
:li.UDP send using REXX
:eol.

:h3.syslog.c
:p.
I do not know if any instructions need be put here. Just look up the syslog BSD directions and
compile your program with syslog.c and include syslog.h. These files are in the syslog directory
of this archive.



:h3.UDP Sample
:p.This is a barebones example of sending UDP messages without syslog.c. The source and compile version are in the samples directory.
:lines align=left.
/*

  Sample code to show a simple message being sent to Syslogd/2
  port 514 UDP with no error checking

  Syslogd/2 will format RFC3164 and log&colon.

  Oct 17 17&colon.38&colon.53 bigturd udpsample&colon. testmessage

*/

#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <netinet\in.h>
#include <unistd.h>


void main( )
{
    struct sockaddr_in testUDP;

    char TestMsg[ ] = "<13>udpsample&colon. testmessage";
    int  testsock;

    memset(&amp.testUDP, 0, sizeof(testUDP));

    testUDP.sin_len         = sizeof(testUDP);
    testUDP.sin_family      = AF_INET;
    testUDP.sin_addr.s_addr = htonl(gethostid(  ));
    testUDP.sin_port        = htons(514);

    testsock = socket(AF_INET, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &amp.testUDP, sizeof(testUDP));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);
}
:elines.

:h3.IPC Sample
:p.This is a barebones example of sending IPC messages without syslog.c. The source and compile version are in the samples directory.
:lines align=left.
/*

  Sample code to show a simple message being sent to Syslogd/2
  IPC with no error checking

  Syslogd/2 will format RFC3164 and log&colon.

  Oct 17 17&colon.38&colon.53 bigturd udpsample&colon. testmessage

*/

#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <sys\un.h>
#include <unistd.h>


void main( )
{
    struct sockaddr_un testIPC;

    char TestMsg[ ] = "<13>udpsample&colon. testmessage";
    int  testsock;

    memset(&amp.testIPC, 0, sizeof(testIPC));

    testIPC.sun_family = AF_UNIX;
    strcpy(testIPC.sun_path, "\\socket\\syslog");

    testsock = socket(AF_UNIX, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &amp.testIPC, sizeof(testIPC));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);
}
:elines.

:h3.UDP REXX Sample
:p.This is a barebones example of sending UDP messages with REXX. The source is in the samples directory.
:lines align=left.
/*
    UDP send sample for Syslogd/2

    (no error checking)
*/

call RxFuncAdd "SockLoadFuncs","rxSock","SockLoadFuncs"
call SockLoadFuncs

data = "<13>udpsample&colon. REXX testmessage"

address.family = "AF_INET"
address.port   = 514
address.addr   = INADDR_ANY


socket = SockSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)

call SockConnect socket, address.
call SockSend socket, data , "MSG_DONTROUTE"
call SockSoClose socket
:elines.

:h2.Appendix D - Syslogd/2 Changes
:p.
:hp2.Changes 1.2 (October 2005):ehp2.
:ul.
:li.added rotate and truncate commands to syslogctrl for manual execution
:li.added truncate option to syslogd which can be enabled via command line or syslog.conf file.
:li.flush timer (DosTimer2) alarms every 60 minutes instead of 30 minutes and controls flush, klog, and truncate
:li.can now send custom LogTag with logger
:li.fixed a couple of my errors which I did not find until using OW14 strl functions.
:li.found that if a LAN was not defined then syslogd trap on start, now if no LAN then try local 127.0.0.1 if error then disable UDP options.
:li.if syslog.conf log path not valid try and make
:li.clean-up logger and syslogctrl, migrate from my C++ short-hand to C
:li.changed docs to OS/2 INF format and updated
:eul.

:p.
:hp2.Changes 1.1 (Summer 2005) Not released:ehp2.
:ul.
:li.switched to OpenWatcom 1.4 using OW strl and getopt functions
:eul.

:p.
:hp2.Changes 1.0a (January 2005):ehp2.
:ul.
:li.removed tcpip attack routines, good idea in theory but not practical. User should configure firewall to prevent outside access to port 514
:li.add named pipe thread and utility program syslogctrl
:li.modified syslog.c (owmlibc.lib) and syslog.h so project compiles all executables (specific - logger.exe)
:li.better clear of message slots in syslogd.cpp to prevent trailing space growth.
:li.BSD repeat compression working - serveral fixes
:li.modified syslog.c to accept kernel messages, forwarded poplog entries were being changed to user
:li.flushing repeat messages with dostimer, mark timer defaults disabled now
:eul.

:p.
:hp2.Version 1.0 (Summer 2004):ehp2.
:ul.
:li.First version uploaded to hobbes. Yes, many problems.
:eul.



:euserdoc.
