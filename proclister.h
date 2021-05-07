#include <glib.h>
#define WITH_SYSTEMD
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
json_t * proc_list_json2(int flags);
void ptree_free(proc_t * p, int lvl);
int proc_kill(int pid);

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
typedef struct action     action;
typedef int (*webhandler)( request * req,  response * res);

miniserver * server_new(json_t * json);
void server_actions_load(miniserver * s, action * actarr, int cnt);
void server_run(miniserver * s);
action * action_find(miniserver * s, const char * url);

struct action {
  char * actlbl; ///< Short id label (no spaces)
  char * name; ///< Descriptive name
  char * url; ///< URL that this action gets dispatched on
  char * conttype; ///< Content type this action plans to produce under normal conditions.
  webhandler hdlr; ///< Request handler callback (function pointer)
  int auth; ///< Require (Basic) authentication. TODO: Enum to the kind of auth.
};
struct miniserver {
  char * docroot;
  int port;
  int jsonindent;
  GSList * actions;
  void * server; // Lower level server object
  int debug;
  int reqdebug;
  char * logfname;
  FILE * logfh;
};
struct request {
  char * url;
  
  char * method;
  int methodnum;
  // For future POST
  char * cont;
  int contlen;
  char * conttype;
  struct action * act; ///< Action that dispatched handler
  miniserver * ms; ///< Server under which we are running
  response * res; ///< Associated Response
};
struct response {
  struct action * act;
  void * conn; ///< Lower level http conn/response, similar to miniserver->server;
  // Content: Either cont,contlen OR json
  json_t * json;
  char * cont;
  int contlen;
  char * conttype;
  int memmode; // For cont. Default: _FREE
  // int fd; // Option for sending file
  // Response Status code (e.g. 200, 404 ...)
  int code;
};
// Req, Res combo (helper for containing bot req, res)
// OR just link req to res by req->res;
