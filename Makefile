# Non windows only!
# Requires Wine and mingw-w64!

CC = i686-w64-mingw32-gcc

CFLAGS = -Wall
LDFLAGS = 

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

SOURCES = $(shell echo *.c)
HEADERS = $(shell echo *.h)
OBJECTS = $(SOURCES:.c=.o)

all: aoe2-builtin-rms.dll

clean:
	rm -f $(OBJECTS)

.PHONY: all clean

%.o: %.c $(HEADERS)
	$(CC) -c -Wall -g -I./include $< -o $@

aoe2-builtin-rms.dll: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(DBGFLAGS) -shared $(OBJECTS) $(LDFLAGS)
