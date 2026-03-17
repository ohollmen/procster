/** @file
* Process server (main()) 
*
* # Compiling
* 
* gcc -o procserver procserver.c -lmicrohttpd
* 
* # Info on MHD development
* 
* - Google: microhttpd examples
* - see (in Debian/Ubuntu) /usr/share/doc/libmicrohttpd-dev/examples/.
* - Response from file:
*     - //response = MHD_create_response_from_fd_at_offset64 (sbuf.st_size, fd, 0);
* - https://lists.gnu.org/archive/html/libmicrohttpd/2014-10/msg00006.html
*   - Important notes on parsing ANY POST body
* (Not just x-www-form-urlencoded or multipart/form-data) - Still a bit unclear and unpractical
* - MHD official PDF Tutorial: https://www.gnu.org/software/libmicrohttpd/tutorial.pdf
* - https://pastebin.com/SjVCkKAW
* - https://stackoverflow.com/questions/29700380/handling-a-post-request-with-libmicrohttpd - MHD POST Info
* 
* # Info on Ulfius
* 
* - See also libulfius in Ubuntu: apt-get libulfius2.2 (Note 2.2)
* - https://github.com/babelouest/ulfius
*
* # TODO
*
* - Consider where "tree" model can be created - at server or client ?
*   - In either 1 or 2 pass algorithm (or between) ? Does sorting help (if processes overflowed 64K (general case), not)
*   - Need hashtables in C for server side tree structuring ?
* - Use json_t *json_deep_copy(const json_t *value) or just parse template string for error message
* # Mem leakges
* Poll each 3000 ms
* Case 1
* - Linear mode, one proc_t node in stack
* - JSON un-freed (No json_decref()), Use json_object_set, NOT json_object_set_new
* - Response memmode MHD_RESPMEM_PERSISTENT
* - 205MB/2:30 82MB / hr
* Case 2
* - Set serialized JSON string "page" memmode = MHD_RESPMEM_MUST_FREE;
* - 45MB / 35min 52MB/43 min 55MB/45min 73MB/ 1hr => (derived) 183MB/2.5hr 73.3MB/hr
* Case 3
* - Additionally Use
*   - json_object_set_new() (OLD: json_object_set) on obj member additions
*     - Better yet: json_object_set_new_nocheck() - No key UTF-8 check
*   - json_array_append_new() (OLD: json_array_append()) on array add
*   - json_decref(obj)
* - Does Practically not leak memory.
*/

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <microhttpd.h> //  /usr/include/microhttpd.h
#define MHD_PLATFORM_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h> // getcwd(), access(), getpid()
#include <jansson.h> // For config
#include <ctype.h> // isdigit()
#include "proclister.h"
// #include "ms/miniserver.h"

#include <sys/stat.h>

static volatile int keep_running = 1;
void sigterm_handler(int signum) {
    (void)signum; // Unused parameter
    keep_running = 0;
    fprintf(stderr, "Received signal %d, stopping MHD...\n", signum);
}
// Old loc of connection_info_struct


char docroot[256] = {0};
typedef struct miniserver miniserver;
// Local minimalistic version server struct
struct miniserver {
  //char * name;    ///< Application / Service name
  //mod-global: char * docroot; ///< Document root for static content.
  //main: int port;       ///< TCP Port number where app / server is listening to connections.
  int jsonindent; ///< See:jpretty, disambiguate
  //GSList * actions; ///< Set of actions (TODO: Use regular array)
  //action ** actions; ///< Set of actions as an (pointer-)array of actions (pointers)
  //inithandler init; ///< Application initialization handler.
  void * server;   ///< Lower level server object (MHD) ... TODO: Call s->mhd ? s->mhd_server ?
  //int debug;       ///< App/server level debug
  //int reqdebug;    ///< Request level (e.g. request dispatching and handling) debug. Also handlers can use this "hint" to enable conditional logging.
  char * logfname; ///< Log file name. This file can be logged into by framework and application implemented handlers.
  FILE * logfh;    ///< Log file handle (opened for logging)
  char * pidfn;    ///< Pid file name (for file server will write PID into at startup)
  // Experimental global response processor (triggers for every action , every request). 
  //webhandler respproc;
  //mimetype ** mts; ///< Array of mime-types (Also: mimecnt to look up from tail (if user defined ones get appended)
  //main: int daemon;      ///< Flag to daemonize the app server to a child process
  // array of opts for MHD Server options (trans string-to-enum)
  int jpretty;     ///< Flag for prettifying JSON
  char * sslcertfn;
  char * sslkeyfn;
  char * sslcert;
  char * sslkey;
  int secure;
  //kvpara ** params; ///< App level parameters, e.g. conn. info
};

