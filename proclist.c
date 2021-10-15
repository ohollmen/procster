/** @file
* Initial prototyping functions to create JSON process listings
* - Create raw/rudimentatry JSON by sprintf() to string using shortest processing path
* - Create ("populate") JSON (AoO) data structure using jansson library.
*
* Includes a small CLI testbench for testing output (to stdout).
* 
* # Installing Dependencies
* ... on (Ubuntu/Debian):
*
*     sudo apt-get install libpropcs-dev g++ -o proctest proctest.c -lprocps \
*       gcc -o proctest proctest.c -lprocps
*
* ## Running
* 
* Store process list to a file:
* 
*     ./proctest list > procs.json
* 
* See `proc_t` structure def in `proc/readproc.h` (/usr/include/proc/readproc.h):
* ```
* find /usr/include/ -name readproc.h | xargs -n 1 less -N
* # Very likely the /usr/include/proc/readproc.h
* ```
* 
* ## Testing
* Validate JSON output:
* ```
* ./proctest list | python -m json.tool
* ```
* ## Naming of process members in JSON
* 
* Naming comes from
* - readproc.h (struct proc_t)
* - NPM process-list
*
* ## Alternatives to printf-JSON
*
* - General: https://stackoverflow.com/questions/6673936/parsing-json-using-c
* - libjson-c3 (json-c) - Github https://github.com/json-c/json-c, APT: libjson-c3 and libjson-c-dev
* - libfastjson4 - fork of json-c (APT: libfastjson4, libfastjson-dev)
* - libjansson4 libjansson-dev
* - JSMN - https://zserge.com/jsmn.html (No APT)
* - cJSON - https://github.com/DaveGamble/cJSON
* - nxJSON - https://bitbucket.org/yarosla/nxjson (Uses in-place strings)
* - Frozen - https://github.com/cesanta/frozen (Sergey Lyubka)
*
* ## Info / References
*
* - https://stackoverflow.com/questions/49437192/libprocps-readproc-stack-smashing
* - https://stackoverflow.com/questions/939778/linux-api-to-list-running-processes
* - https://github.com/reklatsmasters/node-process-list
* - https://stackoverflow.com/questions/58885831/what-does-reaping-children-imply
* - https://idea.popcount.org/2012-12-11-linux-process-states/
* - man wait, main waitpid, man openproc, man readproc
* - https://gitlab.com/procps-ng/procps
* 
* ## Notes on ps STIME and procps start_time
* 
* See https://gitlab.com/procps-ng/procps/-/blob/master/ps/output.c
* Possibly do:
* 
*     time_t t = getbtime() + pp->start_time / Hertz;
* 
*/
#include <stdio.h>
#include <stdlib.h>
//#include <proc/readproc.h> // /usr/include/proc/readproc.h
//#include <proc/sysinfo.h> // Hertz
#include <string.h>

#include <jansson.h>
#include "proclister.h"

// #include <iostream>
// kthreadd is pid 2, all kernel threads parent to it (ppid=2)
#define IS_KTHREAD(proc)  (proc.ppid <= 2)
// using namespace std;

int arr2json(char ** arr, int cnt) { // FILE * fh ?
  if (!arr) { return 1; }
  fputs("[\n", stdout);
  int i;
  for (i=0 ; i < cnt; i++) {
    fputs(arr[i], stdout);
    if (i+1 < cnt) { fputs(",\n", stdout); }
  }
  
  fputs("]\n", stdout);
  return 0;
}
/* Default flags as
NOT: PROC_FILLENV
CONSIDER: PROC_UID (Creates empty list) PROC_FILLUSR
*/
// int flags_def = PROC_FILLMEM | PROC_FILLCOM | PROC_FILLUSR | PROC_FILLSTATUS | PROC_FILLSTAT; // PROC_UID

