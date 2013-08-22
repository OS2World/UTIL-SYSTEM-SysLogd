
SYSTEM= OS2
LNKSYS= os2v2

# Machine type -5r Pent -6r Pent Pro
MACHINE= -5r

CC= wcl386
LD= wlink

#Optimization None -od  Fastest possible -otexan
OPT= -otexan
#OPT= -ox

INCLUDE=$(%watcom)\h;$(%watcom)\h\os2
CFLAGS= -i=$(INCLUDE) -d0 -w4 -e25 -DHAVE_CONFIG_H -D$(SYSTEM) -zq $(OPT) $(MACHINE) -bt=$(SYSTEM) -mf
LDFLAGS= op m op maxe=25 op q op symf op el

COBJECTS= UDPsample.obj IPCsample.obj

.c: .\;..\test
.c.obj:
        $(CC) $(CFLAGS) -c $<

all: UDPsample.exe IPCsample.exe

UDPsample.exe: UDPsample.obj
  $(LD) NAME UDPsample SYS $(LNKSYS) $(LDFLAGS) FILE {UDPsample.obj}

IPCsample.exe: IPCsample.obj
  $(LD) NAME IPCsample SYS $(LNKSYS) $(LDFLAGS) FILE {IPCsample.obj}

.SILENT
clean : .SYMBOLIC
  rm *.obj
  rm *.exe
  rm *.err
  rm *.lst
  rm *.map
  rm *.lk1
  rm *.mk1
  rm *.sym
  rm *.mk





