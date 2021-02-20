/**
* @file
* # Misc Process utilities (not related to process lists or trees).
* 
*/
#include <stdio.h>
// kill(2) (Only applicable to procutil.c)
#include <sys/types.h>
#include <signal.h>

#include <errno.h> // errno
#include <string.h> // strerror()
#include <proc/readproc.h>
#include <proc/sysinfo.h> // Hertz, getbtime


#include <jansson.h>

#include "proclister.h"
char * leadstr[] = {"", "GRPLEAD", "SESSLEAD", NULL};

/** Get a list of *direct* child processes of parent process.
* @param p - Parent process whose *direct* children are being accessed
* @return - List of children
*/
GSList * par_get_ch(proc_t * p) {
  return (GSList*)p->lxcname;
}

/** Kill process by sending SIGKILL signal to it.
 * See also man 7 signal and man 2 kill.
 * @param pid - PID of individual process (>= 1)
 */
int proc_kill(int pid) {
  // int pid = 14197; // 1000000;
  // Check that we are not killing
  // - every process in callers process group when pid == 0
  // - all possible processes by pid == -1
  // - process group by -pid when pid < -1
  if (pid < 1) {
    printf("Process PID (for kill) must be postitive"); return 0;
  }
  int err = kill(pid, SIGKILL);
  if (err) {
     char errbuf[64];
     printf("Error killing: %d (%s)\n", errno, strerror(errno)); // strerror_r(errno, errbuf) GNU
     return 0;
  }
  return 1;
}
/** Convert / Format p->start_time into human readable ISO time.
* @param p - Process
* @param tbuf - String buffer for ISO time
* @param size - Size for tbuf (will be forwarded to strftime())
* @param fix - N/A (yet), pass 0 (TODO: Fix start_time to reflect EPOC time)
* @return None
*/
void proc_st_iso(proc_t * p, char * tbuf, size_t size, int fix) {
  // See time.h. TODO: possibly fix start_time to be "since 1970-1-1" instead of "since boot"
  struct tm t; // = {0}; // Init unnecessary for correct results
  // time_t ts = (unsigned long)p->start_time; // Raw / Wrong (!) type: unsigned long long
  // time_t ts = getbtime() + p->start_time / Hertz; // getbtime(): proc/sysinfo.c, present in libprocps.s Idiom : ps/output.c time_t -> __TIME_T_TYPE -> __SYSCALL_SLONG_TYPE -> long int
  time_t ts = proc_stime_ux(p);
  //if (fix) { p->start_time = ts; } // Fix start_time to be since 1970-1-1
  localtime_r(&ts, &t);
  strftime(tbuf, size, "%F %T", (const struct tm *)&t);
  return;
}
/** Return Process leadership position (NONE=0, GROUP=1, SESSION=2).
* Returned int values can be converted to string labels by 
* @param p - process
* @return Integer encoded value for leadership status (See above desc.).
*/
int gsleader(proc_t * p) {
  int lead = 0;
  if (p->tid == p->pgrp) { // Grp lead
    lead = 1;
    if (p->pgrp == p->session) { lead = 2;} // Sess lead
  }
  return lead;
}

/** Create approximate cmdline string out of char ** cmdline.
* Approximate means no quoting or escaping is done to turn char ** items
* back into *real* runnable properly formatted  commandline string.
* Approximate is good enough for this process listing usage.
* @param list Array of Strings
* @param buf String buffer where Array items are serialized space-separated (must contain enough space for )
* @return true for serialization being done, 0 for list being NUUL (no serialization *can* be done).
* @todo Pass buffer size
*/
int list2str(char ** list, char buf[]) {
  int pos = 0;
  int len = 0;
  if (!list) { return 0; } // printf("No list\n");
  for (;*list;list++) {
    len = strlen(*list);
    // printf("Len: %d\n", len);
    memcpy(&(buf[pos]), *list, len);
    pos += len;
    //if (pos > (len-1)) { buf[]; } // min(len-1, pos)
    buf[pos] = ' ';
    pos++;
    
  }
  buf[pos] = '\0';
  return 1;
}

