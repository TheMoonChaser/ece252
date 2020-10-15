# Makefile, ECE252  
# Ken Wang and George Ju

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz -lcurl -pthread # link with libz, curl-config and pthread

# For students 
LIB_UTIL = zutil.o crc.o lab_png.o catpng.o 
SRCS   = catpng.c lab_png.c crc.c zutil.c
PASTER_OBJS   = paster.o $(LIB_UTIL)

TARGETS= paster 

all: ${TARGETS}

paster: $(PASTER_OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean
clean:
	rm -f *.o $(TARGETS) 
