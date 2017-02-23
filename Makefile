CC=gcc
TARGET=generate_dungeon

$(TARGET): $(TARGET).c
	@gcc -c priority_queue.c
	@gcc $(TARGET).c -o $(TARGET) priority_queue.o -lncurses -Wall -Werror -ggdb
	@echo "Made $(TARGET)"

.PHONY: clean
clean:
	@rm -rf $(TARGET) $(OBJECTS) *.o *.dSYM
	@echo "Directory cleaned."
