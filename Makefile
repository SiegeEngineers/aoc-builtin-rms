# Non windows only!
# Requires Wine and mingw-w64!

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

all: aoe2-builtin-rms.dll

clean:
	rm -f $(OBJECTS)

.PHONY: all clean

aoe2-builtin-rms.dll: dll.c
	$(CC) -o $@ $(FLAGS) -shared $< $(LDFLAGS)
