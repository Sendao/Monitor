OPTIONS = -g -rdynamic -ggdb -O0
D_FLAGS =
C_FLAGS = -Wall $(OPTIONS) $(D_FLAGS) -I../../sendao -std=c++11
L32_FLAGS = -L../../lib32 -Wall $(OPTIONS)
L64_FLAGS = -L../../lib64 -Wall $(OPTIONS)
LIBS = -lsendao


CLIOBJS = main.o pipe.o monitor.o config.o util.o control.o curler_stub.o

monitor: monitor64
	cp monitor64 monitor

monitor32: $(CLIOBJS)
	rm -f monitor32
	g++ $(L32_FLAGS) -o monitor32 $(CLIOBJS) $(LIBS)
	cp -f monitor32 ..

monitor64: $(CLIOBJS)
	rm -f monitor64
	g++ $(L_FLAGS) -o monitor64 $(CLIOBJS) $(LIBS)
	cp -f monitor64 ..

clean:
	rm -f $(CLIOBJS)
	rm -f monitor64 monitor32

.cpp.o:
	g++ -c $(C_FLAGS) $<
	