/** Populate Process node into (Jansson) JSON Object.
* @todo See https://groups.google.com/g/jansson-users/c/xD8QLQF3ex8 and the need to use json_object_set_new()
*/
json_t * proc_to_json(proc_t * proc, char * cmdline) {
  if (!proc) { return NULL; }
  json_t *obj = json_object();
  if (!obj) { return NULL; }
  // char isotime[32]; // = {0};
  char starr[2] = {0, 0}; // Single letter process state
  json_object_set_new_nocheck(obj, "pid", json_integer(proc->tid));
  json_object_set_new_nocheck(obj, "cmd",  json_string(proc->cmd)); // bn of executable
  // char ** cmdline (/proc/$PID/cmdline is null-byte delimited version
  cmdline[0] = '\0';
  int ok = list2str(proc->cmdline, cmdline);
  // printf("CMDLINE:%s\n", cmdline);
  if (proc->cmdline && ok) { json_object_set_new_nocheck(obj, "cmdline",  json_string(cmdline)); }
  json_object_set_new_nocheck(obj, "ppid", json_integer(proc->ppid));
  json_object_set_new_nocheck(obj, "rss",  json_integer(proc->rss));
  // Times: start_time (should be seconds after 1970-1-1, but rolls on!)
  //proc_st_iso(proc, isotime, 32, 0); // ISO
  time_t ts = proc_stime_ux(proc);
  json_object_set_new_nocheck(obj, "starttime", json_integer(ts)); // proc->start_time
  
  json_object_set_new_nocheck(obj, "utime", json_integer(proc->utime));
  json_object_set_new_nocheck(obj, "stime", json_integer(proc->stime));
  // Also cutime, cstime for cumulated ?
  //json_object_set_new_nocheck(obj, "cutime", json_integer(proc->cutime));
  //json_object_set_new_nocheck(obj, "cstime", json_integer(proc->cstime));
  // User info ruser[P_G_SZ] - Only populated w. flag PROC_FILLUSR
  json_object_set_new_nocheck(obj, "owner",  json_string(proc->ruser));
  
  // State: CPU/ processor, Process state (S=sleeping)
  json_object_set_new_nocheck(obj, "cpuid", json_integer(proc->processor));
  //state, R=Running (or runnable), S=sleeping (Interruptable sleep), Z=Defunct/Zombie, D=Uninterruptable sleep (usually IO)
  // T: Stopped (e.g. ctrl-Z), I=???(On kernel threads)
  starr[0] = proc->state;
  json_object_set_new_nocheck(obj, "state",  json_string(starr));
  // Children ? Do not populate children here, but provide (Array) container.
  // In linear model we never keep track of children.
  // GSList * chn = par_get_ch(p);
  // if (chn) { json_object_set_new_nocheck(obj, "children", json_array()); }
  return obj;
}


/** Dump process tree to STDOUT.
 * @param p - Root process of the tree
 * @param lvl - Level in the process tree (Intial caller should pass 0)
 */
void ptree_dump(proc_t * p, int lvl) {
  GSList * chn = NULL;
  //GSList * iter = NULL;
  char tbuf[128] = {0};
  for (int i = 0;i<lvl;i++) { putchar(' '); }
  // u-long-long and u-long: %llu %lu
  ///////
  // Notes: pgrp:s are not processes pid/tid -> pgrp -> session
  // stime=%lu, (unsigned long)p->start_time,
  proc_st_iso(p, tbuf, 128, 0);
  int leadtype = gsleader(p);
  printf("Proc %d grp:%d sess:%d (lvl=%d, cmd=%s, thr#:%d iso=%s, usr=%s) %s\n", p->tid, p->pgrp, p->session, lvl, p->cmd, p->nlwp, tbuf, p->ruser, leadstr[leadtype]);
  if (chn = par_get_ch(p)) {
    for (GSList *iter = chn; iter; iter = iter->next) {
      ptree_dump((proc_t*)iter->data, lvl+1);
    }     
  }
  
}
/** Convert process tree to JSON.
 * 
 */
json_t * ptree_json(proc_t * p, int lvl) { // , json_t * obj
  char cmdline[2048] = {0};
  // if (obj == NULL) { obj = proc_to_json(p); }
  
  json_t * obj = proc_to_json(p, cmdline);
  
  GSList * chn = par_get_ch(p);
  if (!chn) { return obj; }
  json_t * carr = json_array();
  json_object_set_new_nocheck(obj, "children", carr);
  GSList * iter = NULL;
  for (iter = chn; iter; iter = iter->next) {
    json_t * child = ptree_json((proc_t*)iter->data, lvl+1); // Populate child into JSON
    // json_t * arr = json_object_get(obj, "children"); // Could inquire, but use handle above
    // int err = json_array_append(carr, child); // Add to "children"
    json_array_append_new(carr, child);
  }
  return obj;
}
/** Free/Release the tree of processes.
* Release custom used lxcname member (GSList *) and mark it unallocated (NULL).
* to not accidentally free it wrongly (or leak memory)
*/
void ptree_free(proc_t * p, int lvl) {
  // GSList * iter = NULL;
  GSList * chn = par_get_ch(p);
  // TODO: Show GSList count: %d, ...
  //printf("Free parent with children: %p (lvl=%d)\n", chn, lvl);
  for (GSList *iter = chn; iter; iter = iter->next) {
    //NOT: freeproc((proc_t*)iter->data);
    ptree_free((proc_t*)iter->data, lvl+1);
  }
  g_slist_free(chn); // (GSList *)p->lxcname. g_slist_free_full(chn, (GDestroyNotify)freecb);
  p->lxcname = NULL;
  freeproc(p);
}
