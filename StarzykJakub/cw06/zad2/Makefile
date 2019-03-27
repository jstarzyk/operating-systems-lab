CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall
LDFLAGS :=
ARFLAGS :=
LIBPATH=./lib
LIB=libzad2

all: clean libzad2.so client server main

%.o: %.c $(LIBPATH)/$(LIB).h
	$(CC) $(CFLAGS) -c $(<)

libzad2.so: $(LIBPATH)/$(LIB).o
	$(CC) $(CFLAGS) -shared -o $(@) $(LIB).o

client: client.o $(LIB).so
	$(LD) $(LDFLAGS) -o $(@) client.o -Wl,-rpath . -L. -lzad2 -lrt

server: server.o $(LIB).so
	$(LD) $(LDFLAGS) -o $(@) server.o -Wl,-rpath . -L. -lzad2 -lrt
	
main: main.o
	$(CC) $(CFLAGS) -o $(@) main.o

clean:
	rm -f *.o libzad2.so client server main
