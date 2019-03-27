CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall
LDFLAGS :=
ARFLAGS :=
LIBPATH=./lib
LIB=libzad1

all: clean libzad1.so client server main

%.o: %.c $(LIBPATH)/$(LIB).h
	$(CC) $(CFLAGS) -c $(<)

libzad1.so: $(LIBPATH)/$(LIB).o
	$(CC) $(CFLAGS) -shared -o $(@) $(LIB).o

client: client.o $(LIB).so
	$(LD) $(LDFLAGS) -o $(@) client.o -Wl,-rpath . -L. -lzad1

server: server.o $(LIB).so
	$(LD) $(LDFLAGS) -o $(@) server.o -Wl,-rpath . -L. -lzad1
	
main: main.o
	$(CC) $(CFLAGS) -o $(@) main.o

clean:
	rm -f *.o libzad1.so client server main
