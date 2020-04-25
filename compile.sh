rm -rf core* *.o Layer2/*.o exec
gcc -g -c glthreads.c -o glthreads.o
gcc -g -c graph.c -o graph.o 
gcc -g -c net.c -o net.o 
gcc -g -c utils.c -o utils.o 
gcc -g -c nwcli.c -o nwcli.o 
gcc -g -c comm.c -o comm.o
gcc -g -c Layer2/Layer2.c -o Layer2/Layer2.o
gcc -g -c topologies.c -o topologies.o
gcc -g -c test.c -o test.o 
gcc -g test.o glthreads.o graph.o topologies.o net.o comm.o utils.o nwcli.o Layer2/Layer2.o -o exec -L ./CommandParser -lcli -lpthread

