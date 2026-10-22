CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common -I.
LDFLAGS := -pthread -lcrypto

COMMON_OBJS := ../common/protocol.o ../common/topic_manager.o ../common/fault_injection.o ../common/session.o ../common/work_plan.o ../common/logging.o ../common/sha256_util.o
TARGETS := server client

.PHONY: all clean rebuild
all: $(TARGETS)

server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c enhanced_protocol.h ../common/work_plan.h ../common/sha256_util.h
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c enhanced_protocol.h ../common/sha256_util.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/protocol.o: ../common/protocol.c ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/topic_manager.o: ../common/topic_manager.c ../common/topic_manager.h ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/fault_injection.o: ../common/fault_injection.c ../common/fault_injection.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/session.o: ../common/session.c ../common/session.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/work_plan.o: ../common/work_plan.c ../common/work_plan.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/logging.o: ../common/logging.c ../common/logging.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/sha256_util.o: ../common/sha256_util.c ../common/sha256_util.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) server.o client.o $(COMMON_OBJS)

rebuild: clean all
