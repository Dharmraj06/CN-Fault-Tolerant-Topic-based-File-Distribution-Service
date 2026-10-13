# ==============================================================================
# IT305 Computer Networks Lab - Autumn 2026
# Project: Fault-Tolerant Topic-Based File Distribution Service
#
# Directory: Part1/
# Target: server
# Phase: Phase 1 (Baseline Server Build Specification)
# ==============================================================================

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -pedantic -pthread -I../common
LDFLAGS := -pthread

# Source files
SERVER_SRCS := server.c ../common/protocol.c ../common/topic_manager.c
SERVER_OBJS := server.o ../common/protocol.o ../common/topic_manager.o

# Primary target
TARGET := server

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c ../common/protocol.h ../common/topic_manager.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/protocol.o: ../common/protocol.c ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

../common/topic_manager.o: ../common/topic_manager.c ../common/topic_manager.h ../common/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) server.o ../common/protocol.o ../common/topic_manager.o

rebuild: clean all
