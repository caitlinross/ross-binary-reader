all: reader.c
	gcc -Wall -g -DBGQ -o ross-reader reader.c
