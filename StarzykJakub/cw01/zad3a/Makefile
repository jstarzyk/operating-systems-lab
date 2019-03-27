CC := gcc
LD := gcc
AR := ar
CFLAGS := -fPIC -Wall
OPTIMIZE_FLAG := -O3
LDFLAGS :=
ARFLAGS :=
LIBPATH=../zad1
TESTPATH=../zad2


all: clean s_test d_test d_test_dl
	./s_test 500 500 -d c m20 s200 a200; \
	LD_LIBRARY_PATH=$(LIBPATH) ./d_test 500 500 -d c m20 s200 a200; \
	LD_LIBRARY_PATH=$(LIBPATH) ./d_test_dl 500 500 -d c m20 s200 a200;

s_test: d_test.o $(LIBPATH)/libex1.a
	$(LD) $(LDFLAGS) -static -o $(@) d_test.o -L$(LIBPATH) -lex1

d_test: d_test.o $(LIBPATH)/libex1.so
	$(LD) $(LDFLAGS) -o $(@) d_test.o -L$(LIBPATH) -lex1

d_test_dl: d_test_dl.o d_lib_dl.o
	$(LD) $(LDFLAGS) -o $(@) d_test_dl.o d_lib_dl.o -ldl

d_test_dl.o: $(TESTPATH)/d_test.c $(LIBPATH)/d_lib.h
	$(CC) $(CFLAGS) $(OPTIMIZE_FLAG) -DDYNAMIC_LOAD=1 -c $(<) -o $(@)

d_test.o: $(TESTPATH)/d_test.c $(LIBPATH)/d_lib.h
	$(CC) $(CFLAGS) $(OPTIMIZE_FLAG) -c $(<)

clean:
	$(MAKE) OPTIMIZE_FLAG=$(OPTIMIZE_FLAG) -C $(LIBPATH)
	rm -f *.o s_test d_test d_test_dl
