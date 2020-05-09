all: clean testp4

testp4:
	gcc -g -Wall -o testp4 block.c prog4.c testp4.c
create:
	dd if=/dev/zero of=my_dev bs=1024 count=500000
clean:
	rm -rf testp4