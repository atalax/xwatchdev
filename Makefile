
NAME = xwatchdev
CROSS =
CC = gcc
CFLAGS = -Wall -std=gnu99 -lX11 -lXi
SRC = main.c
PREFIX = /usr/local

$(NAME): $(SRC)
	$(CROSS)$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(NAME)

install: $(NAME)
	cp $(NAME) $(PREFIX)/bin/$(NAME)

uninstall:
	rm $(PREFIX)/bin/$(NAME)
