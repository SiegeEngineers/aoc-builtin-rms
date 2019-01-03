CC = i686-w64-mingw32-gcc

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

ifeq ($(RELEASE),1)
  FLAGS = $(OPTFLAGS)
else
  FLAGS = $(DBGFLAGS)
endif

all: aoc-builtin-rms.dll aoc-builtin-rms-api.dll

clean:
	rm -f aoc-builtin-rms.dll aoc-builtin-rms-api.dll

.PHONY: all clean

aoc-builtin-rms-api.dll: aoc-builtin-rms.c
	$(CC) -o $@ -Wall -m32 $(FLAGS) -shared $^
aoc-builtin-rms.dll: aoc-builtin-rms.c main.c
	$(CC) -o $@ -Wall -m32 $(FLAGS) -shared $^
