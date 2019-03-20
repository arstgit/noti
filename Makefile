main: main.o ; gcc -o main main.o

.PHONY: clean install
clean: ; rm -f main main.o debug

install: ; cp main /usr/local/bin/noti
