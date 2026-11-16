CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude -O2
LDFLAGS = -pthread -ldl

SRCDIR = src
OBJDIR = obj
LIBDIR = lib
CLIENTDIR = client
TESTDIR = tests

SERVER_SRCS = $(SRCDIR)/server.c $(SRCDIR)/thread_pool.c $(SRCDIR)/priority_queue.c $(SRCDIR)/service_handler.c $(SRCDIR)/database.c
SERVER_OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SERVER_SRCS)) $(OBJDIR)/sqlite3.o

CLIENT_SRCS = $(CLIENTDIR)/client_simulator.c
CLIENT_OBJS = $(OBJDIR)/client_simulator.o

SERVER_BIN = bin/healthcare_server
CLIENT_BIN = bin/client_simulator
TEST_BIN = bin/run_tests

all: $(SERVER_BIN) $(CLIENT_BIN) $(TEST_BIN)

$(OBJDIR):
	mkdir -p $(OBJDIR) bin

$(OBJDIR)/sqlite3.o: $(LIBDIR)/sqlite3.c | $(OBJDIR)
	$(CC) -O2 -Iinclude -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/client_simulator.o: $(CLIENT_SRCS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

TEST_SRCS = $(TESTDIR)/test_suite.c $(SRCDIR)/priority_queue.c $(SRCDIR)/thread_pool.c $(SRCDIR)/service_handler.c $(SRCDIR)/database.c $(OBJDIR)/sqlite3.o
$(TEST_BIN): $(TEST_SRCS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(TESTDIR)/test_suite.c $(SRCDIR)/priority_queue.c $(SRCDIR)/thread_pool.c $(SRCDIR)/service_handler.c $(SRCDIR)/database.c $(OBJDIR)/sqlite3.o -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) bin/healthcare_server bin/client_simulator bin/run_tests healthcare.db

.PHONY: all clean
