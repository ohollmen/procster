# Allow declarations in the first part of (3-part) for-loop.
# For kill() and strdup() we must use _POSIX_C_SOURCE
# Use -g here for debugging, also good to have -Wall
CC=gcc -O2 -std=c99 -D_POSIX_C_SOURCE=200809
# -ldl for sym. res. support
#//lib/x86_64-linux-gnu/libpcre.so.3: error adding symbols: DSO missing from command line
#collect2: error: ld returned 1 exit status
LIBS=-lmicrohttpd -lprocps -ljansson -lgnutls -ldl -lpcre
LIBS_COMPACT=-lmicrohttpd  -ljansson -lgnutls
# NOTE: p11_kit* syms are found in libp11-kit.so, but not /usr/lib/x86_64-linux-gnu/libp11.a
# (Has  pkcs11_* syms). https://githubmemory.com/repo/p11-glue/p11-kit/issues/355
# Seems -lp11 and -lsystemd (sd_* syms) need to be retained dyn. linked. And -lc
# Seems cannot mix static libpthread and dynamic libc, so take out libpthread too
LIBS_STATIC=-lglib-2.0     -lz -lnettle -lhogweed -ltasn1      -lgmp -lidn2 -lunistring -lffi
# `pkg-config --libs glib-2.0`

# For Ulfius apps /usr/include/ulfius.h
# -I$(EXAMPLE_INCLUDE)
CFLAGS+=-c -Wall -I$(ULFIUS_INCLUDE)  -D_REENTRANT $(ADDITIONALFLAGS) $(CPPFLAGS)
LDFLAGS=-export-dynamic -rdynamic
# Ubuntu with pkg-config
GLIB_CFLAGS=`pkg-config --cflags --libs glib-2.0`
GLIB_LIBS=`pkg-config --libs glib-2.0`
MUSTACHE=./node_modules/mustache/bin/mustache
EXESUFF=
IMG_VER=0.0.3
all: libs main
all2: libs2 main2

libs:
	# Actually test-exe
	# `pkg-config --cflags --libs glib-2.0`
	#$(CC) -DTEST_MAIN -o proclist proclist.c proctree.c procutil.c -lprocps -ljansson $(GLIB_CFLAGS) $(GLIB_LIBS)
	
	#  -lprocps
	$(CC) -c proclist.c -o proclist.o `pkg-config --cflags glib-2.0`
	$(CC) -c proctree.c -o proctree.o `pkg-config --cflags glib-2.0`
	$(CC) -c procutil.c -o procutil.o `pkg-config --cflags glib-2.0`
	# main() object for CLI ( include main() by -DTEST_MAIN )
	$(CC) -DTEST_MAIN -c proclist.c -o proclist_main.o `pkg-config --cflags glib-2.0`
	echo "Run test main: ./proclist"
	echo "Compile Process server: make main"
	ls -al proclist.o proctree.o procutil.o proclist_main.o
	# undefined reference to `main' w. TEST_MAIN_XX
	# proctree functionality now in 
	#$(CC) -DTEST_MAIN -o proctree proctree.c procutil.o -lprocps -ljansson `pkg-config --cflags --libs glib-2.0`
	# CLI Utility (most sophisticated way to compile CLI based on earlier objects)
	$(CC) -o procs proclist_main.o proctree.o procutil.o -lprocps -ljansson $(GLIB_CFLAGS) $(GLIB_LIBS)
# Libs target for libproc2
libs2:
	# .o Object w/o main() -ljansson -lproc2
	gcc  -c procutil2.c -o procutil2.o `pkg-config --cflags glib-2.0`
	# TESTMAIN For CLI
	gcc -DTESTMAIN=1  -o procutil2 procutil2.c -ljansson -lproc2 `pkg-config --cflags glib-2.0` -lglib-2.0
	ls -al procutil2*
