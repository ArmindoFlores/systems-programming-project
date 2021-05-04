
make: client.c KVS-LocalServer.c
	gcc client.c -O3 -o client
	gcc KVS-LocalServer.c -O3 -o localServer

clean: 
	rm client
	rm localServer