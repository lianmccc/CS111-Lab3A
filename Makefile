#NAME: Mingchao Lian
#EMAIL: lianmccc@ucla.edu
#ID: 005348062

default:
	gcc -o lab3a -g -Wall -Wextra lab3a.c

dist:
	tar -czvf lab3a-005348062.tar.gz lab3a.c README Makefile ext2_fs.h

clean:
	rm -f lab3a lab3a-005348062.tar.gz 