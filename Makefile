CC=gcc
CFLAGS=-g
TARGET:test.exe
LIBS=-lpthread -L ./CommandParser -lcli
OBJS=		  glthreads.o \
		  graph.o 		   \
		  topologies.o	   \
		  net.o			   \
		  comm.o		   \
		  Layer2/Layer2.o  \
		  nwcli.o		   

test.exe:test.o ${OBJS} CommandParser/libcli.a
	${CC} ${CFLAGS} test.o ${OBJS} -o test.exe ${LIBS}

test.o:test.c
	${CC} ${CFLAGS} -c test.c -o test.o

glthreads.o:glthreads.c
	${CC} ${CFLAGS} -c glthreads.c -o glthreads.o

graph.o:graph.c
	${CC} ${CFLAGS} -c -I . graph.c -o graph.o

topologies.o:topologies.c
	${CC} ${CFLAGS} -c -I . topologies.c -o topologies.o

net.o:net.c
	${CC} ${CFLAGS} -c -I . net.c -o net.o

comm.o:comm.c
	${CC} ${CFLAGS} -c -I . comm.c -o comm.o

Layer2/Layer2.o:Layer2/Layer2.c
	${CC} ${CFLAGS} -c -I . Layer2/Layer2.c -o Layer2/Layer2.o

nwcli.o:nwcli.c
	${CC} ${CFLAGS} -c -I . nwcli.c  -o nwcli.o

utils.o:utils.c
	${CC} ${CFLAGS} -c -I . utils.c -o utils.o

CommandParser/libcli.a:
	(cd CommandParser; make)
clean:
	rm *.o
	rm glthreads.o
	rm *exe
	rm Layer2/*.o

all:
	make
	(cd CommandParser; make)

cleanall:
	make clean
	(cd CommandParser; make clean)
