ttext: ttext.c
	$(CC) ttext.c -o ttext -Wall -Wextra -pedantic -std=c99 -g -fsanitize=address -fdiagnostics-color

ttext_release: ttext ttext.c
	$(CC) ttext.c -o ttext_release -Wall -Wextra -pedantic -std=c99 -Os -s

ttext_release_static: ttext_release ttext.c
	$(CC) ttext.c -o ttext_release_static -Wall -Wextra -pedantic -std=c99 -Os -s -static

all: ttext

release: ttext_release ttext_release_static

clean:
	rm -f ttext

run: ttext
	./ttext