// char * proc_list_json(int flags);
// char * proc_list_json2(int flags);
#ifdef NEED_POSTDATA_ITER
/** 
See: https://www.gnu.org/software/libmicrohttpd/tutorial.html#Processing-POST-data
MHD_PostDataIterator
*/
/*
static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, 
              uint64_t off, size_t size)
{
  CONNINFO * con_info = coninfo_cls;
  printf("Got: %s\n", data);
  // Fill answer string
  
  return MHD_YES;
}
*/
#endif
//#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h> // NEW Used to get implicit declaration of ‘open’ was here (Must be outsde func though !)
/**** Routines originally in other files ******/
// Copied instance
void daemon_prep() {
  // Detach from controlling terminal. Note: Must be first
  int pid = setsid(); // New session, pg (process group) leader, session leader
  fprintf(stderr, "PG/Sess Leader: %d\n", pid);
  // int pidf = fork(); // Second fork()
  // if (pidf < 0) exit(EXIT_FAILURE);
  // if (pidf > 0) exit(EXIT_SUCCESS);
  //chdir("/"); // Stephens
  //umask(0); // Stephens
  struct rlimit r;
  getrlimit(RLIMIT_NOFILE, &r);
  // Leave open 0,1,2
  // open_max(); // ?
  for (int i=3;i<r.rlim_cur;i++) { close(i); }
  printf("daemon_prep(): Closed files (>=3 .. %d), Turn output off 1,2\n", r.rlim_cur); // r.rlim_max (Do NOT USE) / r.rlim_cur
  //return;
  // 0,1,2: stdin, stdout, stderr
  int fd1 = open("/dev/null", O_RDWR);
  if (fd1 < 0) { return; }
  dup2(fd1, STDIN_FILENO); // Extra trial. With getchar() if this (fd=0, stdin) is dup2()d, There's immediate exit - ! EOF !
  dup2(fd1, STDOUT_FILENO); // 1 - STDOUT_FILENO
  dup2(fd1, STDERR_FILENO); // 2 - STDERR_FILENO
  if (fd1 > 2) { close(fd1); }
  
}
/////// In (New) framework: Set this in hook ... ///////
int ac_headers(struct MHD_Response * response) {
  int ok = 0;
  ok = MHD_add_response_header (response, "Access-Control-Allow-Methods", "HEAD, GET, POST, DELETE, PUT, OPTIONS");
  ok = MHD_add_response_header (response, "Access-Control-Allow-Origin", "*");
  ok = MHD_add_response_header (response, "Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, X-Registry-Auth");
  ok = MHD_add_response_header (response, "Cache-Control", "no-store");
  return ok;
}

/** Send generic (null terminated) string content and discard request.
* @param connection MHD_Connection / single request (See MHD Docs)
* @param page - Page as (null terminated) text
* @param status_code - HTTP numeirc status code (e.g. 200, 400. ...) to send with response
* @return Return the kind of values that MHD request handler returns (MHD_NO, ...).
* @todo Add len for binary data (allow -1 to measure string)
*/
static int send_page (struct MHD_Connection *connection,  const char* page, int status_code) {
  int ret;
  struct MHD_Response *response;
  // TODO: Try to avoid copies, make memmode configurable
  response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_MUST_COPY);
  if (!response) { return MHD_NO; }
  if (!status_code) { status_code = 200; }
  // Correct place for setting headers
  // MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
  ret = MHD_queue_response (connection, status_code, response);
  MHD_destroy_response (response); // response = NULL; // Model ?
  return ret;
}

