/*

  Sample code to show a simple message being sent to Syslogd/2
  IPC with no error checking

  Syslogd/2 will format RFC3164 and log:

  Oct 17 17:38:53 bigturd udpsample: testmessage

*/

#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <sys\un.h>
#include <unistd.h>


void main( )
{
    struct sockaddr_un testIPC;

    char TestMsg[ ] = "<13>udpsample: IPC testmessage";
    int  testsock;

    memset(&testIPC, 0, sizeof(testIPC));

    testIPC.sun_family = AF_UNIX;
    strcpy(testIPC.sun_path, "\\socket\\syslog");

    testsock = socket(AF_UNIX, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &testIPC, sizeof(testIPC));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);
}
