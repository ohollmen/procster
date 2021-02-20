#include <glib.h>
#include <proc/readproc.h>

// Need PROC_FILLSTATUS for ppid, need PROC_FILLSTAT for times.
#define PROC_FLAGS_DEFAULT PROC_FILLMEM | PROC_FILLCOM | PROC_FILLUSR | PROC_FILLSTATUS | PROC_FILLSTAT
// process start time as EPOC timestamp
#define proc_stime_ux(p) (getbtime() + p->start_time / Hertz)
proc_t * proc_tree(void);
void ptree_dump(proc_t * p, int lvl);
// Process tree parnt-child relations helpers
GSList * par_get_ch(proc_t * p);
void par_add_child(proc_t * par, proc_t * ch);
//
json_t * proc_to_json(proc_t * proc, char * cmdline);
json_t * ptree_json(proc_t * p, int lvl);
json_t * proc_list_json2(int flags);
void ptree_free(proc_t * p, int lvl);

typedef struct pstree {
  GHashTable * idx; // Index for fast lookup
  PROCTAB * proctab; // Handle to opened procps "process table"
  proc_t * p0; // Root of the process tree
  GSList * list; // For temporarily unresolved parents
  int debug; // Debugging on
} pstree;

///////////////// Miniserver //////////////////
typedef struct miniserver miniserver;
typedef struct request    request;
typedef struct response   response;

typedef int (*webhandler)( request * req,  response * res);

struct action {
  char * url;
  //webhandler hdlr;
};
struct miniserver {
  char * docroot;
  int port;
  GSList * handlers;
};
struct request {
  struct action * act;
  char * method;
  int methodnum;
  // For future POST
  char * cont;
  int contlen;
  
};
struct response {
  struct action * act;
  // Either json OR cont
  json_t json;
  char * cont;
  int contlen;
  // Response 
  int code;
};