main:
	# Compile http server (procserver) file, link w. libs later
	$(CC) -c procserver.c `pkg-config --cflags glib-2.0`
	#OLD:$(CC) -o procserver procserver.c proctest.o $(LIBS)
	# Link ! $(EXESUFF)
	# Hybrid (old procserver.o w/o main and procserver2.o w. main !!!)
	#$(CC) $(LDFLAGS) -o procserver procserver.o procserver2.o proclist.o procutil.o proctree.o ms/miniserver.a $(LIBS) `pkg-config --libs glib-2.0`
	# Original slim / prurpose-build exe) w/o procserver2.o ms/miniserver.a
	# Note: redundant libs: -ldl -lpcre (libsystemd.so.0 ???). Indirect deps (Even not having them here adds them as dependency) ?
	$(CC) $(LDFLAGS) -o procserver procserver.o  proclist.o procutil.o proctree.o -lprocps $(LIBS_COMPACT) `pkg-config --libs glib-2.0`
	echo "Run (e.g.) by passing PORT: ./procserver$(EXESUFF) 8181"
# Main for libproc2
main2:
	# TODO: Pass -DPROC2 to choose the ifdef's for libproc2 based calls in procserver.c
	$(CC) -DPROC2 -c procserver.c `pkg-config --cflags glib-2.0`
	# Must have libs ... Cannot link: procutil.o
	$(CC) $(LDFLAGS) -o proc2server procserver.o  procutil2.o -lproc2 $(LIBS_COMPACT) `pkg-config --libs glib-2.0`
	echo "Run (e.g.) by passing PORT: ./proc2server$(EXESUFF) 8181"
main_fw:
	echo "This is not a stable/published compile/link target. Do NOT use."
	# New fw version. Depends on a (completely separate) framework (thus separated)
	$(CC) -c procserver2.c `pkg-config --cflags glib-2.0`
	# NOTE: Using LIBS (Not LIBS_COMPACT)
	$(CC) $(LDFLAGS) -o procserver2 procserver2.o  proclist.o procutil.o proctree.o  $(LIBS) -lminiserver `pkg-config --libs glib-2.0`
static:
	# See, README.md, Order LIBS, LIBS_STATIC accurately !
	#WRONG: $(CC) -static -o procserver_static $(LIBS_STATIC) $(LIBS) procserver.o proclist.o procutil.o proctree.o
	# Assume low-to-high from right to left (lower on right)
	# Note: hogweed has nettle symbols
	# -static (full) / -Wl,-Bstatic (for partial)
	$(CC) -Wl,-Bstatic -o procserver_static procserver.o proclist.o procutil.o proctree.o  $(LIBS) $(LIBS_STATIC) -Wl,-Bdynamic -lp11-kit -lp11 -lsystemd  -lpthread
objclean:
	# TODO: find . -type f -name "*.o" | xargs -n 1 rm
	rm -f proclist.o proctree.o procutil.o proclist_main.o procserver.o
	#rm -f  *.o
clean:
	rm -f procserver procs  ulftest ms_test *.o
test:
	# -d "data"
	#curl -X POST -H "Content-Type: application/json" --data-binary @test.json --output - http://localhost:8001/json
	curl http://localhost:8001/proclist
test2:
	curl -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary "k1=v1&k2=v2" http://localhost:8001/encoded
ulf:
	$(CC) $(CFLAGS) ulftest.c -DDEBUG -g -O0
	# $(LIBS)  -lorcania
	$(CC) -o ulftest ulftest.o -lc -lulfius
docs:
	-@mkdir -p doxydocs
	doxygen .doxygen.conf
# https://stackoverflow.com/questions/816370/how-do-you-force-a-makefile-to-rebuild-a-target
miniserver:
	#touch miniserver
	cd ms; make --always-make all
	ls -al ms/
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
image:
	docker build --rm=true -t 'procster:$(IMG_VER)' -f docker/Dockerfile .
	echo "Test: docker run --rm -p 8181:8181  --pid=host 'procster:$(IMG_VER)'"
