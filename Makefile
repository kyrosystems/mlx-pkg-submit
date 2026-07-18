CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -lcurl -lcrypto

SRCS = src/main.c src/manifest.c src/github.c src/http.c \
       src/interactive.c src/sha256.c src/batch.c src/init.c

OBJS = $(SRCS:.c=.o)
BIN  = mlx-pkg-submit

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)/usr/bin/$(BIN)

.PHONY: all clean install
