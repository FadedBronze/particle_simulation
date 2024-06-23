build:
	gcc -Wall ./src/*.c -lSDL2 -lSDL2_image -lm -o game
run:
	./game
clean:
	rm game
