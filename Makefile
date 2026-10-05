# Mon D.O.S. — Mini système de fichiers + Shell
# Module NSY103 — CNAM Toulouse

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -I.
LDFLAGS =

COMMON_SRC = common/errors.c
FS_SRC     = fs/disk.c fs/bitmap.c fs/inode.c fs/directory.c fs/path.c fs/fs.c
IPC_SRC    = ipc/ipc.c
KERNEL_SRC = kernel/kernel.c kernel/commands.c
SHELL_SRC  = shell/shell.c shell/parser.c

SHELL_OBJS = $(COMMON_SRC:.c=.o) $(FS_SRC:.c=.o) $(IPC_SRC:.c=.o) \
             $(KERNEL_SRC:.c=.o) $(SHELL_SRC:.c=.o)

.PHONY: all clean format run docker-build docker-run

all: bin/shell

bin:
	mkdir -p bin data

bin/shell: bin $(SHELL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SHELL_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SHELL_OBJS) bin/shell

format: bin/shell
	@mkdir -p data
	@echo "Formatage via premier montage (automatique si disque.img absent)."

run: bin/shell
	./bin/shell

docker-build:
	docker build -t mdos-nsy103 .

docker-run:
	docker run --rm -it -v "$(PWD)/data:/app/data" mdos-nsy103
