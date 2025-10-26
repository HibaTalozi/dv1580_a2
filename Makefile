CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -pthread -fPIC
LDFLAGS = -pthread
LIB = libmemory_manager.so

all: mmanager list

#Build shared library for memory manager

mmanager: memory_manager.c memory_manager.h
	@echo "Building shared library ($(LIB))..."
	$(CC) $(CFLAGS) -shared -o $(LIB) memory_manager.c
	@echo "$(LIB) built successfully! 😍"

#Build linked list app (links with memory manager)

list: linked_list.c linked_list.h test_linked_list.c
	@echo "Building thread-safe linked list..."
	$(CC) $(CFLAGS) -o test_linked_list linked_list.c test_linked_list.c -L. -lmemory_manager -lm
	@echo "test_linked_list built successfully! 😁"

#Build and run memory manager tests

test-mem: test_memory_manager.c
	@echo "Building memory manager test..."
	$(CC) $(CFLAGS) -o test_memory_manager test_memory_manager.c -L. -lmemory_manager $(LDFLAGS)
	@echo "test_memory_manager built successfully! 😎"

run-mem: test-mem
	@echo "Running memory manager test..."
	LD_LIBRARY_PATH=. ./test_memory_manager

run-list: list
	@echo "Running linked list test..."
	LD_LIBRARY_PATH=. ./test_linked_list

#  Clean up build files

clean:
	@echo "Cleaning up..."
	rm -f *.o *.so test_linked_list test_memory_manager
	@echo "Clean done! 👍"

.PHONY: all mmanager list test-mem run-mem run-list clean


