CFLAGS = -Wall -std=c99 -ggdb -fPIC
LFLAGS = -lpthread
CC = gcc
BINDIR = bin
SRCDIR = src
LIBDIR = lib
LIBNAME = libKVS

LIBFILES = src/KVS-lib.c src/list.c src/common.c
LSRVFILES = src/KVS-LocalServer.c src/common.c src/ssdict.c src/list.c 
ASRVFILES = src/authServer.c src/ssdict.c src/list.c 

LIBOBJS =  $(addprefix $(BINDIR)/,$(notdir $(LIBFILES:.c=.o)))
LSRVOBJS =  $(addprefix $(BINDIR)/,$(notdir $(LSRVFILES:.c=.o)))
ASRVOBJS =  $(addprefix $(BINDIR)/,$(notdir $(ASRVFILES:.c=.o)))
HEADERS = $(wildcard $(SRCDIR)/*.h)

VPATH = main:src

all: lib demo1 demo2 lserver aserver
	
lib: $(LIBDIR)/$(LIBNAME).so

demo1: $(BINDIR)/client.o $(LIBDIR)/$(LIBNAME).so
	$(CC) -o $(BINDIR)/client $< $(LIBDIR)/$(LIBNAME).so $(LFLAGS)

demo2: $(BINDIR)/chat_demo.o $(LIBDIR)/$(LIBNAME).so
	$(CC) -o $(BINDIR)/chat $< $(LIBDIR)/$(LIBNAME).so $(LFLAGS) -lncurses

lserver: $(LSRVOBJS)
	$(CC) -o $(BINDIR)/localserver $(LSRVOBJS) $(LFLAGS)

aserver: 
	$(CC) -o $(BINDIR)/authserver $(ASRVFILES) $(LFLAGS)

.PHONY: tests
tests:
	@echo Not Implemented

$(LIBDIR)/$(LIBNAME).so: $(LIBOBJS)
	$(CC) $(LIBOBJS) -o $@ -shared

$(BINDIR)/%.o: %.c | $(HEADERS)
	mkdir -p $(LIBDIR)
	mkdir -p $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf bin/*
	rm -rf lib/*
