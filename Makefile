CFLAGS = -Wall -g -pthread
LDFLAGS = -lm

.PHONY: all clean

all: run

main: *.cpp
	g++ $(CFLAGS) *.cpp -o main $(LDFLAGS)

run: main
	./main

clean:
	rm -f main