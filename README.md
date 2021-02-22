# Procster - List OS Processes over the Web

Procster is a simple microhttpd based HTTP service that lists
processes via a REST/JSON Web API.

Procster is meant to serve you in a toolkit / API manner, so that
you merely tap on to it with your favorite HTTP client and decide what
you do with data.
<!--, but Procster also ships with a small test-nature Web
GUI to graphically get hang of what data Procsster can provide.
-->

## Procster Design Rationale

Procster is written on top of 4 main, reusable footings:
- procps - Linux process lister API, also used by "ps" and "top" utilities
- microhttpd - Lightweight, standalone HTTP server
- glib - For helper data structures
- libjansson - JSON library to parse and serialize JSON

Choosing a very slim "embedded" web server (Instead of general purpose
web server like Apache or NginX) and efficient libraries for other tasks
keeps the memory footprint on the system "unnoticable".
As such Procster should be easily runnable on SBC systems like RaspberryPi,
Beaglebone, Odroid, etc.

## Procster and security

Typically listing OS processes is not someting you want to do in the world
of hacks and cracks and security breaches. This makes Procster mainly an
intranet, home or closed network tool.
See additional config possibilities of using HTTPS, authenication (HTTP
Basic Auth or API Key).

## Compiling, Running and Testing

Clone source from Github:
```
git clone https://github.com/ohollmen/procster.git
cd procster
```
Install dependencies (Example on Ubuntu 18.04)
```
# For Processlister ... (procps 3.3, jansson 2.11)
sudo apt-get install -y libprocps6 libprocps-dev libjansson4 libjansson-dev
sudo apt-get install -y libglib2.0-dev
# For Microhttpd
sudo apt-get install -y libmicrohttpd12 libmicrohttpd-dev
```
... And Centos/RH (7.6)
```
# procps 3.3, jansson 2.10
sudo yum install -y procps-ng procps-ng-devel jansson jansson-devel
# TODO: glib2-static
sudo yum install -y glib2 glib2-devel
sudo yum install -y libmicrohttpd libmicrohttpd-devel
```

Compile libraries and process server:

```
make
```
Test CLI Utility output (No HTTP involved):
```
# Compact JSON
./proctest
# Prettified JSON
./proctest | python -m json.tool | less
```
Run HTTP/REST/JSON Process server (pass port as arg):
```
procserver 8181
```
Test HTTP Output:
```
# As-is compact JSON
curl http://localhost:8181/procs
# Prettified JSON, Inspect with pager
curl http://localhost:8181/procs | python -m json.tool | less
```

## Compiling procps-ng afresh for Centos

```
# gettext gettext-libs gettext-devel gettext-common-devel
yum -y install gettext gettext-libs gettext-devel
cd /tmp
git clone https://gitlab.com/procps-ng/procps
cd procps
# Need GNU gettext/autopoint, libtool-2, libtoolize
./autogen.sh
./configure --without-ncurses
make
sudo make install
```
This places include files and libraries under /usr/local/include/proc/ and /usr/local/lib respectively.
Because of the way includes are referred (from `/proc`) subdir, the respective -I and -L options would be
-I/usr/local/include/ -L/usr/local/lib (To testrun on Centos, you may need to add export LD_LIBRARY_PATH=/usr/local/lib or
preferably add /usr/local/lib to /etc/ld.so.conf and run ldconfig). You should also uninstall procps-ng-devel from
causing ambiguity with newly installed headers from (gitlab) source package (causing wrong structure offsets,
wrong function signatures, etc.).

## Debugging (for Centos, sigh ...)

Compile (proclist CLI utility) with debug info (-g / -ggdb). Add this option into the Makfile line "CC=gcc ...".
Start debug by launching:
```
gdb ./proclist
GNU gdb (Ubuntu 8.1-0ubuntu3.2) 8.1.0.20180409-git
...
Reading symbols from ./proclist...done.
(gdb) b 176
Breakpoint 1 at 0x555555555343: file proclist.c, line 176.
(gdb) run tree
Starting program: /home/mrsmith/projects/procster/proclist
```
Example: Check what all members (and member values) the proc_t has in it for particular procps(-ng) OS distro version:
(line number examplary/approximate, try to hit the beginning of proclist.c:proc_list_json2 for loop):
```
...
(gdb) b proclist.c:152
(gdb) run
```
Example of hitting error:
```
...
139	  int ok = list2str(proc->cmdline, cmdline);
(gdb) n

Program received signal SIGSEGV, Segmentation fault.
0x00007ffff7408d0b in __memcpy_ssse3_back () from /lib64/libc.so.6
```
