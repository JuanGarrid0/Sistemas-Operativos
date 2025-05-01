
CC = gcc

CFLAGS = -Wall -g

SRC = restaurante.c
OBJ = $(SRC)
EXEC = restaurante


all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lrt
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -lrt
clean:
	rm -f * $(EXEC)