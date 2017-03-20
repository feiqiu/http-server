SOURCE := $(wildcard *.c)

OBJECTS := $(patsubst %.c, %.o, $(SOURCE))

CC = gcc

FLAGS= -l pthread

main:$(OBJECTS)
	@ $(CC) $^ -o $@  $(FLAGS)
	
%.o:%.c
	@ $(CC) $^ -c -o $@
	
.PHONY : clean
clean:
	@ rm -rf $(OBJECTS) main