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
Install dependencies (e.g) on Ubuntu 18.04:
```
# For Processlister ... (procps 3.3.12, jansson 2.11)
sudo apt-get install -y libprocps6 libprocps-dev libjansson4 libjansson-dev
sudo apt-get install -y libglib2.0-dev
# For Microhttpd
sudo apt-get install -y libmicrohttpd12 libmicrohttpd-dev
```
For static linking you may additionally need:

sudo apt-get install -y libc6-dev libgnutls28-dev libpcre3-dev zlib1g-dev libsystemd-dev libp11-dev libidn2-dev libunistring-dev
libtasn1-6-dev nettle-dev libgmp-dev liblzma-dev liblz4-dev libgcrypt20-dev
libffi-dev libgpg-error-dev

... or Centos/RH (7.6):
```
# procps 3.3, jansson 2.10
sudo yum install -y procps-ng procps-ng-devel jansson jansson-devel
# TODO: glib2-static
sudo yum install -y glib2 glib2-devel
sudo yum install -y libmicrohttpd libmicrohttpd-devel
```
On any of the older distros and Linux versions with older procps (e.g. Ubuntu 14.04 has libprocps3 - 3.3.9), compile libprops
out of source code (see separate section for short instructions on this).

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
# On terminal/console (blocking)
procserver 8181
# Via systemd (See Makefile for instructions to generate unit file)
# See unit file for hints and instructions
# Once only:
sudo systemctl enable --now `pwd`/procster.service
# Later use normal systemd commands (leave out .service)
sudo systemctl restart procster
```
Test HTTP Output:
```
# As-is compact JSON
curl http://localhost:8181/procs
# Prettified JSON, Inspect with pager
curl http://localhost:8181/procs | python -m json.tool | less
```
## Installing on large number of machines

Initial install:
```
# Run copying of config JSON as template
make unit
# Edit ~/.procster/procster.conf.json for your env.
# keep documentroot to absolute path (for e.g. systemd reasons)
cat ~/.procster/procster.conf.json | ./node_modules/mustache/bin/mustache -  conf/procster.service.mustache > ./procster.service
# Install (Example)
# Inventory in ~/.procster
ansible-playbook -i ~/.procster/hosts conf/procster_install.yaml -b --extra-vars "ansible_user=mrsmith ansible_sudo_pass=s3Cr3t hosts=myhost"
# Restart
ansible-playbook -i ~/.procster/hosts conf/procster_restart.yaml -b --extra-vars "ansible_user=mrsmith ansible_sudo_pass=s3Cr3t hosts=myhost"
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

## Debugging The Procster

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
Or the "lazy way" (run from very top level, ask a backtrace after crash):
```
(gdb) run list
The program being debugged has been started already.
Start it from the beginning? (y or n) y
Starting program: /projects/ccxsw/home/ccxswbuild/procster/./procs list
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib64/libthread_db.so.1".

Program received signal SIGSEGV, Segmentation fault.
0x00007ffff7408d0b in __memcpy_ssse3_back () from /lib64/libc.so.6
(gdb) backtrace
#0  0x00007ffff7408d0b in __memcpy_ssse3_back () from /lib64/libc.so.6
#1  0x0000000000401b93 in list2str (list=0x6142d8, 
    buf=buf@entry=0x7fffffffe090 "/export/electriccloud/electriccommander/jre/bin/java -server -XX:+UseG1GC -XX:+HeapDumpOnOutOfMemoryError -XX:+AggressiveOpts -XX:+UseFastAccessorMethods -Dec.logRoot=\"logs/agent/jagent.log\" -Dcommand"...)
    at procutil.c:114
#2  0x0000000000401c49 in proc_to_json (proc=proc@entry=0x7fffffffdc80, 
    cmdline=cmdline@entry=0x7fffffffe090 "/export/electriccloud/electriccommander/jre/bin/java -server -XX:+UseG1GC -XX:+HeapDumpOnOutOfMemoryError -XX:+AggressiveOpts -XX:+UseFastAccessorMethods -Dec.logRoot=\"logs/agent/jagent.log\" -Dcommand"...)
    at procutil.c:139
#3  0x00000000004017ee in proc_list_json2 (flags=107, flags@entry=0) at proclist.c:153
#4  0x0000000000401326 in main (argc=<optimized out>, argv=<optimized out>) at proclist.c:182
```
