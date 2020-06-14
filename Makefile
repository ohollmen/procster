
CC=gcc -O2
LIBS=-lmicrohttpd -lprocps -ljansson
# For Ulfius apps /usr/include/ulfius.h
# -I$(EXAMPLE_INCLUDE)
CFLAGS+=-c -Wall -I$(ULFIUS_INCLUDE)  -D_REENTRANT $(ADDITIONALFLAGS) $(CPPFLAGS)

all: libs main

libs:
	# Actually test-exe
	$(CC) -DTEST_MAIN -o proctest proctest.c -lprocps -ljansson
	$(CC) -c proctest.c -o proctest.o -lprocps
	echo "Run test main: ./proctest"
main:
	$(CC) -o procserver procserver.c proctest.o $(LIBS)
	echo "Run by: ./procserver"
clean:
	rm procserver proctest *.o
test:
	# -d "data"
	curl -X POST -H "Content-Type: application/json" --data-binary @test.json --output - http://localhost:8001/json
test2:
	curl -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary "k1=v1&k2=v2" http://localhost:8001/encoded
ulf:
	$(CC) $(CFLAGS) ulftest.c -DDEBUG -g -O0
	# $(LIBS)  -lorcania
	$(CC) -o ulftest ulftest.o -lc -lulfius