// Old loc of multi-stage parsing macros

/** MHD POST Handler example.
 * Example of "proper" / generic multi-stage (state-aware) POST request parsing.
 * NOTE: This should be fairly easily convertable to generic
 * Echoes back the request content (w/o parsing it in between).
 */
#ifdef BODY_PARSING
int answer_to_connection_echo_back (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  if (!strcmp (method, "GET")) { return send_page(connection, "No GET allowed !!!\n", MHD_HTTP_OK); }
  // Must be POST or PUT w. Body
  
  printf("URL(%s): %s Datasize: %lu, con_cls: %p\n", method, url, *upload_data_size, *con_cls);
  
  //CONNINFO * con_info = *con_cls ? (CONNINFO *)*con_cls : con_info_create(connection);
  request * con_info = *con_cls ? *con_cls : req_new((miniserver *)cls, url, method); // Should be named req
  if (!*con_cls) { req_init(con_info, connection);  } // Redundant: *con_cls = req;
  // Checks that we have con_info
  if (parse_no_state(con_info)) { return MHD_NO; } // 0 con_info_create() failed ?
  /* */
  if (parse_starting(con_info)) { return MHD_YES; } // 1 
  int pret = post_body_parse(connection, upload_data, upload_data_size, con_cls);
  if (!pret) { if (con_info) { req_free(con_info); } return MHD_NO; } // Fatal error (MHD_NO) - terminate request OLD: con_info_destroy()
  if (parse_running(con_info))  { return MHD_YES; }
  /* coverity[assign_where_compare_meant] */
  parse_ending(con_info);
  ////////// Handle Request: Send the request content as response content /////////////
  // TODO: Should get handler from  app level (cls) or request level (con_cls). Handler should be set earlier by ... (?)
  struct MHD_Response *
    response = MHD_create_response_from_buffer(strlen(con_info->postdata), (void*)con_info->postdata, MHD_RESPMEM_MUST_COPY);
  if (con_info) { req_free(con_info); } // Must free ! OLD: con_info_destroy
  int ok   = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json"); if (!ok) { return MHD_NO; }
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response); // Model

  return ret;
}
#endif

/** Legacy: Generic POST Handler with POST Body parsing.
* See: MHD_post_process and MHD_create_post_processor
* MHD_ContentReaderFreeCallback
* MHD_RequestCompletedCallback set by  MHD_OPTION_NOTIFY_COMPLETED (2 pointer params)
*/

/*
int answer_to_connection_naive_echo (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  if (!strcmp (method, "GET")) {return send_page(connection, "No GET allowed !!!\n", MHD_HTTP_OK);}
  // Must be POST or PUT w. Body
  
  printf("URL(%s): %s Datasize: %lu, con_cls: %p\n", method, url, *upload_data_size, *con_cls);
  
  
  CONNINFO *con_info = NULL; // *con_cls;
  int pret = post_body_parse(connection, upload_data, upload_data_size, con_cls);
  if (!pret) { return MHD_NO; } // Fatal error - terminate request
  if (!*con_cls) { fprintf(stderr, "No *con_cls - should never happen!\n"); return MHD_NO; } // assert
  con_info = *con_cls; 
  if (con_info->is_parsing < 2) { return MHD_YES; } // 2 = COMPLETE, so not complete yet
  // *con_cls = NULL; /// NOT Needed if handled by (See: ...) MHD_OPTION_NOTIFY_COMPLETED
  ////////// Send the response the most ordinary way /////////////
  struct MHD_Response *
    response = MHD_create_response_from_buffer(strlen(con_info->postdata), (void*)con_info->postdata, MHD_RESPMEM_MUST_COPY);
  if (con_info) { con_info_destroy(con_info); } // Must free !
  int ok  = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json"); if (!ok) { return MHD_NO; }
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response); // Model

  return ret;
}
*/


