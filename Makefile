all:	client server
client: client.c
	gcc client.c -ggdb -o client -lpthread
server: server.c
	gcc server.c -ggdb -o server -lpthread

clean:
	rm client server
