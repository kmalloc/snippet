dummyCrash : b.cc
	g++ -g -Wall -o $@ $^ -lbreakpad_client -lpthread -L/home/miliao/libs/lib -I/home/miliao/code/breakpad-source-code/src

clean:
	rm dummyCrash *.o
