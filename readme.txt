v2.01 Changes:

1. Finally changed the DosSleep in MAIN thread to an event SEM,
   should prevent message buffer full when MAIN was idle.
   Thanks to Gabriele Gamba for the feedback.
2. Fixed missing option in -h (help) screen
3. Compiled with Open Watcom 1.6 and lxlite'd for fun.


2.0 Changes:

1. Can configure to truncate logfiles - someone asked for it
2. Can issue a command to rotate logfiles - someone asked for it
3. Configure to bind to a specific lan adapter - I did this for
   my gateway system.
4. Added a way to test the setup - lot of emails concerning "it doesn't
   work" which turned out to be operator error
5. Did INF docs - that's why this is short -read the docs :)


I could have changes on my other system - my version control sucks.


The source is included, I just waited to upload it until Open Watcom 1.4
went GA. I don't think it will compile with gcc, sorry Bird.


To build: cd into build (with OW 1.4 environment) and do a wmake.

The executables are compiled for Pent Pro (6r) with full optimization.


If you like it email me. If you hate it email me. If it screws your system -
email someone else :)

v1.2 Oct 2005
Add rotate and truncate commands, convert to C source

v1.1
Clean up and use OW14 getopt and other provided calls

v1.0
Initial working version using OW13 (Summer 2004)


