/**
* 
* Depends on (Ubuntu/Debian) libpropcs-dev
* g++ -o proctest proctest.c -lprocps
* gcc -o proctest proctest.c -lprocps
* Running
* ./proctest > procs.json
* 
* See proc_t structure def in proc/readproc.h (/usr/include/proc/readproc.h)
* ```
* find /usr/include/ -name readproc.h | xargs -n 1 less -N
* # Very likely the /usr/include/proc/readproc.h
* ```

* # Testing
* ./proctest | python -m json.tool
* 
* # Naming of process members in JSON
* 
* Naming comes from
* - readproc.h (struct proc_t)
* - 
* # Alternatives to printf-JSON
*
* General: https://stackoverflow.com/questions/6673936/parsing-json-using-c

* - libjson-c3 (json-c) - Github https://github.com/json-c/json-c, APT: libjson-c3 and libjson-c-dev
* - libfastjson4 - fork of json-c (APT: libfastjson4, libfastjson-dev)
* - libjansson4 libjansson-dev
* - JSMN - https://zserge.com/jsmn.html (No APT)
* - cJSON - https://github.com/DaveGamble/cJSON
* - nxJSON - https://bitbucket.org/yarosla/nxjson (Uses in-place strings)
* - Frozen - https://github.com/cesanta/frozen (Sergey Lyubka)
*
* # Info / References
*
* - https://stackoverflow.com/questions/49437192/libprocps-readproc-stack-smashing
* - https://stackoverflow.com/questions/939778/linux-api-to-list-running-processes
* - https://github.com/reklatsmasters/node-process-list
* - https://stackoverflow.com/questions/58885831/what-does-reaping-children-imply
* - https://idea.popcount.org/2012-12-11-linux-process-states/
* - man wait, main waitpid, man openproc, man readproc
* - https://gitlab.com/procps-ng/procps
* 
* # Notes on ps STIME and procps start_time
* https://gitlab.com/procps-ng/procps/-/blob/master/ps/output.c
* Possibly do: time_t t = getbtime() + pp->start_time / Hertz;
*/
#include <stdio.h>
#include <stdlib.h>
#include <proc/readproc.h> // /usr/include/proc/readproc.h
#include <proc/sysinfo.h> // Hertz
#include <string.h>

#include <jansson.h>
// #include <iostream>
// kthreadd is pid 2, all kernel threads parent to it (ppid=2)
#define IS_KTHREAD(proc)  (proc.ppid <= 2)
// using namespace std;

int arr2json(char ** arr, int cnt) {
  fputs("[\n", stdout);
  int i;
  for (i=0 ; i < cnt; i++) {
    fputs(arr[i], stdout);
    if (i+1 < cnt) { fputs(",\n", stdout); }
  }
  
  fputs("]\n", stdout);
}
/* Default flass as
NOT: PROC_FILLENV
CONSIDER: PROC_UID (Creates empty list) PROC_FILLUSR
*/
int flags_def = PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS | PROC_FILLCOM | PROC_FILLUSR; // PROC_UID

/** Create a process list directly from readproc() listing as JSON.
* For flags see FLAGS in man openproc.
* @param flags The openproc() flags that define the extent of parsing process
* info from /proc/$PID (There are ~ 20 flags available).
* @return Dynamically allocated process listing JSON string or NULL in case of errors (error messages get written to stderr).
* @todo Make process attibutes more configurable. Possibly corralate fields to generate to flags passed as input (need lookup tables for those).
* @todo Possibly wrap response with additional JSON object to be able to convey
* status and error messages.
*/
char * proc_list_json(int flags) {
  proc_t proc; // Proc Instance
  //char * arr[1500] = {0};
  //char procstrbuf[256];
  // Needs to be done *intially* to avoid munmap_chunk(): invalid pointer.
  // Seems this is NOT needed in readproc() loop.
  memset(&proc, 0, sizeof(proc));
  int jlen = 30000; // 30000 (50 causes double free or corruption (!prev) and core dump)
  int jpos = 0;
  char * jsonstr = (char*)calloc(jlen, sizeof(char));
  if (!jsonstr) { fputs("calloc() Error\n", stderr); return NULL;}
  int i;
  strncat(jsonstr, "[\n", 2); jpos += 2;
  
  // PROC_FILLUSR
  if (!flags) { flags = flags_def; } // Default flags
  PROCTAB * proclist = openproc(flags);
  if (!proclist) { fputs("openproc() Error\n", stderr); return NULL; }
  for (i=0 ; readproc(proclist, &proc) != NULL; i++) {
    // procstrbuf
    // Kernel thread ppid == 0
    if (IS_KTHREAD(proc)) { continue; }
    jpos += sprintf(&(jsonstr[jpos]), "{\"cmd\":\"%s\", \"ppid\": %5d, \"rss\":%5ld", // "},\n",
       proc.cmd, proc.ppid, proc.rss);
    
    jpos += sprintf(&(jsonstr[jpos]), "},\n");
    // printf("Got ppid: %d\n", proc.ppid);
    //arr[i] = strdup(procstrbuf); // Dup. individual JSON string
    //if (!arr[i]) {} // Error
    // See the need to realloc()
    if ((jlen - jpos) < 10) {
      int jlen2 = 2*jlen;
      jsonstr = realloc(jsonstr, jlen2);
      fprintf(stderr, "Triggered realloc(): %lu\n", (unsigned long)jsonstr);
      jlen = jlen2;
    } // (jpos / jlen) > 0.95
  }
  closeproc(proclist);
  jpos -= 2; jsonstr[jpos] = '\0'; strncat(jsonstr, "\n]\n", 3); jpos += 3;
  return jsonstr;
}
/** Create approximate cmdline string out of char ** cmdline.
* Approximate means no quoting or escaping is done to turn char ** items
* back into *real* runnable properly formatted  commandline string.
* Approximate is good enough for this process listing usage.
* @param list Array of Strings
* @param buf String buffer where Array items are serialized space-separated (must contain enough space for )
* @return true for serialization being done, 0 for list being NUUL (no serialization *can* be done).
*/
int list2str(char ** list, char buf[]) {
  int pos = 0;
  int len = 0;
  if (!list) {  return 0; } // printf("No list\n");
  for (;*list;list++) {
    len=strlen(*list);
    // printf("Len: %d\n", len);
    memcpy(&(buf[pos]), *list, len);
    pos += len;
    buf[pos] = ' ';
    pos++;
    
  }
  buf[pos] = '\0';
  return 1;
}

