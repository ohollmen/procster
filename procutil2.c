/** @file
* Procutil for the new-gen libproc2 API.
* Compile (ad-hoc, test):
* 
* - gcc -c procutil2.c
#   -I/usr/include/glib-2.0/ - not needed with pkg-config
* - gcc -DTESTMAIN=1  -o procutil2 procutil2.c -ljansson -lproc2 `pkg-config --cflags glib-2.0` -lglib-2.0
* Run:
* ```
* ./procutil2
* ```
* ## Refs
* 
* - Documentation: https://man7.org/linux/man-pages/man3/procps_pids.3.html
* - https://gitlab.com/procps-ng/procps
* - https://stackoverflow.com/questions/65281694/how-to-correctly-free-a-ghashtable-of-structs
* - Hashtable Integer keys and GINT_TO_POINTER() macro: https://stackoverflow.com/questions/22057521/glib2-hash-table-gint-to-pointer-macro
*   - Suggests for int keys: Use g_direct_hash, g_direct_equal and GINT_TO_POINTER() for assign/lookup
* - GINT_TO_POINTER() in /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h
*   - define GINT_TO_POINTER(i)	((gpointer) (glong) (i))
*   - Seems GINT_TO_POINTER() makes ght treat number as pointer, there is no "address of", only casting
*   - Also: the only use case for g_int_hash() and g_int_equal() is if you're using an allocated pointer to an integer as the key, for instance, if you're using a struct member. 
* 
* ## libproc2 API Example
* 
* This field selection can be made on the caller side by:
* ```
*  struct pids_info *info = NULL; // Will hold field setup.
*  enum pids_item items[] = { PIDS_ID_PID, PIDS_CMD };
*  int numitems = sizeof(items) / sizeof(items[0]);
*  int result = procps_pids_new(&info, items, numitems);
* ```
* Further the fetching the processes and populating json can be done by
```
enum pids_fetch_type fetch_type = PIDS_FETCH_TASKS_ONLY; // ... OR 
struct pids_fetch *fetch_result = procps_pids_reap(info, fetch_type);
int totcnt = fetch_result->counts->total;
    for (int i = 0; i < totcnt; ++i) {
      struct pids_stack *proc2 = fetch_result->stacks[i];
      json_t *obj = proc_to_json(proc2, info, ""); // cmdline
      // Append ...
      ...
    }
```
*/

#include <libproc2/pids.h>
#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>

// Must use -I/user/include/glib-2.0/
// Compile and linking get much more complicated w. glibc !!! (Add -I..., Add 
#include <glib.h> // tree stuff



/** Reusable property/field composition.
* See /usr/include/libproc2/pids.h (enum pids_item) for allowed values.
*/
enum pids_item proc_std_props[] = {
  PIDS_ID_PID, // s_int
  PIDS_CMD,    // str Also: PIDS_CMDLINE (str), and PIDS_CMDLINE_V (strv),
  PIDS_ID_PPID, // s_int
  PIDS_RSS,     // ul_int Also: PIDS_SMAP_RSS
  PIDS_VM_SIZE, // ul_int
  PIDS_TIME_START, // real ! Also: PIDS_TIME_ELAPSED,
  // User and system times
  PIDS_TICS_USER, PIDS_TICS_SYSTEM, // ull_int (both) Ticks / Times (User/System). Also _C versions (??)
  //PIDS_ID_RUID, // u_int Also ...
  PIDS_ID_RUSER, // str
  
  PIDS_PROCESSOR, // s_int Also: PIDS_PROCESSOR_NODE, s_int
  PIDS_STATE,  // s_ch
  // Test/Debug
  PIDS_ID_EUID, // u_int
  // PIDS_ENVIRON, PIDS_ENVIRON_V,
  
};

