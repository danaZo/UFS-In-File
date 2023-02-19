CC = gcc
FLAGS = -Wall -g
SOURCES =$(wildcard *.c)
OBJ = $(subst .c,.o , $(SOURCES))


$(info objects are $(OBJ))
$(info sources are $(SOURCES))


test: $(OBJ) 
	$(CC) $(FLAGS) -o $@ $(OBJ)

$(OBJ):  %.o : %.c blocks.h
	$(CC) -c $(FLAGS) $< -o $@


clean:
	rm *.o fs

.PHONY: clean all