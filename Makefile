# BACKEND=BCM   bcm2835 library, Pi Zero .. Pi 4 (default)
# BACKEND=LGPIO lgpio library, Pi 5
BACKEND ?= BCM

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -D$(BACKEND)
LDFLAGS = -lm

ifeq ($(BACKEND),BCM)
LDFLAGS += -lbcm2835
endif
ifeq ($(BACKEND),LGPIO)
LDFLAGS += -llgpio
endif

SRCS = src/main.c src/config.c src/hal.c src/it8951.c src/font.c \
       src/canvas.c src/page.c src/doc.c src/keyboard.c src/compose.c src/utf8.c src/files.c src/typewriter.c
OBJS = $(SRCS:.c=.o)

typescreen: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) typescreen

.PHONY: clean test docs

# Host-side tests: no hardware, no backend.
test: tests/test.c src/page.c src/font.c src/canvas.c src/compose.c src/doc.c src/keyboard.c src/utf8.c src/files.c src/config.c src/typewriter.c
	$(CC) -Wall -Wextra -O1 -Isrc $^ -lm -o tests/test && ./tests/test && rm -rf tests/test tests/doc.txt tests/dir tests/c.conf

# API documentation: docs/html/index.html
docs:
	doxygen Doxyfile