/** Convert new libproc2 single process info to JSON
* Note the new process node type here: `struct pids_stack * proc2` (where proc2 naming reflects the process record of libproc2).
* The fields stored in pids_stack passed here must comply to expected
* For composition and order of fields see the module variable `proc_std_props`.



*/
json_t * proc_to_json(struct pids_stack *proc2, struct pids_info *info, char * cmdline) {
  // 
  if (!proc2) { return NULL; }
  json_t *obj = json_object();
  if (!obj)   { return NULL; }

  // Access the items within the stack (e.g., PID and command name)
  // You'll need to define your own enumerators or use hardcoded indices
  // to access the results based on the order defined in the 'items' array
        
  //int pid = proc2->head[0].result.u_int; // Assuming PID is the first item
  //const char *cmd = proc2->head[1].result.str; // Assuming CMD is the second item
  // Use provided macro PIDS_VAL() to access properties in result set.
  // Access Examples (to local scalars)
  /*
  int pid         = PIDS_VAL(0, s_int, proc2, info); // PIDS_ID_PID at index 0, result type s_int
  const char *cmd = PIDS_VAL(1, str, proc2, info);
  printf("PID: %d, CMD: %s\n", pid, cmd);
  */
  // Get value from struct pids_result: 
  // Set JSON members as strings and integers ...
  
  char starr[2] = {0, 0}; // Single letter process state
  // Note: json_integer() takes json_int_t (typedef long json_int_t)
  json_object_set_new_nocheck(obj, "pid",  json_integer(PIDS_VAL(0, s_int, proc2, info)));
  json_object_set_new_nocheck(obj, "cmd",  json_string (PIDS_VAL(1, str,   proc2, info)));
  
  json_object_set_new_nocheck(obj, "ppid", json_integer((long)PIDS_VAL(2, s_int, proc2, info)));
  json_object_set_new_nocheck(obj, "rss",  json_integer((json_int_t)PIDS_VAL(3, ul_int, proc2, info)));
  json_object_set_new_nocheck(obj, "size", json_integer(PIDS_VAL(4, ul_int, proc2, info)));
  // process start time as EPOC timestamp
  // #define proc_stime_ux(p) (getbtime() + p->start_time / Hertz)
  //time_t ts = proc_stime_ux(proc); // TODO ...PIDS_TIME_START real ?! Coerce to integer
  time_t ts = (int) PIDS_VAL(5, real, proc2, info); // json_real(double)
  json_object_set_new_nocheck(obj, "starttime", json_integer(ts));
  
  json_object_set_new_nocheck(obj, "utime", json_integer(PIDS_VAL(6, ull_int, proc2, info)));
  json_object_set_new_nocheck(obj, "stime", json_integer(PIDS_VAL(7, ull_int, proc2, info)));
  
  json_object_set_new_nocheck(obj, "owner", json_string (PIDS_VAL(8, str,   proc2, info)));
  //json_object_set_new_nocheck(obj, "owner_uid", json_integer (PIDS_VAL(8, u_int,   proc2, info)));
  
  json_object_set_new_nocheck(obj, "cpuid", json_integer(PIDS_VAL(9, s_int, proc2, info)));
  
  starr[0] = PIDS_VAL(10, s_ch, proc2, info); // proc->state;
  json_object_set_new_nocheck(obj, "state",  json_string(starr)); // One-letter string
  
  
  return obj;
}
typedef struct pstreenode PSN;
/** Helper node for libproc2 process-tree collecion */
struct pstreenode {
  int pid;
  int ppid; // Initial Orphans must have parent ref to not remain orphans
  json_t * proc_j;
  //GSList * children; // GSList * list = NULL;
  //char dummy[2000000]; // Test for memleak(s)
};
PSN * psn_new(int pid) { // json_t * proc_j
  PSN * psn = (PSN*)calloc(1, sizeof(PSN));
  psn->pid = pid;
  //if (proc_j) { psn->proc_j = proc_j; } // NOTHERE: proc_to_json(proc2, info, NULL); NOTE: info ????
  //else {
  //psn->proc_j = json_object(); // Empty
  //}
  // AUTO: psn->children = NULL;
  return psn;
}
void * psn_add_child(PSN * psn, PSN * child) {
  // psn->children = g_slist_append((GSList*)psn->children, child); // NOT Needed
  // Both Should also have JSON by now
  if (psn->proc_j && child->proc_j) {
    json_t * arr = NULL;
    // Already has JSON "children"
    if (arr = json_object_get(psn->proc_j, "children")) { }
    else {
      arr = json_array();
      json_object_set_new_nocheck(psn->proc_j, "children", arr);
    }
    json_array_append_new(arr, child->proc_j); // Parents array of children
  }
}
/** Free PSN so that JSON ownership is held outside (not free'd)
*/
void psn_free(void * psn) {
  // Nothing else but self to free.
  free(psn);
}
//GSList * psn_get_ch(PSN * psn) {
//  return (GSList*)psn->children;
//}

