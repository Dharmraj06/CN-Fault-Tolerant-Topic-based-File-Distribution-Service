CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -pthread -I../common -I.
LDFLAGS = -pthread

COMMON_DIR = ../common

COMMON_OBJS = \
	$(COMMON_DIR)/protocol.o \
	$(COMMON_DIR)/topic_manager.o \
	$(COMMON_DIR)/fault_injection.o \
	$(COMMON_DIR)/session.o \
	$(COMMON_DIR)/work_plan.o

TARGETS = server client

.PHONY: all clean

all: $(TARGETS)

server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c enhanced_protocol.h ../common/work_plan.h
	$(CC) $(CFLAGS) -c server.c -o $@

client.o: client.c enhanced_protocol.h
	$(CC) $(CFLAGS) -c client.c -o $@

$(COMMON_DIR)/work_plan.o: ../common/work_plan.c ../common/work_plan.h
	$(CC) $(CFLAGS) -c ../common/work_plan.c -o $@

clean:
	rm -f $(TARGETS) server.o client.o $(COMMON_OBJS)