/** Create Process list JSON.
* Serialization is done using Jansson JSON library by building AoO and
* serializing it.
* @param flags - Flags for openproc() (See man openproc)
* @return JSON array of (process) objects as jansson library data structure.
*/
char * proc_list_json2(int flags) {
  proc_t proc; // Proc Instance
  memset(&proc, 0, sizeof(proc));
  json_t *array = json_array();
  if (!flags) { flags = flags_def; }
  PROCTAB * proclist = openproc(flags);
  if (!proclist) { fputs("openproc() Error\n", stderr); return NULL; }
  int i;
  printf("Hertz=%Ld\n", Hertz);
  // Things to be refined
  char starr[2] = {0, 0}; // Single letter process state
  char cmdline[2048] = {0}; //  stack smashing detected (re-terminate before populating each time)
  for (i=0 ; readproc(proclist, &proc) != NULL; i++) {
    if (IS_KTHREAD(proc)) { continue; }
    //json_t * proc_to_json(proc_t * proc) {
    json_t *obj = json_object();
    json_object_set(obj, "pid", json_integer(proc.tid));
    json_object_set(obj, "cmd",  json_string(proc.cmd)); // bn of executable
    // char ** cmdline (/proc/$PID/cmdline is null-byte delimited version
    cmdline[0] = '\0';
    int ok = list2str(proc.cmdline, cmdline);
    // printf("CMDLINE:%s\n", cmdline);
    if (ok) { json_object_set(obj, "cmdline",  json_string(cmdline)); }
    json_object_set(obj, "ppid", json_integer(proc.ppid));
    json_object_set(obj, "rss",  json_integer(proc.rss));
    // Times: start_time (should be seconds after 1970-1-1, but rolls on!)
    json_object_set(obj, "starttime", json_integer(proc.start_time));
    
    json_object_set(obj, "utime", json_integer(proc.utime));
    json_object_set(obj, "stime", json_integer(proc.stime));
    // Also cutime, cstime for cumulated
    //json_object_set(obj, "utime", json_integer(proc.cutime));
    //json_object_set(obj, "stime", json_integer(proc.cstime));
    // User info ruser[P_G_SZ] - Only populated w. flag PROC_FILLUSR
    json_object_set(obj, "owner",  json_string(proc.ruser));
    
    // State: CPU/ processor, Process state (S=sleeping)
    json_object_set(obj, "cpuid", json_integer(proc.processor));
    //state, R=Running (or runnable), S=sleeping (Interruptable sleep), Z=Defunct/Zombie, D=Uninterruptable sleep (usually IO)
    // T: Stopped (e.g. ctrl-Z), I=???(On kernel threads)
    starr[0] = proc.state;
    json_object_set(obj, "state",  json_string(starr));
    //}
    // See also: json_array_append_new(array, obj); // Steals value ...
    json_array_append(array, obj);
  }
  closeproc(proclist);
  size_t jflags = JSON_COMPACT; // JSON_INDENT(2); // JSON_COMPACT; // JSON_ENSURE_ASCII
  // json_dumpfd()
  char * json = json_dumps(array, jflags);
  // Need to free array and obj:s within it ?
  return json;
}
/** Tester utility for process JSON generation.
*/
#ifdef TEST_MAIN
int main () {
  char * jsonstr = proc_list_json2(0);
  if (!jsonstr) { printf("Failed to get processes !"); return 1; }
  // We now have i process items
  //int cnt = i;
  //arr2json(char ** arr, int cnt);
  fputs(jsonstr, stdout);
  free(jsonstr);
  //cout << "size of proc " << sizeof(proc) << "\n";
  //cout << "size of proc_info " << sizeof(proc_info) << "\n";

  

  return 0;  
}
#endif