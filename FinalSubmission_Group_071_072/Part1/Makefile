CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common
LDFLAGS := -pthread

COMMON_OBJS := ../common/protocol.o ../common/topic_manager.o ../common/file_transfer.o ../common/logging.o
TARGETS := server client

.PHONY: all clean rebuild
all: $(TARGETS)

server: server.o ../common/protocol.o ../common/topic_manager.o ../common/logging.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c ../common/protocol.h ../common/topic_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c ../common/protocol.h ../common/file_transfer.h ../common/topic_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/protocol.o: ../common/protocol.c ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/topic_manager.o: ../common/topic_manager.c ../common/topic_manager.h ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/file_transfer.o: ../common/file_transfer.c ../common/file_transfer.h ../common/protocol.h ../common/topic_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/logging.o: ../common/logging.c ../common/logging.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) server.o client.o $(COMMON_OBJS)

rebuild: clean all
