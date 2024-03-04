CC=gcc
CFLAGS=-std=c11 -o2 -Isrc
SRCDIR=src
SRCS=$(wildcard $(SRCDIR)/*.c)
OBJS=$(patsubst $(SRCDIR)/%.c,%.o,$(SRCS))

ipkcpd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%.o: $(SRCDIR)/%.c $(SRCDIR)/server.h $(SRCDIR)/parser.h
	$(CC) $(CFLAGS) -c $< -o $@ -lpthread

.PHONY: clean

clean:
	rm -f $(OBJS) ipkcpd