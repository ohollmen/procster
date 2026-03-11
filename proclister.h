#include <glib.h>
#define WITH_SYSTEMD
#ifdef PROC2
#include <libproc2/pids.h>
// Only essentials
struct pids_fetch * proc_fetch(struct pids_info ** info);
json_t * proc2_list_json(struct pids_info *info, struct pids_fetch * fetch_result);
json_t * proc2_tree(struct pids_info *info, struct pids_fetch * fetch_result);
#else
#include <proc/readproc.h>
#include <proc/sysinfo.h> // Hertz, getbtime


// Need PROC_FILLSTATUS for ppid, need PROC_FILLSTAT for times.
#define PROC_FLAGS_DEFAULT PROC_FILLMEM | PROC_FILLCOM | PROC_FILLUSR | PROC_FILLSTATUS | PROC_FILLSTAT
// process start time as EPOC timestamp
#define proc_stime_ux(p) (getbtime() + p->start_time / Hertz)
// Centos 7.6 does not have member lxcname:
// procutil.c:27:20: error: ‘proc_t’ has no member named ‘lxcname’
//#define proc_chn_init(p) (p)->lxcname = NULL
#define proc_chn_init(p) ((p)->sd_uunit = NULL)

proc_t * proc_tree(void);
void ptree_dump(proc_t * p, int lvl);
// Process tree parnt-child relations helpers
GSList * par_get_ch(proc_t * p);
void par_add_child(proc_t * par, proc_t * ch);
//
json_t * proc_to_json(proc_t * proc, char * cmdline);
json_t * ptree_json(proc_t * p, int lvl);
// Legacy way of listing processes
char * proc_list_json(int flags);
// New return-JSON way ...
json_t * proc_list_json2(int flags);
void ptree_free(proc_t * p, int lvl);
#endif


// Neutal / process-lib agnostic
int proc_kill(int pid);
int pid_extract(const char * url);

// Redudant, unused ?
/*
typedef struct pstree {
  GHashTable * idx; // Index for fast lookup
  PROCTAB * proctab; // Handle to opened procps "process table"
  proc_t * p0; // Root of the process tree
  GSList * list; // For temporarily unresolved parents
  int debug; // Debugging on
} pstree;
*/
