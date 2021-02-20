
CC=gcc -O2
LIBS=-lmicrohttpd -lprocps -ljansson
# For Ulfius apps /usr/include/ulfius.h
# -I$(EXAMPLE_INCLUDE)
CFLAGS+=-c -Wall -I$(ULFIUS_INCLUDE)  -D_REENTRANT $(ADDITIONALFLAGS) $(CPPFLAGS)

all: libs main

libs:
	# Actually test-exe
	$(CC) -DTEST_MAIN -o proclist proclist.c proctree.c procutil.c -lprocps -ljansson `pkg-config --cflags --libs glib-2.0`
	
	#  -lprocps
	$(CC) -c proclist.c -o proclist.o `pkg-config --cflags glib-2.0`
	$(CC) -c proctree.c -o proctree.o `pkg-config --cflags glib-2.0`
	$(CC) -c procutil.c -o procutil.o `pkg-config --cflags glib-2.0`
	echo "Run test main: ./proclist"
	echo "Compile Process server: make main"
	ls -al proclist.o proctree.o
	# undefined reference to `main' w. TEST_MAIN_XX
	#$(CC) -DTEST_MAIN -o proctree proctree.c procutil.o -lprocps -ljansson `pkg-config --cflags --libs glib-2.0`
main:
	$(CC) -c procserver.c `pkg-config --cflags glib-2.0`
	#OLD:$(CC) -o procserver procserver.c proctest.o $(LIBS)
	$(CC) -o procserver procserver.o proclist.o procutil.o proctree.o $(LIBS) `pkg-config --libs glib-2.0`
	echo "Run by: ./procserver PORT (e.g. 8181)"
clean:
	rm -f procserver proclist.o proclist proctree.o procutil.o ulftest *.o
test:
	# -d "data"
	curl -X POST -H "Content-Type: application/json" --data-binary @test.json --output - http://localhost:8001/json
test2:
	curl -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary "k1=v1&k2=v2" http://localhost:8001/encoded
ulf:
	$(CC) $(CFLAGS) ulftest.c -DDEBUG -g -O0
	# $(LIBS)  -lorcania
	$(CC) -o ulftest ulftest.o -lc -lulfius
docs:
	mkdir doxydocs
	doxygen doxy.conf