/** Create a process list directly from readproc() listing as JSON.
* For flags see FLAGS in `man openproc`.
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
  if (!flags) { flags = PROC_FLAGS_DEFAULT; } // Default flags (old: flags_def)
  PROCTAB * proclist = openproc(flags);
  if (!proclist) { free(jsonstr); fputs("openproc() Error\n", stderr); return NULL; } // Must free() !
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
      char * jsonstr2 = realloc(jsonstr, jlen2);
      if (!jsonstr2) { free(jsonstr); return NULL; }
      jsonstr = jsonstr2;
      fprintf(stderr, "Triggered realloc(): %lu\n", (unsigned long)jsonstr);
      jlen = jlen2;
    } // (jpos / jlen) > 0.95
  }
  closeproc(proclist);
  jpos -= 2; jsonstr[jpos] = '\0'; strncat(jsonstr, "\n]\n", 3); // jpos += 3; // <= Was: jpos Not used anymore
  //jlen = jpos; // not used
  return jsonstr;
}

/** Create Process list JSON.
* Serialization is done using Jansson JSON library by building AoO and
* serializing it. The linear list of processes can be built using single
* stack based process entry buffer.
* @param flags - Flags for openproc() (See man openproc)
* @return JSON array of (process) objects as jansson library data structure
* (has to be freed by caller)
*/
json_t * proc_list_json2(int flags) { // OLD: ret char *
  proc_t proc; // Proc Instance
  memset(&proc, 0, sizeof(proc));
  json_t *array = json_array();
  if (!flags) { flags = PROC_FLAGS_DEFAULT; } // flags_def
  PROCTAB * proclist = openproc(flags);
  if (!proclist) { fputs("openproc() Error\n", stderr); return NULL; }
  int i;
  // Things to be refined
  // OLDHERE: char starr[2] = {0, 0}; // Single letter process state
  char cmdline[2048] = {0}; //  stack smashing detected (re-terminate before populating each time)
  for (i=0 ; readproc(proclist, &proc) != NULL; i++) {
    if (IS_KTHREAD(proc)) { continue; }
    // proc.lxcname = NULL; // OLD
    proc_chn_init(&proc);
    json_t * obj = proc_to_json(&proc, cmdline);
    // See also: json_array_append_new(array, obj); // Steals value ...
    // json_array_append(array, obj); // OLD
    json_array_append_new(array, obj); // NEW
  }
  closeproc(proclist);
  return array; // TODO: Serialize at the caller (To choose compact/pretty).
  ///////////////////////////
  //size_t jflags = JSON_COMPACT; // JSON_INDENT(2); // JSON_COMPACT; // JSON_ENSURE_ASCII
  // json_dumpfd()
  //char * jsonstr = json_dumps(array, jflags);
  // Need to free array and obj:s within it ?
  //return jsonstr;
}
/** Tester utility for process JSON generation.
*/
#ifdef TEST_MAIN
int main (int argc, char ** argv) {
  if (argc < 2) {
    fprintf(stderr, "Need subcommand list or tree (cnt=%d)\n(use PROCLIST_DEBUG=1 for verbose output)\n", argc); return 1;
  }
  char * cmd = argv[1];
  // printf("Got subcommand %s\n", cmd);
  char * debugstr = getenv("PROCLIST_DEBUG");
  int debug = debugstr ? atoi(debugstr) : 0;
  size_t jflags = JSON_INDENT(2);
  char * jsonstr = NULL;
  if (!strcmp(cmd, "list")) {
    // OLD: char * jsonstr =
    json_t * array = proc_list_json2(0);
    jsonstr = json_dumps(array, jflags);
    if (!jsonstr) { printf("Failed to serialize processes (JSON list)!"); return 1; }
    // We now have i process items
    //int cnt = i;
    //arr2json(char ** arr, int cnt);
    fputs(jsonstr, stdout);
    // free(jsonstr);
    //json_decref(array);
  }
  else if (!strcmp(cmd, "tree")) {
    debug && printf("Collect raw tree of processes\n");
    proc_t * root = proc_tree();
    if (!root) { printf("Failed to collect (raw) tree of processes\n"); }
    //ptree_dump(root, 0); return 0;
    debug && printf("Populate tree of JSON process nodes\n");
    json_t * obj = ptree_json(root, 0);
    debug && printf("Serialize JSON (jansson)n");
    jsonstr = json_dumps(obj, jflags);
    if (!jsonstr) { printf("Failed to serialize processes (JSON tree)!"); return 1; }
    fputs(jsonstr, stdout);
    debug && printf("Free raw tree of process nodes\n");
    ptree_free(root, 0);
    //json_decref(obj);
  }
  if (jsonstr) { free(jsonstr); }
  if (debug) { fprintf(stderr, "Survived Free/Deref\n"); }
  //cout << "size of proc " << sizeof(proc) << "\n";
  //cout << "size of proc_info " << sizeof(proc_info) << "\n";
  return 0;  
}
#endif
