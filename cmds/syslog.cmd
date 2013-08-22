/* REXX: send UDP message to SYSLOGD port 514 (see also RFC 3164) */
/* Demo: normally copy procedures RXLOG and DGRAM to your script. */
/* usage: RXSYSLOG( pri, msg ) resp. as command RXSYSLOG pri msg  */

/* MSG should start with a tag like FTPD:1234: followed by text,  */
/* e.g. "FTPD:1234: FTP LOGOFF 127.0.0.1" indicating socket 1234. */

/* PRI should be a number 0 .. 7 resulting in <160> .. <167> for  */
/* local usage.  Values 8 .. 191 = 23 * 8 + 7 are also supported, */
/* but may have a specific meaning as defined in RFC 3164.  This  */
/* corresponds to 24 -1 facilities.  You can't use facility 0, it */
/* is reserved for kernel messages.  RXSYSLOG translates 0 .. 7   */
/* to priorities 160 = 20 * 8 up to 167 in facility 20 reserved   */
/* for local use (actually the 4th of local facilities 16 .. 23). */

/* OS/2 Syslogd.exe doesn't display the priority, but it could be */
/* used to forward messages to a remote system, and then it would */
/* be important to use messsage severities as defined in RFC 3164 */
/* or ?:\mptn\include\syslog.h - use 6 (166) for info messages or */
/* 7 (167) for debug messages (lower numbers are more important). */

/* Fixed bug:  Obviously OS/2 Syslogd.exe determines the HOSTNAME */
/* by itself.  Therefore it's unnecessary to include the HOSTNAME */
/* explicitly in the message.  The old code is still shown in an  */
/* unused procedure RFC3164 below.  The new procedure RXLOG sends */
/* to 127.0.0.1 without checking the HOSTNAME.  This implicitly   */
/* fixes another problem:  On an OS/2 system with dynamic IP the  */
/* result of hostid.exe or SockGetHostId() can return an old IP.  */

/* To force a valid hostid you could either use hostid.exe IP, or */
/* configure all TCP/IP dialup settings as a "primary interface". */
/* Procedure THOST (see below, not more used) expected a valid or */
/* unresolvable (offline) hostid, but failed for an invalid (old) */
/* resolvable hostid.                                             */

/* The first blank delimited word of a message should be a "tag"  */
/* as defined in RFC 3164 - or in other words end with a colon.   */
/* If the message contains no tag, then RXLOG inserts the path of */
/* the source as tag.  That's a feature if procedures RXLOG and   */
/* DGRAM are copied to another script.  The new RXLOG supports    */
/* OS/2 syslogd message length 2000 (excl. <pri>MMM DD hh:mm:ss). */

/* Start OS/2 ftpd.exe with option -l to get its syslog messages, */
/* its timestamps unfortunately differ from the RfC 3164 format.  */
/* For security reasons I use Syslogd.exe -t \dev\con displaying  */
/* syslog messages on /dev/con - YMMV.  (c) 2003, Frank Ellermann */

   signal on novalue  name TRAP  ;  signal on syntax name TRAP
   signal on failure  name TRAP  ;  signal on halt   name TRAP
   CRLF = x2c( 0D0A )            ;  EXPO = 'CRLF'

   select
      when arg() = 1    then  parse arg PRI  MSG
      when arg() = 2    then  parse arg PRI, MSG
      otherwise               exit USAGE( arg() 'arguments' )
   end
   if datatype( PRI, 'w' ) = 0 | PRI < 0 | 23 * 8 + 7 < PRI
      then                    exit USAGE( PRI '[0..191]' )

   call RXLOG PRI, MSG  ;     if result = '' then exit 0
   exit TRAP( result )           /* TRAP with RXLOG error message */

USAGE:   procedure expose (EXPO) /* show usage or error message   */
   parse source . . THIS
   TEXT = 'pri is 0..7, (0=P,A,C,E,W,N,I,D=7), 8..191'
   TEXT = 'or   :'   THIS || '( pri, tag: text )'  || CRLF || TEXT
   TEXT = 'usage:'   THIS ' pri  tag: text'        || CRLF || TEXT
   if arg( 1 ) <> '' then TEXT = 'error:' arg( 1 ) || CRLF || TEXT
   say TEXT ;  return 1

