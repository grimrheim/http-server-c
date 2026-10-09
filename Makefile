CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -fsanitize=address,undefined -g -MMD -MP
TARGET = server
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
LDFLAGS = -fsanitize=address,undefined

#LIBS = -lpthread

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	./$@

build/%.o: src/%.c
		@mkdir -p build
		$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)

.PHONY: clean

clean:
		rm -rf build $(TARGET)
