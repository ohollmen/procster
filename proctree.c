/**
 * Install libprocps6 libprocps-dev libglib2.0-dev libjansson4 libjansson-dev
 * 
 * # Glib and jansson
 * 
 * https://stackoverflow.com/questions/42295821/using-int-as-key-in-g-hash-table
 * https://stackoverflow.com/questions/16868077/glib-hash-table-lookup-failure
 * https://developer.ibm.com/tutorials/l-glib/
 * https://jansson.readthedocs.io/en/2.8/conformance.html
 * - https://stackoverflow.com/questions/50226252/what-is-the-proper-way-of-free-memory-in-creating-json-request-from-jansson-liba
 * - https://stackoverflow.com/questions/37018004/json-decref-not-freeing-memory
 * - https://groups.google.com/g/jansson-users/c/xD8QLQF3ex8 - jansson author Petri Lehtinen discussing
 *   building and easy to free (autofree) jansson data structure by using ..._new() versions of functions.
 *
 * gcc `pkg-config --cflags glib-2.0` foo.c `pkg-config --libs glib-2.0`
 * gcc foo.c `pkg-config --cflags --libs glib-2.0`
 * 
 * 
 * gcc -o proc proc.c -I/usr/include/glib-2.0 -lprocps  `pkg-config --cflags --libs glib-2.0`
 * 
 * Special flags for passing extras to openproc()
 * - PROC_PID - Pass an array of PIDS (0-terminated piudlist) as 2nd arg (pid_t* pidlist)
 * - PROC_UID - Pass (after PID:s) many UID:s followed by number of them.
 * 
 * # See struct proc_t
 * 
 * less /usr/include/proc/readproc.h
 * Notes on start_time:
 * Ubuntu  18.04: start_time;     // stat            start time of process -- seconds since 1-1-70
 * Gitlab (2021): start_time;// stat            start time of process -- seconds since system boot

 */
 
 
 
#include <stdio.h>
//#include <proc/readproc.h>
//#include <proc/sysinfo.h>
//#include <gmodule.h>
#include <glib.h>
#include <time.h> // /usr/include/time.h

#include <jansson.h>

#include "proclister.h"


//       proc_t* readproc(PROCTAB *PT, proc_t *return_buf);
//       void freeproc(proc_t *p);


// static int flags = PROC_FILLMEM | PROC_FILLCOM | PROC_FILLUSR | PROC_FILLSTATUS | PROC_FILLSTAT;



void free_data (gpointer data) {
  //printf ("freeing: %s %p\n", (char *) data, data);
  //free (data);
}
void key_free(void * k) {
  printf("KEY: free(%p)\n", k);
  // NOP !
}
/** Merely show hastable iteration. Intent is not to free this way
* as the process nodes are still used within tree structure (Hashtable
* was only a temporary index to assist in building of the tree).
*/
void val_free(void * v) {
  printf("VAL: freeproc(%d)\n", ((proc_t*)v)->tid);
  //NOT: freeproc((proc_t*)v);
}



/** Populate a tree of (proc_t) processes.
* Because of intricate linkages of process tree - and not wanting them
* broken or incomplete, we do not support partial filtered process trees
* (although the implementation contains some bulletproofing for this).
* 
* @return Root process node of the tree.
*/
proc_t * proc_tree(void) {
  // proc_t proc = {0};
  proc_t * procp = NULL;
  // Note: Tree-approach is non-trivial with UID(s) or PID:s specified -
  // All processes need to directly or indirectly link to root, to not be orphaned
  // int openflags = flags; //  | PROC_UID;
  // int uidarr[2] = { 56574, 0 }; // 
  // hashfunc, cmp, keyfree, valfree
  GHashTable * ht = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL); // key_free, val_free
  if (!ht) { printf("GHashTable allocation failure !\n"); return NULL; }
  GSList * list = NULL; // Container/List for unresolved parents
  // Alloc for kernel / PID:0
  proc_t * p0 = (proc_t*)calloc(1, sizeof(proc_t));
  if (!p0) { printf("root calloc failure !\n"); return NULL; }
  sprintf(p0->cmd, "vmlinux");
  sprintf(p0->ruser, "root");
  p0->nlwp = 1; // Threads
  p0->state = 'R'; // Others: rss
  // TODO: Parse /proc/cmdline to proc->cmdline (char **) ?
  g_hash_table_insert (ht, GINT_TO_POINTER(p0->tid), p0);
  int debug = 0;
  
  int flags = PROC_FLAGS_DEFAULT;
  PROCTAB*  proctab = openproc(flags); // , uidarr, 1); // TODO: readproctab(0); ?
  while (procp = readproc(proctab, NULL)) { // ALT: &proc
    proc_chn_init(procp); // Reserve use (for children)
    // debug && printf("%d (ppid: %d) ", procp->tid, procp->ppid);
    g_hash_table_insert(ht, GINT_TO_POINTER(procp->tid), procp);
    // No parent - Parent to (as a child of) p0 / kernel
    if (!procp->ppid) {
      par_add_child(p0, procp);
      //goto DONEPROC;
      continue;
    }
    proc_t * par = g_hash_table_lookup (ht, GINT_TO_POINTER(procp->ppid));
    // Approx 80% have parent already hashed an avail - Add to par (GSList)
    if (par) {
      par_add_child(par, procp); // printf("Have parent hashed");
    }
    // Parent N/A. Add procp (or tid) to reiterate child(ren) later
    else { list = g_slist_prepend(list, procp); } // NOT: _append (See IBM article)
    //DONEPROC:
    //debug = 0;
    // debug && printf(" %s ", procp->lxcname); // Check usage
    // debug && puts("\n");
  }
  closeproc(proctab);
  // Reiterate children whose parents were not found on the first round.
  // void ptree_link_orphans(proc_t * p0, GHashTable * ht, GSList * list) {
  GSList * iter = NULL;
  // g_slist_foreach(list, doit, (void*)ht);
  for (iter = list; iter; iter = iter->next) {
    proc_t * p = (proc_t*)(iter->data);
    // printf("Current item is %d, ppid: %d\n", p->tid, p->ppid);
    proc_t * par = g_hash_table_lookup (ht, GINT_TO_POINTER(p->ppid));
    if (!par) {
      printf("Par not looked up even if all procs are added !\n"); // continue;
      // Parent to root ? par_add_child(p0, p); OR ...
      par = p0;
    }
    par_add_child(par, p);
  }
  // } // ptree_link_orphans
  g_slist_free(list); list = NULL; // Free temp helper
  // printf("HT: %d keys\n", g_hash_table_size(ht));
  g_hash_table_unref(ht); // g_hash_table__destroy(ht) // Calling early will corrupt tid
  ht = NULL;
  // printf("The list is now %d items long\n", g_slist_length(list));
  
  return p0;
}

// setvbuf(stdout, NULL,  _IONBF, 0); // Not needed
  // printf("Hertz: %lld\n", Hertz);
//int pid = 28931;
  //proc_t * myp = g_hash_table_lookup (ht, GINT_TO_POINTER(pid));
  //if (myp) { printf("Got: %d\n", myp->tid); }
  // else { printf("No proc by PID=%d\n", pid); }

#ifdef TEST_MAIN_XX
int main (int argc, char ** argv) {
  proc_t * root = proc_tree();
  ptree_dump(root, 0);
  
  //ptree_json(p0);
  return 0;
}
#endif



void proc_htab_free(GHashTable * ht) {
  
}
