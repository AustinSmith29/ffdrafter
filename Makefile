all:
	gcc -Wall -g -o draftbot main.c drafter.c players.c config.c -lm