/* <URL:http://purl.net/xyzzy/src/rxsyslog.cmd>, (c) F. Ellermann */

RXLOG:   procedure expose (EXPO) /* send message to Syslog daemon */
   if RxFuncQuery( 'SockLoadFuncs' ) then do
      call RxFuncAdd 'SockLoadFuncs', 'RXSOCK', 'SockLoadFuncs'
      call SockLoadFuncs 'N'     /* TRAP if RXSOCK.DLL not found  */
   end

   parse arg PRI, MSG            /* OS/2 syslogd.exe on 127.0.0.1 */
   if right( word( MSG, 1 ), 1 ) <> ':' then do
      parse source . . MON    ;  MSG = MON || ':'  MSG
   end
   MSG = left( MSG, min( 2000, length( MSG )))

   select                        /* forcing REXX error if bad PRI */
      when 0 <= PRI & PRI < 8 then  PRI = '<' || PRI + 160 || '>'
      when PRI < 192          then  PRI = '<' || PRI + 0   || '>'
   end
   parse value date() with DAY MON .      ;  DAY = right( DAY, 2 )
   MSG = PRI || MON DAY time()      MSG   /* DAY format " D"/"DD" */

   return DGRAM( 127.0.0.1, 514, MSG, 'MSG_DONTROUTE' )

DGRAM:   procedure expose (EXPO) /* sendto host, port, msg, [flg] */
   if sign( verify( arg( 1 ), '0123456789.' )) = 0 then do
      call SockGetHostByAddr arg( 1 ), 'PEER.'
      if result = 0 then   PEER.ADDR = arg( 1 )
   end                           /* support IP without host name, */
   else do                       /* but not host name without IP: */
      call SockGetHostByName arg( 1 ), 'PEER.'
      if result = 0  then  return 'unknown' arg( 1 ) '[' h_errno ']'
   end

   PEER.PORT = arg( 2 ) ;  PEER.FAMILY = 'AF_INET'
   SOCK = SockSocket( PEER.FAMILY, 'SOCK_DGRAM', 'IPPROTO_UDP' )
   if SOCK < 0 then return 'socket error' errno

   if arg( 4, 'e' )
      then SENT = SockSendTo( SOCK, arg( 3 ), arg( 4 ),  'PEER.' )
      else SENT = SockSendTo( SOCK, arg( 3 ),            'PEER.' )

   if SENT < 0 then  SENT = 'socket error' errno   ;  else do
      SENT = length( arg( 3 )) - SENT
      if SENT > 0 then  SENT = 'lost' SENT 'bytes' ;  else SENT = ''
   end
   call SockClose SOCK  ;  return SENT

/* ----- unused code, illustrating strict RCF 3164 message format */

RFC3164: procedure expose (EXPO) /* send message to Syslog daemon */
   parse arg PRI, MSG         ;  HOST = THOST()
   if right( word( MSG, 1 ), 1 ) <> ':' then do
      parse source . . MON    ;  MSG = MON || ':'  MSG
   end                           /* RfC 3164 limit 1024 - prefix: */
   MSG = left( MSG, min( 1002 - length( HOST ), length( MSG )))

   select                        /* forcing REXX error if bad PRI */
      when 0 <= PRI & PRI < 8 then  PRI = '<' || PRI + 160 || '>'
      when PRI < 192          then  PRI = '<' || PRI + 0   || '>'
   end
   parse value date() with DAY MON .      ;  DAY = right( DAY, 2 )
   MSG = PRI || MON DAY time() HOST MSG   /* DAY format " D"/"DD" */

   return DGRAM( HOST, 514, MSG, 'MSG_DONTROUTE' )

THOST:   procedure expose (EXPO) /* get HOSTNAME of valid hostid: */
   if RxFuncQuery( 'SockLoadFuncs' ) then do
      call RxFuncAdd 'SockLoadFuncs', 'RXSOCK', 'SockLoadFuncs'
      call SockLoadFuncs 'N'     /* TRAP if RXSOCK.DLL not found  */
   end
   HOST.ADDR = SockGetHostId()   /* try to use primary IP address */
   if SockGetHostByAddr( HOST.ADDR, 'HOST.' ) then return HOST.NAME
   HOST.ADDR = '127.0.0.1'       /* try to use  localhost address */
   if SockGetHostByAddr( HOST.ADDR, 'HOST.' ) then return HOST.NAME
   HOST.NAME = strip( value( 'HOSTNAME', /**/, 'OS2ENVIRONMENT' ))
   if HOST.NAME = '' then return 'localhost' ;     return HOST.NAME

