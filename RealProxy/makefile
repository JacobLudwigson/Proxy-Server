CC = gcc
DEBUG ?= 0

# Base flags
CFLAGS = -Wall -Wextra -pedantic -Wno-deprecated-declarations
LDFLAGS = -lpthread -lcrypto

# Add debug/asan flags if DEBUG=1
ifeq ($(DEBUG),1)
	CFLAGS += -g -fsanitize=address -O0
	LDFLAGS += -fsanitize=address
endif

# Targets
server: proxy.o dynArr.o cache.o httpUtils.o dynArr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

proxy.o: proxy.c dynArr.h cache.h httpUtils.h
	$(CC) $(CFLAGS) -c proxy.c $(LDFLAGS)

cache.o: cache.c cache.h dynArr.h
	$(CC) $(CFLAGS) -c cache.c dynArr.c

httpUtils.o: httpUtils.c httpUtils.h
	$(CC) $(CFLAGS) -c httpUtils.c

dynArr.o: dynArr.c dynArr.h
	$(CC) $(CFLAGS) -c dynArr.c



clean:
	rm -f *.o server