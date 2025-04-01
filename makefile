CC = gcc
DEBUG ?= 0

# Base flags
CFLAGS = -Wno-deprecated-declarations
LDFLAGS = -lpthread -lcrypto

# Add debug/asan flags if DEBUG=1
ifeq ($(DEBUG),1)
	CFLAGS += -g -fsanitize=address -O0
	LDFLAGS += -fsanitize=address
endif

# Targets
server: proxy.o dynArr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

proxy.o: proxy.c dynArr.h
	$(CC) $(CFLAGS) -c proxy.c $(LDFLAGS)

dynArr.o: dynArr.c dynArr.h
	$(CC) $(CFLAGS) -c dynArr.c

clean:
	rm -f *.o server