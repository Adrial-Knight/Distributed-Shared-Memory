ALL: default

CC       = gcc
CLINKER  = $(CC)
OPTFLAGS = -O0


SHELL = /bin/sh

CFLAGS  = -DREENTRANT -g
CCFLAGS = $(CFLAGS)
LIBS    = -lpthread

EXECS = common.o exemple

default: $(EXECS)

exemple: exemple.o common.o dsm.o
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-if [ ! -d "bin" ]; then mkdir bin ; fi
	@-mv $@ ../Phase1/bin/

.o:
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-mv $@ ../Phase1/bin/

clean:
	@-/bin/rm -f *.o *~ PI* $(EXECS) *.out core
