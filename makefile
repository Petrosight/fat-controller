CC=gcc
XXXCFLAGS=-Wall -Wextra -W-pedantic
CFLAGS=-g -I. $(XXXCFLAGS)
DEPS = fatcontroller.h daemonise.h jobdispatching.h dgetopts.h sfmemlib.h subprocslog.h
OBJ = ${DEPS:.h=.o}
LIBS=-lpthread
TARGET=/usr/local/bin
INITD=/etc/init.d
ETC=/etc
RM=rm -f
MV=mv -f
CP=cp -f
CPN=cp -n

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fatcontroller: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	-${RM} fatcontroller *.o

install: fatcontroller
	@$(CP) fatcontroller $(TARGET)
	# @$(CP) ./scripts/fatcontrollerd $(INITD)
	# @if [ ! -e $(ETC)/fatcontroller ]; then \
	# 	$(CP) -f ./scripts/fatcontroller $(ETC); \
	# fi
	# @chmod 755 $(INITD)/fatcontrollerd
	@echo "The FatController has been installed!"