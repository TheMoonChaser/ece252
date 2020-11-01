# Makefile, ECE252  
# Ken Wang and George Ju

CC = gcc       # compiler
CFLAGS = -Wall -g -std=gnu99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz -lcurl -pthread # link with libz, curl-config and pthread

# For students 
LIB_UTIL = zutil.o crc.o lab_png.o catpng.o shm_stack.o
SRCS   = catpng.c lab_png.c crc.c zutil.c shm_stack.c
PASTER_OBJS   = paster2.o $(LIB_UTIL)

TARGETS= paster2 

all: ${TARGETS}

paster2: $(PASTER_OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean
clean:
	rm -f *.o $(TARGETS) 
