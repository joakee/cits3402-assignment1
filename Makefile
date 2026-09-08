CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDLIBS  =

# shared codel both the serial and (i think?) parallel implementations dep
COMMON  = pdf_header.c toy_hash.c

all: serial_implementation

serial_implementation: serial_implementation.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# uncomment once openmp_implementation.c finally exists and add it to `all` above
####openmp_implementation: openmp_implementation.c $(COMMON)
####	$(CC) $(CFLAGS) -fopenmp -o $@ $^ $(LDLIBS)

clean:
	rm -f serial_implementation openmp_implementation

.PHONY: all clean
