rm -rf core* *.o exec
gcc -g -c glthreads.c -o glthreads.o
gcc -g -c graph.c -o graph.o 
gcc -g -c net.c -o net.o 
gcc -g -c nwcli.c -o nwcli.o 
gcc -g -c topologies.c -o topologies.o
gcc -g -c test.c -o test.o 
gcc -g test.o graph.o glthreads.o topologies.o net.o nwcli.o -o exec -L ./CommandParser/ -lcli

