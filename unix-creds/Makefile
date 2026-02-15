CFLAGS = -O2 -Wall -std=c89 -pedantic

PROGS = xuid utmpx

all:	$(PROGS)
xuid:	xuid.o show_creds.o

.PHONY:	clean
clean:; rm -f *.o *.so $(PROGS)
