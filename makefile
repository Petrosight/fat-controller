CC=gcc
XXXCFLAGS=-Wall -Wextra
CFLAGS=-g -I. $(XXXCFLAGS)
DEPS=fatcontroller.h daemonise.h jobdispatching.h dgetopts.h sfmemlib.h subprocslog.h
OBJ=${DEPS:.h=.o}
LIBS=-lpthread
TARGET=/usr/local/bin
INIT=/etc/init
ETC=/etc/fatcontroller.d
EXAMPLE_CONFIG=service.fat.example
RM=rm -f
MV=mv -f
CP=cp -f
CPN=cp -n

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fatcontroller: $(OBJ)
	@if [ ! -e ./bin/ ]; then \
		mkdir -p ./bin/; \
	fi
	$(CC) -o ./bin/$@ $^ $(CFLAGS) $(LIBS)

clean:
	-${RM} fatcontroller *.o

install: fatcontroller
	@$(CP) ./bin/fatcontroller $(TARGET)
	@$(CP) ./scripts/fatcontrollerd $(TARGET)
	@if [ ! -e $(ETC) ]; then \
		mkdir -p ${ETC}; \
	fi
	
	@if [ ! -e ${ETC}/${EXAMPLE_CONFIG} ]; then \
		$(CP) -f ./scripts/${EXAMPLE_CONFIG} $(ETC); \
	fi
	
	@${CP} -f ./scripts/fatcontroller.ubuntu ${INIT}/fatcontroller.conf
	@${CP} -f ./scripts/fatcontroller-job.ubuntu ${INIT}/fatcontroller-job.conf
	
	@chmod 755 $(TARGET)/fatcontrollerd
	@echo "The FatController has been installed!"