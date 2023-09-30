# File System 
back:
	cp filesystem.c backup.c

build:
	gcc -o myfs.out filesystem.c

run:
	./myfs.out sampleinput.txt

clean:
	rm -rf myfs
	rm -rf myfs.out
