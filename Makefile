# File System 1
back:
	cp filesystem.c backup_perf.c

build:
	gcc -o myfs.out filesystem.c

run:
	./myfs.out test.txt

clean:
	rm -rf myfs
	rm -rf myfs.out
