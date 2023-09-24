backup:
	cp filesystem.c backup.c

build:
	gcc -o myfs.out filesystem.c

run:
	./myfs.out test.txt

clean:
	rm -rf myfs
	rm -rf myfs.out