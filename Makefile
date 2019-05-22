CC = i686-w64-mingw32-gcc

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

ifeq ($(RELEASE),1)
  FLAGS = $(OPTFLAGS)
else
  FLAGS = $(DBGFLAGS)
endif

all: aoc-builtin-rms.dll aoc-builtin-rms-api.dll
format:
	clang-format -style=file -i aoc-builtin-rms.c main.c aoc-builtin-rms.h

clean:
	rm -f aoc-builtin-rms.dll aoc-builtin-rms-api.dll ezxml.o

.PHONY: all clean format

ezxml.o: ezxml.c ezxml.h
	$(CC) -c -o $@ -DEZXML_NOMMAP -Wall -m32 $(FLAGS) ezxml.c

aoc-builtin-rms.dll: aoc-builtin-rms.c hook.c ezxml.o main.c
	$(CC) -o $@ -Wall -m32 $(FLAGS) -shared $^
