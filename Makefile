CC=g++

CFLAGS=-c -Wall

all: begin
begin: miProxy.o nameserver.o
	$(CC) miProxy.o -o miProxy/miProxy
	$(CC) nameserver.o -o nameserver/nameserver
miProxy.o: miProxy/miProxy.cpp
	$(CC) $(CFLAGS) miProxy/miProxy.cpp -std=c++11
nameserver.o: nameserver/nameserver.cpp
	$(CC) $(CFLAGS) nameserver/nameserver.cpp -std=c++11
clean:
	rm -rf *o nameserver/nameserver miProxy/miProxy
