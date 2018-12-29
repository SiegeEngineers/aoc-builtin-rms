# Non windows only!
# Requires Wine and mingw-w64!

CC = i686-w64-mingw32-gcc

CFLAGS = -Wall -m32
LDFLAGS = 

OPTFLAGS = -O3
DBGFLAGS = -DDEBUG -g

SOURCES = $(shell echo *.c)
OBJECTS = $(SOURCES:.c=.o)

all: aoe2-builtin-rms.dll

clean:
	rm -f $(OBJECTS)

.PHONY: all clean

dll.o: dll.c
	$(CC) -c $(CFLAGS) $(OPTFLAGS) $(DBGFLAGS) $< -o $@

aoe2-builtin-rms.dll: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OPTFLAGS) $(DBGFLAGS) -shared $(OBJECTS) $(LDFLAGS)