/* see <URL:http://purl.net/xyzzy/rexxtrap.htm>, (c) F. Ellermann */

TRAP:                            /* select REXX exception handler */
   call trace 'O' ;  trace N           /* don't trace interactive */
   parse source TRAP                   /* source on separate line */
   TRAP = x2c( 0D ) || right( '+++', 10 ) TRAP || x2c( 0D0A )
   TRAP = TRAP || right( '+++', 10 )   /* = standard trace prefix */
   TRAP = TRAP strip( condition( 'c' ) 'trap:' condition( 'd' ))
   select
      when wordpos( condition( 'c' ), 'ERROR FAILURE' ) > 0 then do
         if condition( 'd' ) > ''      /* need an additional line */
            then TRAP = TRAP || x2c( 0D0A ) || right( '+++', 10 )
         TRAP = TRAP '(RC' rc || ')'   /* any system error codes  */
         if condition( 'c' ) = 'FAILURE' then rc = -3
      end
      when wordpos( condition( 'c' ), 'HALT SYNTAX'   ) > 0 then do
         if condition( 'c' ) = 'HALT' then rc = 4
         if condition( 'd' ) > '' & condition( 'd' ) <> rc then do
            if condition( 'd' ) <> errortext( rc ) then do
               TRAP = TRAP || x2c( 0D0A ) || right( '+++', 10 )
               TRAP = TRAP errortext( rc )
            end                        /* future condition( 'd' ) */
         end                           /* may use errortext( rc ) */
         else  TRAP = TRAP errortext( rc )
         rc = -rc                      /* rc < 0: REXX error code */
      end
      when condition( 'c' ) = 'NOVALUE'  then rc = -2 /* dubious  */
      when condition( 'c' ) = 'NOTREADY' then rc = -1 /* dubious  */
      otherwise                        /* force non-zero whole rc */
         if datatype( value( 'RC' ), 'W' ) = 0 then rc = 1
         if rc = 0                             then rc = 1
         if condition() = '' then TRAP = TRAP arg( 1 )
   end                                 /* direct: TRAP( message ) */

   TRAP = TRAP || x2c( 0D0A ) || format( sigl, 6 )
   signal on syntax name TRAP.SIGL     /* throw syntax error 3... */
   if 0 < sigl & sigl <= sourceline()  /* if no handle for source */
      then TRAP = TRAP '*-*' strip( sourceline( sigl ))
      else TRAP = TRAP '+++ (source line unavailable)'
TRAP.SIGL:                             /* ...catch syntax error 3 */
   if abbrev( right( TRAP, 2 + 6 ), x2c( 0D0A )) then do
      TRAP = TRAP '+++ (source line unreadable)'   ;  rc = -rc
   end
   select
      when 0 then do                   /* in pipes STDERR: output */
         parse version TRAP.REXX . .   /* REXX/Personal: \dev\con */
         signal on syntax name TRAP.FAIL
         if TRAP.REXX = 'REXXSAA'      /* fails if no more handle */
            then call lineout 'STDERR'  , TRAP
            else call lineout '\dev\con', TRAP
      end
      when 0 then do                   /* OS/2 PM: RxMessageBox() */
         signal on syntax name TRAP.FAIL
         call RxMessageBox ,           /* fails if not in PMREXX  */
            translate( TRAP, ' ', x2c( 0D )), , 'CANCEL', 'WARNING'
      end                              /* replace any CR by blank */
      otherwise   say TRAP ; trace ?L  /* interactive Label trace */
   end

   if condition() = 'SIGNAL' then signal TRAP.EXIT
TRAP.CALL:  return rc                  /* continue after CALL ON  */
TRAP.FAIL:  say TRAP ;  rc = 0 - rc    /* force TRAP error output */
TRAP.EXIT:  exit   rc                  /* exit for any SIGNAL ON  */

