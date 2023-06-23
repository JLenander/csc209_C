PORT=59212
CFLAGS= -DPORT=\$(PORT) -g -std=gnu99 -Wall -Werror

all: friend_server friendme

friend_server: friend_server.o friends.o
	gcc ${CFLAGS} -o friend_server friend_server.o friends.o

friendme: friendme.o friends.o
	gcc ${CFLAGS} -o friendme friendme.o friends.o

%.o: %.c
	gcc ${CFLAGS} -c $<

clean:
	rm -f *.o friend_server friendme
