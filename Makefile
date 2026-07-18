CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lcurl -lcrypto
BINDIR  = /usr/local/bin
MANDIR  = /usr/local/share/man/man1

SRCS    = src/main.c src/manifest.c src/sha256.c src/github.c src/interactive.c src/http.c
OBJS    = $(SRCS:.c=.o)
BIN     = mlx-pkg-submit

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all install clean