/*
if (!*con_cls) {
    CONNINFO *con_info = calloc (1, sizeof (CONNINFO));
    if (!con_info) { return MHD_NO; }
    //con_info->answerstring = NULL;
  if (!strcmp (method, "POST")) {
    fprintf(stderr, "Working for POST 1, %lu (con_cls: %lu)\n", *upload_data_size, (unsigned long)*con_cls);
    con_info->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);   
    if (!(con_info->postprocessor)) { puts("Bummer\n"); free(con_info); return MHD_NO; }
    MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
    printf("Called MHD_create_post_processor: %llu\n", (long unsigned long)con_info->postprocessor);
    con_info->connectiontype = POST;
    // con_info->answerstring = "Full Page";
    
  }
  if (!strcmp (method, "GET")) {
    con_info->connectiontype = GET;
    // return from here as GET can terminate after related op (No body parsing)
    
   
  }
  *con_cls = (void *) con_info;
    return MHD_YES;
  }
  //////////////////////// HERE: *con_cls is true /////////////////////////
  if (!strcmp (method, "GET")) { return send_page (connection, "<h1>Should have a form</h1>", MHD_HTTP_OK); }
  // Even with POST we finally do send_page() / 
  if (!strcmp (method, "POST")) {
    CONNINFO * con_info = *con_cls; // Comes in con_cls
    fprintf(stderr, "Working for POST 2, %lu\n", *upload_data_size);
    if (*upload_data_size) {
      
      MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;
    }
    else if (con_info->answerstring) { return send_page (connection, con_info->answerstring, MHD_HTTP_OK); }
    // else { return send_page (connection, con_info->answerstring, con_info->answercode); }
  } 

  return send_page (connection, "Error !!!", MHD_HTTP_OK);
  // return ret;
*/
// #include <sys/types.h>
#include <fcntl.h> // open()
#include <sys/stat.h> // struct stat
/** Try to resolve url to a static file and return response for it.
* @param url - Request URL to test for static content
* @return MHD_Response (pointer) for static file or NULL if URL is not a static file.
* @todo: Check suffix and try to map to appropriate mime-type
*/
struct MHD_Response * trystatic(const char * url) {
  char urlfile[256] = {0};
  if (!strcmp(url, "/")) { url = "/index.html"; }
  sprintf(urlfile, "%s/%s", docroot, (char*)url);
  printf("Try static file: %s\n", urlfile);
  int acc = access( urlfile, F_OK );
  if (acc == -1) { printf("No access to %s\n", urlfile); return 0; }
  int fd = open(urlfile, O_RDONLY);
  if (fd < 0) { return 0; }
  struct stat statbuf = {0};
  int sok = fstat(fd, &statbuf);
  if (sok < 0) { close(fd); return 0; } // Must close() !
  uint64_t size = statbuf.st_size; // off_t
  printf("Ready to create response from fd: %d (%ld B)\n", fd, size);
  // MHD will close fd after. Note also _from_fd_at_offset(..., offset)
  struct MHD_Response * mhd_response = MHD_create_response_from_fd(size, fd); // Model
  // Extract suffix
  char * suff = strrchr(url, '.');
  if (suff) { printf("Found suffix: '%s'\n", ++suff); }
  //char * defmime = "text/plain"; // DECLARED_BUT_NOT_REFERENCED
  //MHD_add_response_header (response, "Content-Type", defmime);
  // close(fd);
  return mhd_response;
}
/** Check HTTP Basic credentials.

*/
int basic_creds_ok(struct MHD_Connection *connection) {
  char * pass = NULL;
  char * user = MHD_basic_auth_get_username_password (connection, &pass);
  // Validate user,pass minimum validity requirements.
  if (!user || strlen(user) < 3) { return 0; }
  if (!pass || strlen(pass) < 5) { return 0; }
  // Test
  if (strcmp(user, "test") || strcmp(pass, "test")) { return 0; }
  // Real check from user,pass hashtable
  // char * corrpass = g_hashtab_lookup(creds, user);
  // if (strcmp(corrpass, pass)) { return 0; }
  return 1;
}

