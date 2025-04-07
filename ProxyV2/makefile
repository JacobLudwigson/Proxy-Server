CC = gcc
DEBUG ?= 0

CFLAGS = -Wall -w -pedantic -Wno-deprecated-declarations
LDFLAGS = -lpthread -lcrypto

# Add debugging utils for seg faults bc I apparently cant code without overrunning every buffer Ive ever used
ifeq ($(DEBUG),1)
	CFLAGS += -g -fsanitize=address -O0
	LDFLAGS += -fsanitize=address
endif

CACHE_DIR = cache

server: proxy.o | $(CACHE_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CACHE_DIR):
	mkdir -p $(CACHE_DIR)

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c $(LDFLAGS)

clean:
	rm -f *.o server