CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall -O3
LDFLAGS :=
ARFLAGS :=

all: clean main

main: main.o
	$(LD) $(LDFLAGS) -o $(@) main.o

%.o: %.c
	$(CC) $(CFLAGS) -c $(<)

clean:
	rm -f *.o main
