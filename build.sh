#!/bin/bash
# Comprehensive build script for Adaptive Healthcare Server, Client Simulator & Test Suite

set -e
echo "=================================================="
echo "  Building Adaptive Emergency Healthcare Server   "
echo "=================================================="

mkdir -p bin obj

# 1. Compile SQLite amalgamation if not yet compiled
if [ ! -f obj/sqlite3.o ]; then
    echo "[1/4] Compiling SQLite Amalgamation Engine..."
    gcc -O2 -Iinclude -c lib/sqlite3.c -o obj/sqlite3.o
else
    echo "[1/4] SQLite Amalgamation Object up to date."
fi

echo "[2/4] Compiling Core Server Modules & SQLite Database..."
gcc -Wall -Wextra -pthread -Iinclude -O2 \
    src/server.c src/thread_pool.c src/priority_queue.c src/service_handler.c src/database.c obj/sqlite3.o \
    -o bin/healthcare_server -ldl

echo "[3/4] Compiling Client Traffic Simulator & Benchmark Harness..."
gcc -Wall -Wextra -pthread -Iinclude -O2 \
    client/client_simulator.c \
    -o bin/client_simulator -ldl

echo "[4/4] Compiling Automated OS Test Suite..."
gcc -Wall -Wextra -pthread -Iinclude -O2 \
    tests/test_suite.c src/priority_queue.c src/thread_pool.c src/service_handler.c src/database.c obj/sqlite3.o \
    -o bin/run_tests -ldl

echo "=================================================="
echo "Build successful! All binaries created in bin/:"
echo "  - bin/healthcare_server"
echo "  - bin/client_simulator"
echo "  - bin/run_tests"
echo "=================================================="
