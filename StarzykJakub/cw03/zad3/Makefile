CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall -O3
LDFLAGS :=
ARFLAGS :=

all: clean main ltest mtest

main: main.o
	$(LD) $(LDFLAGS) -o $(@) main.o

ltest: ltest.o
	$(LD) $(LDFLAGS) -o $(@) ltest.o
	
mtest: mtest.o
	$(LD) $(LDFLAGS) -o $(@) mtest.o

%.o: %.c
	$(CC) $(CFLAGS) -c $(<)

clean:
	rm -f *.o main ltest mtest
