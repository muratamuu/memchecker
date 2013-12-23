libmemcheck.so: memcheck.o
	g++ -fPIC -shared -o libmemcheck.so memcheck.o

memcheck.o: memcheck.cxx
	g++ -O2 -Wall -c memcheck.cxx

consoleapp: consoleapp.o
	gcc -o consoleapp consoleapp.o -L. -lmemcheck

consoleapp.o: consoleapp.c
	gcc -O2 -Wall -c consoleapp.c

sample: sample.o
	gcc -o sample sample.o

sample.o: sample.c
	gcc -c -O2 -Wall -c sample.c

clean:
	rm libmemcheck.so memcheck.o consoleapp consoleapp.o sample sample.o
