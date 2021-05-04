CFLAGS = -pedantic -std=c99 -ggdb -fPIC
LFLAGS = -lpthread
CC = gcc
BINDIR = bin
SRCDIR = src
LIBDIR = lib
LIBNAME = libKVS

FILES = $(wildcard $(SRCDIR)/*.c)
OBJS =  $(addprefix $(BINDIR)/,$(notdir $(FILES:.c=.o)))
HEADERS = $(wildcard $(SRCDIR)/*.h)

VPATH = main:src

lib: $(LIBDIR)/$(LIBNAME).so

main: bin/main.o
	$(CC) -o $(BINDIR)/main $< $(LFLAGS)


.PHONY: tests
tests:
	@echo Not implemented

$(LIBDIR)/$(LIBNAME).so: $(OBJS) 
	$(CC) $(OBJS) -shared -o $@	

$(BINDIR)/%.o: %.c | $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf bin/*
	rm -rf lib/*