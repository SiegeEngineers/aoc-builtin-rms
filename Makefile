CC = i686-w64-mingw32-gcc

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g
BASEFLAGS = -Wall -m32
INCLUDES = -Iinclude/

ifeq ($(RELEASE),1)
  FLAGS = $(BASEFLAGS) $(OPTFLAGS)
else
  FLAGS = $(BASEFLAGS) $(DBGFLAGS)
endif

all: aoc-builtin-rms.dll
format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f aoc-builtin-rms.dll aoc-builtin-rms-api.dll ezxml.o

.PHONY: all clean format

ezxml.o: include/ezxml/ezxml.c include/ezxml/ezxml.h
	$(CC) -c -o $@ -DEZXML_NOMMAP $(FLAGS) $<

aoc-builtin-rms.dll: aoc-builtin-rms.c hook.c ezxml.o main.c
	$(CC) -o $@ $(FLAGS) $(INCLUDES) -shared $^
