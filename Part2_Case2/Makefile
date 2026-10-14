# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Part 2 Case 2: In-Memory Session Resume Server and Client
# ==============================================================================

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common
LDFLAGS = -pthread

COMMON_DIR = ../common
COMMON_OBJS = $(COMMON_DIR)/protocol.o \
              $(COMMON_DIR)/topic_manager.o \
              $(COMMON_DIR)/file_transfer.o \
              $(COMMON_DIR)/fault_injection.o \
              $(COMMON_DIR)/session.o

TARGETS = server client

all: $(TARGETS)

server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c
	$(CC) $(CFLAGS) -c $< -o $@

$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c $(COMMON_DIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server client server.o client.o $(COMMON_DIR)/session.o

.PHONY: all clean
