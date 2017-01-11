all: mush

mush: mush.c
	gccx -o mush mush.c
