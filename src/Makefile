CC = gcc
CFLAGS = -Wall -Werror -g -fPIC
OBJECTS = vm-lib.so vm-sim vm-lib-ref.so vm-lib-bad.so

all: $(OBJECTS)

vm-lib.so: vm-lib.c
	$(CC) $(CFLAGS) -shared -o $@ $<

vm-lib-ref.so: vm-lib-ref.c
	$(CC) $(CFLAGS) -shared -o $@ $<

vm-lib-bad.so: vm-lib-bad.c
	$(CC) $(CFLAGS) -shared -o $@ $<

clean:
	rm -f $(OBJECTS)
