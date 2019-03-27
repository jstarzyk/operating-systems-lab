CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall
OPTIMIZE_FLAG := -O3
LDFLAGS :=
ARFLAGS :=

all: clean libex1.a libex1.so

libex1.a: d_lib.o d_lib_internal.o
	$(AR) $(ARFLAGS) cr $(@) d_lib.o d_lib_internal.o

libex1.so: d_lib.o d_lib_internal.o
	$(CC) $(CFLAGS) $(OPTIMIZE_FLAG) -shared -o $(@) d_lib.o d_lib_internal.o

%.o: %.c d_lib.h
	$(CC) $(CFLAGS) $(OPTIMIZE_FLAG) -c $(<)

clean:
	rm -f *.o libex1.a libex1.so
