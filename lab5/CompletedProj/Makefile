# Makefile, ECE252  
# Ken Wang and George Ju

CC = gcc       # compiler
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_LIBS = $(shell pkg-config --cflags libxml-2.0 libcurl)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_LIBS) $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g -DDEBUG1_
LD = gcc       # linker
LDFLAGS = -std=gnu99 -g 
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(shell pkg-config --libs libxml-2.0 libcurl) $(LDLIBS_XML2) $(LDLIBS_CURL) -lz -lcurl 

SRCS   = findpng2.c queue.c
FINDPNG_OBJS  = findpng2.o queue.o

TARGETS= findpng2

all: ${TARGETS}

findpng2: $(FINDPNG_OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS)