/** MHD Response handler for creating OS Process listing.
* Provides an example of simple GET handling of HTTP Request.
*/
int answer_to_connection0 (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  // const char *page  = "<html><body>Hello, browser!</body></html>";
  struct MHD_Response * response = NULL;
  int ret; // = MHD_YES;
  int memmode = MHD_RESPMEM_PERSISTENT; // enum MHD_ResponseMemoryMode (MHD_RESPMEM_*: PERSISTENT, MUST_FREE, MUST_COPY )
  char * ctype = "text/plain";
  char * page = NULL;
  size_t jflags = JSON_INDENT(2);
  
  char * errpage = "{\"status\": \"err\", \"msg\": \"Error: ...\"}"; // TODO: produce as jansson D.S.
  char * okpage = "{\"status\": \"ok\", \"data\": \"Ran OK\"}";
  // printf("URL(%s): %s (Body Datasize: %lu)\n", method, url, *upload_data_size);
  printf("URL(%s): %s\n", method, url);
  int ok = 0; // Must be early to not "pass initialization" (cov)
  int httpcode = MHD_HTTP_OK;
  // MHD_get_connection_values (connection, MHD_HEADER_KIND, print_out_key, NULL);
  if ( !strcmp(url, "/proclist")) {
    //
    ctype = "application/json";
#ifdef PROC2
    struct pids_info *info = NULL;
    struct pids_fetch * fetch_result = proc_fetch(&info);
    json_t * array = proc2_list_json(info, fetch_result);
    if (info) procps_pids_unref(&info); // NEW
#else
    json_t * array = proc_list_json2(0); // Produce Process list content (OLD: page = ...)
#endif
    page = json_dumps(array, jflags);
    memmode = MHD_RESPMEM_MUST_FREE;
    json_decref(array); // array = NULL; // Model
    if (!page || !*page) {
      printf("Failed to produce content for client !\n"); page = errpage; memmode = MHD_RESPMEM_MUST_COPY; // Error
    }
  }
  else if (!strcmp(url, "/proctree")) { // 6? , 9
    ctype = "application/json";
    json_t * obj = NULL;
#ifdef PROC2
    struct pids_info *info = NULL;
    struct pids_fetch * fetch_result = proc_fetch(&info);
    obj = proc2_tree(info, fetch_result);
    if (!obj) { printf("Process tree root does not have JSON !!!\n"); return 1; }
    //OLD:procs = p0->proc_j;
    //OLD:psn_free(p0);
#else
    proc_t * root = proc_tree();
    // PW.BRANCH_PAST_INITIALIZATION - event: "A goto jumps past the initialization of a variable"
    if (!root) { page = errpage; memmode = MHD_RESPMEM_MUST_COPY; goto CHECKPAGE; }
    obj = ptree_json(root, 0);
    if (!obj) { ptree_free(root, 0); page = errpage; memmode = MHD_RESPMEM_MUST_COPY; goto CHECKPAGE; }
#endif
    page = json_dumps(obj, jflags);
    memmode = MHD_RESPMEM_MUST_FREE;
#ifndef PROC2
    ptree_free(root, 0);  // Model
#endif
    // "Resource \"root\" is not freed or pointed-to in \"ptree_free\"."
    json_decref(obj); // obj = NULL; // Model ?
    CHECKPAGE:
    if (!page || !*page) {
      printf("Failed to produce content for client !\n"); page = errpage; memmode = MHD_RESPMEM_MUST_COPY; // Error
    }
  }
  // Others: /uptime (/proc/uptime) /loadavg /proc/loadavg
  // Match
  else if (!strncmp(url, "/kill/", 6)) {
    char pidstr[20] = {0};
    int pid = pid_extract(url);
    if (pid) {
      printf("Got valid pid: %d\n", pid);
      if (pid <= 0) { printf("pid <= 0, error out for now.\n"); goto ERROR; }
      int ret = proc_kill(pid);
      if (ret) { printf("Error %d killing pid=%d\n", ret, pid); goto ERROR; }
      page = okpage; memmode = MHD_RESPMEM_MUST_COPY;
    }
    else {
      ERROR:
      errpage = "{\"status\": \"err\", \"msg\": \"Error killing process\"}";
      page = errpage; memmode = MHD_RESPMEM_MUST_COPY; // goto CHECKPAGE;
    }
  }
  else {
    response = trystatic(url);
    // "Overwriting previous write to \"ret\" with value from \"MHD_queue_response(connection, 200U, response)\".
    // "An assigned value that is never used may represent unnecessary computation, an incorrect algorithm, or possibly the need for cleanup or refactoring."
    if (response) {  goto QUEUE_REQUEST; } // Had: ret = MHD_YES; ()
    ctype = "text/plain"; printf("No action match for URL: %s\n", url);
  }
  // Check "page" once more 
  if (!page) { page = errpage; memmode = MHD_RESPMEM_MUST_COPY; }
  // memmode should be in MHD_RESPMEM_PERSISTENT, MHD_RESPMEM_MUST_COPY, MHD_RESPMEM_MUST_FREE
  response = MHD_create_response_from_buffer (strlen(page), (void*) page, memmode);
  
  ok = MHD_add_response_header (response, "Content-Type", ctype); // "application/json"
  // Allow conditional cross-domain ref (example from docker headers setup w. daemon cl opts --api-cors-header *)
  // 203 Non-Authoritative Information
  //if () {
  ok = ac_headers(response);
  if (!ok) { httpcode = 203; }
  //}
  // MHD_add_response_header (response, "Content-Type", "");
  //  return request; }
  
  QUEUE_REQUEST:
  ret = MHD_queue_response (connection, httpcode, response);
  // if (ret != MHD_YES) { } // Don't check, just return (per examples, see below)
  
  if (response) { MHD_destroy_response(response); } // Model ?
  return ret;
}
/** Post request Callback launched by MHD after completing request.
* 
*/
/*
void req_term_cb(void *cls, struct MHD_Connection * connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
  fprintf(stderr, "Request Terminated (con_cls=%p, cls=%p) !\n", con_cls, cls); // Was *con_cls: "Directly dereferencing pointer \"con_cls\"."
  if (! con_cls || !*con_cls) { return; } // Important. See manual p. 18
  con_info_destroy((CONNINFO *) *con_cls);
  *con_cls = NULL; // Comm. to MHD
  // NOT: return NULL;
  return;
}
*/

