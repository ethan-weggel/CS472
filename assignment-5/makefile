
HEADERS = udp_proto.h
CFLAGS = -g -Wall -Wno-unused-function
CC = gcc

all: du-ftp

./objs/du-proto.o: du-proto.c du-proto.h
	$(CC) $(CFLAGS) -c du-proto.c -o ./objs/du-proto.o

./objs/du-ftp.o: du-ftp.c du-ftp.h
	$(CC) $(CFLAGS) -c du-ftp.c -o ./objs/du-ftp.o

./objs/utilities.o: utilities.c utilities.h
	$(CC) $(CFLAGS) -c utilities.c -o ./objs/utilities.o

./objs/ftp-debug.o: ftp-debug.c ftp-debug.h
	$(CC) $(FLAGS) -c ftp-debug.c -o ./objs/ftp-debug.o

du-ftp: ./objs/du-ftp.o ./objs/du-proto.o ./objs/utilities.o ./objs/ftp-debug.o
	$(CC) $(CFLAGS) ./objs/du-proto.o ./objs/du-ftp.o ./objs/utilities.o ./objs/ftp-debug.o -o du-ftp

run:
	./du-ftp

clean:
	rm ./objs/* ./du-ftp