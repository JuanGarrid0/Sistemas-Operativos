CC=gcc
CFLAGS=-c -Wall
SOURCE=restaurante.c
OBJ=$(SOURCE:.c=.o)
EXE=restaurante

all: $(SOURCE) $(EXE)

$(EXE): $(OBJ)
        $(CC) $(OBJ) -o $@

%.o: %.c
        $(CC) $(CFLAGS) $< -o $@

clean:
        rm -rf $(OBJ) $(EXE)