// Old PID Saving. Note: new impl in fw.
int savepid(json_t * json) {
  if (!json) { return 1; }
  json_t * jpidfn = json_object_get(json, "pidfn");
  //if (jpidfn) { s->pidfn = json_string_value(jpidfn); }
  if (!jpidfn) { return 2; }
  const char * pidfn = json_string_value(jpidfn);
  if (pidfn && *pidfn) {
    int pid = getpid();
    // Always overwrite, brute force
    FILE * logfh = fopen(pidfn, "w");
    if (!logfh) { return 3; }
    fprintf(logfh, "%d\n", pid);
    if (logfh) { fclose(logfh); }
  }
  return 0;
}
int savepid_simple(char * pidfn, int pid) {
  if (!pidfn) { return 1; }
  if (!pid) { return 2; }
  FILE * logfh = fopen(pidfn, "w");
  if (!logfh) { return 3; }
  fprintf(logfh, "%d\n", pid);
  if (logfh) { fclose(logfh); }
  return 0;
}
// OLD: daemon_prep()

/** Launch Procster MHD server (the old non-framework version, used by old main()).
 */
void daemon_launch(int port, json_t * json) {
  // OLD Log and PID
  
  // apc - Accept Policy Callback
  // TODO: Have this as config string (or array) member in JSON file (e.g. "mhdrunflags")
  // MHD_USE_TCP_FASTOPEN, MHD_USE_AUTO (poll mode), MHD_USE_DEBUG == MHD_USE_ERROR_LOG
  int flags = MHD_USE_SELECT_INTERNALLY | MHD_USE_INTERNAL_POLLING_THREAD; 
  struct MHD_Daemon * mhd = MHD_start_daemon (flags, port, NULL, NULL,
    // NOTE: Connection handler: answer_to_connection*
    &answer_to_connection0, NULL,
    MHD_OPTION_NOTIFY_COMPLETED,
    NULL, NULL, // req_term_cb, userdata MHD Manual p. 18 ()
    
    MHD_OPTION_END); 
  FILE * fh = stderr; // TODO: 
  if (NULL == mhd) { fprintf(fh, "Could not start MHD (check if port %d is taken)\n", port); } // return 3; 
  fprintf(fh, "Starting Micro HTTP daemon at port=%d, pid=%d (Try: http://localhost:%d/proclist)\n", port, getpid(), port);
  int ok = 100; // MHD_run(mhd); // MHD_YES on succ
  // Note:
  // - Starting w. systemd we always receive EOF (-1) here immediately (!?)
  // Starting from terminal (even with --daemon) we receieve the char we first typed, even if *only*
  // '\n' triggers return (line-buffering).
  //(void) getchar ();
  //int c = getchar();
  
  /*
  int c;
  // 
  while (c = getchar()) {
    if (c == 255) { break; }
  }
  fprintf(fh, "Stopping Daemon (getchar=%d, ok=%d)\n", c, ok);
  MHD_stop_daemon (mhd);
  */
}
// Loaner from external fw to let MHD run as persistent server 
void server_run_wait(miniserver * ms) {
  int c; int ccnt = 0;
  FILE * lfh = ms->logfh; // ms->logfh / stderr
  //setbuf(stdin, NULL);
  fprintf(lfh, "Coming to pause()\n");
  pause();
  /*
  while (c = getchar()) {
    ccnt++;
    fprintf(stderr, "Got char: val:%d\n", c); // Ctrl-D => -1
    if (c == -1) { break; } // OLD: 255
  }
  */
  
  fprintf(lfh, "Stopping Daemon (after receiving getchar=%d, ccnt=%d)\n", c, ccnt);
  if (ms->server) { MHD_stop_daemon (ms->server); }
  if (lfh) { fprintf(lfh, "Closing logging to '%s'\n", ms->logfname); fclose(lfh); }
}
// sigprocmask() - solution for stopping MHD.
// Atomically unblock signals and wait if a signal arrives while sigsuspend is active, it will be delivered
// and the handler will be called, setting keep_running = 0. sigsuspend will then return, and the loop will terminate.
void server_run_wait_sigproc(miniserver * ms) {
  int ret = 0;
  sigset_t  block_mask, old_mask, empty_mask; // 3 mask sets
  if (!ms) { return; }
  // Set the signal mask, saving the old one
  // SIG_BLOCK - block, modify, SIG_UNBLOCK - unblock, modify, SIG_SETMASK - set explictly 1-by-1
  //int spret = sigprocmask(SIG_BLOCK, &block_mask, &old_mask);
  //if (spret == -1) { perror("sigprocmask SIG_BLOCK"); return; }
  struct sigaction sa = {0}; // In stack !
  // memset(&sa, 0, sizeof(sa)); // Prefer {0};
  sa.sa_handler = sigterm_handler; // Sets keep_running = 0;
  sa.sa_flags = 0; // No special flags (implied by {0})
  if (sigaction(SIGINT,  &sa, NULL) == -1) { perror("sigaction SIGINT"); return; }
  if (sigaction(SIGTERM, &sa, NULL) == -1) { perror("sigaction SIGTERM"); return; }
  
  // Should run/start daemon here ????
  
  sigemptyset(&empty_mask); // Init to empty mask for sigsuspend. Opposite: sigfillset()
  
  while (keep_running) {
    sigsuspend(&empty_mask); // const sigset_t *mask. Like pause() 
  }
  if (ms->server) { MHD_stop_daemon (ms->server); }
}
/** Main for (Micro HTTP Daemon) process app.
* 5th param to MHD_start_daemon() defines the main connection handler
* (that should do respective dispatching if app handles many different actions).
*/
int main (int argc, char *argv[]) {
  int logtofile = 0;
  if (argc < 2) { printf("No args, pass port. (e.g. %s 8181)\n", argv[0]); return 1; }
  int port = atoi(argv[1]);
  int daemon = 0;
  for (int i=0;i<argc;i++) { if (!strcmp(argv[i], "--daemon")) { daemon = 1; break; }} // printf("%s\n", argv[i]);
  printf("Daemonize=%d (main pid=%d)\n", daemon, getpid());
  
  char * docr = getcwd(docroot, sizeof(docroot));
  if (!docr) { printf("No docroot gotten (!?)\n"); return 2; }
  printf("Docroot: %s\n", docroot);
  /////////////////////////////////////////////////////////
  json_error_t error = {0};
  /* the error variable contains error information */
  json_t * json = json_load_file("./procster.conf.json", 0, &error);
  if (!json) { printf("JSON parsing error: %s\n", error.text); }
  else { printf("Parsed JSON: %llu\n", (unsigned long long)json); }
  if (!daemon) { goto RUN; }
  pid_t pid = fork(); // Parent: Child pid returned, Child: 0 ret'd
  if (pid < 0) { printf("Parent (PID: %d) exiting for failed fork (PID: %d) !!!\n", getpid(), pid); return 0;}
  if (pid) { printf("Parent (PID: %d) exiting for child daemon (PID: %d)\n", getpid(), pid); return 0; }
  daemon_prep(); // In child only (daemon && !pid)
  RUN:
  //////////// Separate (local) logging and PID saving from essentials of daemon_launch
  logtofile = 1;
  char * logfn = "/tmp/procster.log";
  char * pidfn = "./procster.pid";
  FILE * fh = stdout;
  if (logtofile) { fh = fopen(logfn, "wb"); setbuf(fh, NULL); }
  // Refresh FD:s ? Systemd can capture output
  int piderr = savepid(json);
  //int piderr = savepid_simple(pidfn, getpid() );
  if (piderr) { fprintf(fh, "Error %d saving PID\n", piderr); }
  ///////////////////////////////
  
  daemon_launch(port, json);
  // TODO: server_new_procster(json) // Implement *essential* parts (only)
  // NO: debug, reqdebug, daemon (passed from cli?), docroot (auto-probed by getcwd), user, group
  // OK" pidfn, possibly logfn, port
  miniserver * ms = calloc(1, sizeof (struct miniserver)); // server_new(json); // large JSON loading func in ms/miniserver.c
  ms->logfh = fh; ms->server = NULL; // Make daemon_launch return MHD server ?
  if (logtofile) { ms->logfname = strdup(logfn); }
  // TODO: copy / inline here: server_run_wait_procster() ?
  fprintf(fh, "Starting server_run_wait()\n");
  //server_run_wait(ms); // microthhpd getchar() workarounds In ms/miniserver.c.
  server_run_wait_sigproc(ms);
  return 0;
}

