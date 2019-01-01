CC = i686-w64-mingw32-gcc

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

ifeq ($(RELEASE),1)
  FLAGS = $(OPTFLAGS)
else
  FLAGS = $(DBGFLAGS)
endif

all: aoc-builtin-rms.dll

clean:
	rm -f aoc-builtin-rms.dll

.PHONY: all clean

aoc-builtin-rms.dll: aoc-builtin-rms.c
	$(CC) -o $@ -Wall -m32 $(FLAGS) -shared $<
