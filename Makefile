CC = i686-w64-mingw32-gcc

CFLAGS = -Wall -m32
LDFLAGS = 

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

ifeq ($(RELEASE),1)
  FLAGS = $(CFLAGS) $(OPTFLAGS)
else
  FLAGS = $(CFLAGS) $(DBGFLAGS)
endif

SOURCES = $(shell echo *.c)
OBJECTS = $(SOURCES:.c=.o)

all: aoc-builtin-rms.dll

clean:
	rm -f $(OBJECTS)

.PHONY: all clean

aoc-builtin-rms.dll: aoc-builtin-rms.c
	$(CC) -o $@ $(FLAGS) -shared $< $(LDFLAGS)
