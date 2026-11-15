CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
OBJ = main.o fs.o disk.o

all: fs_project

fs_project: $(OBJ)
	$(CC) $(CFLAGS) -o fs_project $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o fs_project vdisk.bin