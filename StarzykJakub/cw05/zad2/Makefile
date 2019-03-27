CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall
LDFLAGS :=
ARFLAGS :=

all: clean main master slave

main: main.o
	$(LD) $(LDFLAGS) -o $(@) main.o

master: master.o
	$(LD) $(LDFLAGS) -o $(@) master.o

slave: slave.o
	$(LD) $(LDFLAGS) -o $(@) slave.o

%.o: %.c
	$(CC) $(CFLAGS) -c $(<)

clean:
	rm -f *.o main master slave
