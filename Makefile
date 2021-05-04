CFLAGS = -Wall -std=c99 -ggdb -fPIC
LFLAGS = -lpthread
CC = gcc
BINDIR = bin
SRCDIR = src
LIBDIR = lib
LIBNAME = libKVS

LIBFILES = src/KVS-lib.c
LSRVFILES = src/KVS-LocalServer.c
ASRVFILES = 

LIBOBJS =  $(addprefix $(BINDIR)/,$(notdir $(LIBFILES:.c=.o)))
LSRVOBJS =  $(addprefix $(BINDIR)/,$(notdir $(LSRVFILES:.c=.o)))
ASRVOBJS =  $(addprefix $(BINDIR)/,$(notdir $(ASRVFILES:.c=.o)))
HEADERS = $(wildcard $(SRCDIR)/*.h)

VPATH = main:src

lib: $(LIBDIR)/$(LIBNAME).so

app: bin/client.o $(LIBDIR)/$(LIBNAME).so
	$(CC) -o $(BINDIR)/client $< $(LIBDIR)/$(LIBNAME).so $(LFLAGS)

$(LIBDIR)/$(LIBNAME).so: $(LIBOBJS)
	$(CC) $(LIBOBJS) -shared -o $@

$(BINDIR)/%.o: %.c | $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf bin/*
	rm -rf lib/*