# Allow declarations in the first part of (3-part) for-loop.
# For kill() and strdup() we must use _POSIX_C_SOURCE
# Use -g here for debugging, also good to have -Wall
CC=gcc -O2 -std=c99 -D_POSIX_C_SOURCE=200809
LIBS=-lmicrohttpd -lprocps -ljansson -lgnutls
# NOTE: p11_kit* syms are found in libp11-kit.so, but not /usr/lib/x86_64-linux-gnu/libp11.a
# (Has  pkcs11_* syms). https://githubmemory.com/repo/p11-glue/p11-kit/issues/355
# Seems -lp11 and -lsystemd (sd_* syms) need to be retained dyn. linked. And -lc
# Seems cannot mix static libpthread and dynamic libc, so take out libpthread too
LIBS_STATIC=-lglib-2.0     -lz -lnettle -lhogweed -ltasn1      -lgmp -lidn2 -lunistring -lffi
# `pkg-config --libs glib-2.0`

# For Ulfius apps /usr/include/ulfius.h
# -I$(EXAMPLE_INCLUDE)
CFLAGS+=-c -Wall -I$(ULFIUS_INCLUDE)  -D_REENTRANT $(ADDITIONALFLAGS) $(CPPFLAGS)
# Ubuntu with pkg-config
GLIB_CFLAGS=`pkg-config --cflags --libs glib-2.0`
GLIB_LIBS=`pkg-config --libs glib-2.0`
MUSTACHE=./node_modules/mustache/bin/mustache
EXESUFF=
all: libs main

libs:
	# Actually test-exe
	# `pkg-config --cflags --libs glib-2.0`
	#$(CC) -DTEST_MAIN -o proclist proclist.c proctree.c procutil.c -lprocps -ljansson $(GLIB_CFLAGS) $(GLIB_LIBS)
	
	#  -lprocps
	$(CC) -c proclist.c -o proclist.o `pkg-config --cflags glib-2.0`
	$(CC) -c proctree.c -o proctree.o `pkg-config --cflags glib-2.0`
	$(CC) -c procutil.c -o procutil.o `pkg-config --cflags glib-2.0`
	$(CC) -DTEST_MAIN -c proclist.c -o proclist_main.o `pkg-config --cflags glib-2.0`
	echo "Run test main: ./proclist"
	echo "Compile Process server: make main"
	ls -al proclist.o proctree.o procutil.o proclist_main.o
	# undefined reference to `main' w. TEST_MAIN_XX
	#$(CC) -DTEST_MAIN -o proctree proctree.c procutil.o -lprocps -ljansson `pkg-config --cflags --libs glib-2.0`
	# Most sophisticated way to compile CLI based on earlier objects
	$(CC) -o procs proclist_main.o proctree.o procutil.o -lprocps -ljansson $(GLIB_CFLAGS) $(GLIB_LIBS)
main:
	$(CC) -c procserver.c `pkg-config --cflags glib-2.0`
	#OLD:$(CC) -o procserver procserver.c proctest.o $(LIBS)
	$(CC) -o procserver$(EXESUFF) procserver.o proclist.o procutil.o proctree.o $(LIBS) `pkg-config --libs glib-2.0`
	echo "Run (e.g.) by passing PORT: ./procserver$(EXESUFF) 8181"
static:
	# See, README.md, Order accurately !
	#WRONG: $(CC) -static -o procserver_static $(LIBS_STATIC) $(LIBS) procserver.o proclist.o procutil.o proctree.o
	# Assume low-to-high from right to left (lower on right)
	# Note: hogweed has nettle symbols
	# -static (full) / -Wl,-Bstatic (for partial)
	$(CC) -Wl,-Bstatic -o procserver_static procserver.o proclist.o procutil.o proctree.o  $(LIBS) $(LIBS_STATIC) -Wl,-Bdynamic -lp11-kit -lp11 -lsystemd  -lpthread
objclean:
	rm -f proclist.o proctree.o procutil.o proclist_main.o procserver.o
	#rm -f  *.o
clean:
	rm -f procserver procs  ulftest ms_test
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
	-@mkdir -p doxydocs
	doxygen .doxygen.conf
miniserver:
	$(CC) -c miniserver.c `pkg-config --cflags glib-2.0`
	$(CC) -c ms_test.c `pkg-config --cflags glib-2.0`
	$(CC) -o ms_test miniserver.o ms_test.o `pkg-config --libs glib-2.0` -ljansson -lmicrohttpd
	echo "Run server test by: ./ms_test"
unit:
	mkdir -p $(HOME)/.procster/
	# echo "No Config" && exit 1
	if [ ! -f "$(HOME)/.procster/procster.conf.json" ]; then cp ./procster.conf.json "$(HOME)/.procster/procster.conf.json" ; fi
	# Run this step and output to procster.service ( ... > ./procster.service)
	cat ~/.procster/procster.conf.json | $(MUSTACHE) -  conf/procster.service.mustache
covmodel:
	# -I/usr/include/glib-2.0/ $(GLIB_CFLAGS)
	gcc -c conf/covmodels.c `pkg-config --cflags glib-2.0`
	echo "Test Compiled Model"
# Relies on presence of "cppcheck"
cppcheck:
	cppcheck -i json_example.c -i ulftest.c .
