all: main

main:main.c
	gcc -framework CoreAudio -framework CoreFoundation -framework AudioToolbox main.c -o main

clean:
	rm main
