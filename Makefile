CC=gcc
CFLAGS=-c -Wall
LIBS=
INCLUDES=-I include
SRCDIR=src
BINDIR=build
OBJDIR=$(BINDIR)/obj
SOURCES=main.c parser.c list.c utility.c shell.c hashtable.c
OBJECTS=$(addprefix $(OBJDIR)/, $(SOURCES:.c=.o))
EXECUTABLE=$(BINDIR)/smsh.exe

debug: all

ifeq ($(MAKECMDGOALS), debug)
CFLAGS+=-g
endif

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(INCLUDES) $(OBJECTS) $(LIBS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm $(OBJDIR)/*.o $(EXECUTABLE)

.PHONY: clean debug



