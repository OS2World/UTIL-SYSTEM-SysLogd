/*
    UDP send sample for Syslogd/2

    (no error checking)

*/

call RxFuncAdd "SockLoadFuncs","rxSock","SockLoadFuncs"
call SockLoadFuncs

data = "<13>udpsample: REXX testmessage"

address.family = "AF_INET"
address.port   = 514

/* put in the IP if using IF option or INADDR_ANY */
address.addr   = "192.168.1.2"


socket = SockSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)

call SockConnect socket, address.
call SockSend socket, data , "MSG_DONTROUTE"
call SockSoClose socket
