CC = gcc
CFLAGS = -Wall -Werror -g -fPIC
OBJECTS = vm-lib.so vm-sim

all: $(OBJECTS)

vm-lib.so: vm-lib.c
	$(CC) $(CFLAGS) -shared -o $@ $<
    
clean:
	rm -f $(OBJECTS)
