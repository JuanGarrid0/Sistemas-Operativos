
CC = gcc

CFLAGS = -Wall -g

SRC = restaurante.c
OBJ = $(SRC)
EXEC = restaurante


all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f * $(EXEC)