CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common
LDFLAGS := -pthread

COMMON_OBJS := ../common/protocol.o ../common/topic_manager.o ../common/file_transfer.o ../common/fault_injection.o ../common/session.o ../common/logging.o
TARGETS := server client

.PHONY: all clean rebuild
all: $(TARGETS)

server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c ../common/protocol.h ../common/topic_manager.h ../common/fault_injection.h ../common/session.h
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c ../common/protocol.h ../common/file_transfer.h ../common/session.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/protocol.o: ../common/protocol.c ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/topic_manager.o: ../common/topic_manager.c ../common/topic_manager.h ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/file_transfer.o: ../common/file_transfer.c ../common/file_transfer.h ../common/protocol.h ../common/topic_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/fault_injection.o: ../common/fault_injection.c ../common/fault_injection.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/session.o: ../common/session.c ../common/session.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/logging.o: ../common/logging.c ../common/logging.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) server.o client.o ../common/session.o ../common/logging.o

rebuild: clean all
