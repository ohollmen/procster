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