/** Transform libproc2 process result set to JSON tree ...
* TODO: Look how procps JSON tree is structured (proctree.c)
*/
// PSN
json_t* proc2_tree(struct pids_info *info, struct pids_fetch * fetch_result) {
  int totcnt = fetch_result->counts->total;
  // Hashtable to store children ? Hashtable to store JSON ? Single hashtable
  //GHashTable * ht = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, psn_free); // key_free, val_free
  // 
  GHashTable * ht = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, psn_free);
  if (!ht) { printf("GHashTable allocation failure !\n"); return NULL; }
  GSList * list = NULL; // Container/List for unresolved parents
  // Setup process 0 (object) as root of tree
  PSN * p0 = psn_new(0); p0->ppid = 0; p0->proc_j = json_object(); // Set JSON attrs
  json_object_set_new_nocheck(p0->proc_j, "cmdline",  json_string ("vmlinux"));
  for (int i = 0; i < totcnt; i++) {
    // Where to "hold" PS-children ? hashtable keyed by integer-PID ?
    struct pids_stack * proc2 = fetch_result->stacks[i]; // stacks[i]
    int pid  = PIDS_VAL(0, s_int, proc2, info);
    int ppid = PIDS_VAL(2, s_int, proc2, info); // idx 2
    PSN * psn = psn_new(pid); // psn->ppid = ppid;
    // Populate json, assign
    psn->proc_j = proc_to_json(proc2, info, ""); // last: cmdline
    //g_hash_table_insert(ht, GINT_TO_POINTER(psn->pid), psn);
    g_hash_table_insert(ht, &(psn->pid), psn);
    //printf("PID: %d, PPID: %d\n", pid, ppid);
    // Note: Because of this we dont need root (ppid=0) in ht (hashtable) !
    if (!ppid) { psn_add_child(p0, psn); printf("ROOT: Add child (%d)\n", psn->pid); continue; }
    //PSN * psnpar = g_hash_table_lookup (ht, GINT_TO_POINTER(ppid));
    PSN * psnpar = g_hash_table_lookup (ht, &ppid);
    if (psnpar) {
      //printf("Got par: %d\n", psnpar->pid);
      psn_add_child(psnpar, psn);
    }
    else { list = g_slist_prepend(list, psn); psn->ppid = ppid; } // Add orphan (_prepend for hi-perf.)
  }
  // DEBUG: Announce how many orphans
  printf("Num. orphans: %d\n", g_slist_length(list) ); // Actually guint
  GSList * iter = NULL;
  for (iter = list; iter; iter = iter->next) {
    PSN * psn = (PSN*)(iter->data);
    //PSN * par = g_hash_table_lookup (ht, GINT_TO_POINTER(psn->ppid));
    PSN * par = g_hash_table_lookup (ht, &(psn->ppid));
    if (!par) {
      printf("Par lookup failed even if all procs are added !\n"); // continue;
      par = p0; // Parent to root (in the lack of better parent)
    }
    psn_add_child(par, psn);
  }
  //  if (ht) {
  
  g_hash_table_destroy(ht); // ht = NULL; // }
  g_hash_table_unref(ht);
  json_t * root = p0->proc_j;
  psn_free(p0);
  //return p0; // Note: p0 not in hashtable, was not free'd
  return root;
}
// Not needed ?
json_t * ptree_json(PSN * psn) {
  
}
/** List processes with libproc2 API .
- set up the fields
- 
*/
struct pids_fetch * proc_fetch(struct pids_info ** pinfo) {
  struct pids_info *info = NULL;
  enum pids_item * items = proc_std_props; // Example: { PIDS_ID_PID, PIDS_CMD };
  int numitems = sizeof(proc_std_props) / sizeof(items[0]);
  int result = procps_pids_new(&info, items, numitems);
  if (result != 0) { fprintf(stderr, "Error initializing procps_pids: %d\n", result); return NULL; }
  //printf("Setup %d flds.\n", numitems);
  //Further the fetching the processes and populating json can be done by
  enum pids_fetch_type fetch_type = PIDS_FETCH_TASKS_ONLY; // PIDS_FETCH_THREADS_TOO
  struct pids_fetch * fetch_result = procps_pids_reap(info, fetch_type); // fetch_type aka which
  *pinfo = info;
  return fetch_result;
}
/** Convert libproc2 process result set to JSON (array).
*/
json_t * proc2_list_json(struct pids_info *info, struct pids_fetch * fetch_result) {
  if (!fetch_result) { return NULL; }
  if (!info)         { return NULL; }
  int totcnt = fetch_result->counts->total;
  // struct pids_stack ** stacks = fetch_result->stacks;
  json_t *array = json_array();
  // struct pids_stack * stacks = fetch_result->stacks;
  for (int i = 0; i < totcnt; i++) {
    struct pids_stack * proc2 = fetch_result->stacks[i];
    // struct pids_stack * proc2 = stacks[i];
    json_t *obj = proc_to_json(proc2, info, ""); // cmdline
    json_array_append_new(array, obj); // Append ...
  }
  return array;
}
// Dummy (to compile)
int proc_kill(int pid) {
  return 1;
}
int pid_extract(const char * url) {
  return 1;
}
#ifdef TESTMAIN
int main(int argc, char **argv) {
  struct pids_info *info = NULL;
  struct pids_fetch * fetch_result = proc_fetch(&info);
  json_t * procs = NULL; // Array (linear) or Object (tree)
  char * jsonstr = NULL;
  size_t jflags = JSON_INDENT(2);
  if ((argc > 1) && !strcmp(argv[1], "tree")) {
    //PSN * p0  = proc2_tree(info, fetch_result);
    procs = proc2_tree(info, fetch_result);
    printf("Got tree p0 (root): %ld\n", (long)procs); // OLD: (long)p0
    //NA: json_t * obj = ptree_json(root, 0); // 0 = top
    //if (!p0->proc_j) {
    if (!procs) {
      printf("Process tree root does not have JSON !!!\n"); return 1; }
    //procs = p0->proc_j;
    
    //jsonstr = json_dumps(p0->proc_j, jflags);
    
  }
  else {
  //json_t *array
  procs = proc2_list_json(info, fetch_result);
  // Dump ... ?
  //jsonstr = json_dumps(array, jflags);
  }
  if (procs)   { jsonstr = json_dumps(procs, jflags);}
  if (jsonstr) { fputs(jsonstr, stdout); free(jsonstr); }
  if (procs) { json_decref(procs); }
  //printf("DONE\n");
}
#endif

