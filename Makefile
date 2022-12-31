ttext: ttext.c
	$(CC) ttext.c -o ttext -Wall -Wextra -pedantic -std=c99 -g -fsanitize=address -fdiagnostics-color

ttext_release: ttext ttext.c
	$(CC) ttext.c -o ttext_release -Wall -Wextra -pedantic -std=c99 -Os -s

ttext_release_static: ttext_release ttext.c
	$(CC) ttext.c -o ttext_release_static -Wall -Wextra -pedantic -std=c99 -Os -s -static

release: ttext_release ttext_release_static

debug: ttext

all: debug release
.DEFAULT_GOAL := all

clean:
	rm -f ttext ttext_release ttext_release_static

run: ttext
	./ttext
