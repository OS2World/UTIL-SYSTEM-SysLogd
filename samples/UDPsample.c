/*

  Sample code to show a simple message being sent to Syslogd/2
  port 514 UDP with no error checking

  Syslogd/2 will format RFC3164 and log:

  Oct 17 17:38:53 bigturd udpsample: testmessage

*/

#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <netinet\in.h>
#include <unistd.h>


void main( )
{
    struct sockaddr_in testUDP;

    char TestMsg[ ] = "<13>udpsample: UDP testmessage";
    int  testsock;

    memset(&testUDP, 0, sizeof(testUDP));

    testUDP.sin_len         = sizeof(testUDP);
    testUDP.sin_family      = AF_INET;
    testUDP.sin_addr.s_addr = htonl(gethostid(  ));
    testUDP.sin_port        = htons(514);

    testsock = socket(AF_INET, SOCK_DGRAM, 0);

    (void)connect(testsock, (struct sockaddr*) &testUDP, sizeof(testUDP));
    (void)send(testsock, TestMsg, sizeof(TestMsg), MSG_DONTROUTE);

    soclose(testsock);
}
