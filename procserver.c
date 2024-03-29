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

// MUST be min 256
// #define POSTBUFFERSIZE  512
#define POSTBUFFERSIZE  256
// Example ONLY
enum ConnectionType {GET = 0,POST = 1};
enum ParsingState {INITED =0, PARSING, COMPLETE};
typedef struct connection_info_struct CONNINFO;
/** MHD connection/request Helper data structure for maintaining the "state"
* of request data reading / parsing (as data is read in chunks) for HTTP methods
* that have a HTTP Body (POST, PUT).
* The state is maintained across multiple calls to MHD "answer_to_connection"
* (response handler) callback function. The last call to "answer_to_connection"
* will let response to be created.
* The reference implementation of this flow is in function named answer_to_connection
* in this file and will utilize post_body_parse as workhorse of parsing / assembling
* the POST body (into connection_info_struct, member postdata).
*/
struct connection_info_struct {
  // enum ConnectionType connectiontype;
  // struct MHD_PostProcessor *postprocessor;
  // FILE *fp;
  // const char *answerstring;
  // int answercode;
  int debug;
  int is_parsing; /**< State of parsing (INITED =0, PARSING, COMPLETE) */
  int contlen;    /**< Currently stored content length / length gotten from "Content-length" */
  char * conttype; /**< Content type */
  char * postdata; /**< Body Content */
  int size;
  int used;
};


char docroot[256] = {0};

char * proc_list_json(int flags);
// char * proc_list_json2(int flags);
#ifdef NEED_POSTDATA_ITER
/** 
See: https://www.gnu.org/software/libmicrohttpd/tutorial.html#Processing-POST-data
MHD_PostDataIterator
*/
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
#endif

/** Generic string content sending.
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

#define con_info_need_mem(con_info, cnt) (con_info->used + cnt + 1)

/** Destroy POST Request info freeing all allocated memory.
* It is recommended that this is called in the MHD main handler or req_term_cb() (which ? Both passed to MHD_start_daemon()
* as 5th and 8th params respectively).
* 
*/
void con_info_destroy(CONNINFO *con_info) {
  if (!con_info) { return; }
  if (con_info->postdata) { free(con_info->postdata); }
  if (con_info->conttype) { free(con_info->conttype); }
  free(con_info);
  return;
}
/** Allocate CONNINFO for POST Parsing.
 * CONNINFO is used to keep track of body retrieval / buffering state across multiple calls
 * to MHD answer_to_connection() request handler callback.
 * @param connection - MHD connection / request struct
 * @return App specific HTTP Request object (BODY data reading state mgr)
 */
CONNINFO *con_info_create(struct MHD_Connection *connection) {
  const char * ct   = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Content-type");
  const char * clen = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Content-length");
  fprintf(stderr, "create(): CT: %s, CLEN: %s\n", ct, clen);
  
  int initsize = 128; // Content buffer for con_info.postdata. CANNOT be fixed size)
  
  // Mark content-type and content-length to con_info
  CONNINFO * con_info = (CONNINFO *)calloc (1, sizeof (CONNINFO));
  if (!con_info) { return NULL; } // 0
  // NEW:
  con_info->contlen  = atoi(clen);
  con_info->conttype = strdup(ct); // if (ct) { ... } ?
  // Allow contlen to dictate initial size. Reallocations should not occur w. this.
  if (con_info->contlen) { initsize = con_info_need_mem(con_info, con_info->contlen); }
  // Test against post_body_max (from where ?)
  if (con_info->contlen > 8000) { con_info_destroy(con_info); return NULL; }
  con_info->postdata = (char*)calloc(initsize, sizeof(char)); // TODO: Decide on initial size (config?)
  // con_info->is_parsing = 0; con_info->used = 0; // Redundant w. calloc
  con_info->size = initsize;
  con_info->debug = 1; // TODO: Pass/consider config or simply set outside
  if (con_info->debug) { fprintf(stderr, "create(): con_info =  %p\n", con_info); }
  return con_info;
}



/** Parse POST Body incrementally.
* This gets called multiple times as a result of MHD answer_to_connection() being called multiple times with request types
* that have HTTP Body present.
* 
* The state of request processing and body parsing is reflected in con_info.state in follwing ways:
* - INITED (0) - con_info has been allocated and returned via param list con_cls
* - PARSING (1) - the fragments being passed by calls to answer_to_connection() are being parsed.
* - COMPLETE (2) - when body parsing has been completed, all POST content stored.
* @param connection - MHD Connection
* @param upload_data - Incoming POST data fragment (to add to collected POST data)
* @param upload_data_size - Size of new fragment
* @param con_cls - MHD Request specific user-data, Here: CONNINFO *
* @return MHD_NO for fatal errors that should terminate whole HTTP request, MHD_YES for valid state and progression of parsing.
* A typical usage within answer_to_connection() might look like this:
     ...
*/
int post_body_parse(struct MHD_Connection *connection, const char *upload_data,  size_t *upload_data_size, void ** con_cls) {
  if (!connection) { return MHD_NO; }
  
  CONNINFO * con_info = NULL;
  con_info = *con_cls; // Grab con_info from HMD-conventional void **
  // Enable following if-else blocks to keep more of the parsing state handling here (instead of macros)
  /*
  // Establishing con_cls (last param of request handler (answer_to_connection()))
  if (!*con_cls) {
    con_info = con_info_create(connection);
    if (!con_info) { return MHD_NO; }
    *con_cls = con_info;
  }
  else { con_info = *con_cls; }
  
  ///////////// Transition to parsing and getting (first fragment of) body content /////
  if (!con_info->is_parsing) { con_info->is_parsing = 1; return MHD_YES; } // 1
  */
  // POST Body Chunk/fragment coming in OR all fragments parsed
  // 
  if (!con_info) { return MHD_NO; } // Should always have con_info by now
  
  // New chunk coming (could be INITED,PARSING)
  if (*upload_data_size) {
    int cnt = *upload_data_size;
    int need = con_info_need_mem(con_info, cnt); // (con_info->used + cnt + 1);
    if (need > con_info->size) { // Fixed (off-by-one) >= to > to not realloc unnecessarily
      int newsize = 2*con_info->size; // Advisory, 2X may not be enough (Need: MAX(newsize, need))
      if (need > newsize) { newsize = need; } // Fix to match needed size (if advice is smaller)
      con_info->postdata = (char*)realloc(con_info->postdata, newsize);
      if (!con_info->postdata) { return MHD_NO; }
      con_info->debug && fprintf(stderr, "Realloc from %d to: %d\n", con_info->size, newsize);
      con_info->size = newsize;

    }
    // Receive the post data and write them into the buffer (new: binary append)
    // strncat(con_info->postdata, upload_data, cnt); // string(upload_data, *upload_data_size); // OLD (for text content only)
    memcpy(con_info->postdata + con_info->used, upload_data, cnt); // Tolerates binary and does not need to find end.
    con_info->used += cnt;
    con_info->postdata[con_info->used] = '\0'; // ->used gets us lastusedoffset + 1
    *upload_data_size = 0; // Mark/flag processed / consumed (Per MHD doc)
  }
  else { // Done (*upload_data_size is 0, last call)
    ///////// Data now in con_info->postdata //////////
    con_info->debug && fprintf(stderr, "Got POST Body:\n%s\n", con_info->postdata);
    // *con_cls = NULL; // Mark/flag "Done" ? Set outside because there's no access to con_info w/o *con_cls !
    con_info->is_parsing = 2;
    // Mark length for case contlen == 0 (contlen not gotten from headers) ?
    //if (!con_info->contlen) { con_info->contlen = con_info->used; } // ->size ?
  }
  
  return MHD_YES; // 1;
}
/** Macros for detecting parsing state (from MHD multiple calls to response handler).
* Macros have to be called in the order they appear here (See also example for post_body_parse()).
* This is because they also change the state of the parsing.
* These macros eliminate the calls to parser.
* @todo: Name parsing_* instead of parse_* (?)
*/
// Usage: if (parse_no_state(c)) { return MHD_NO; }
// Do note call parse function before this detection
#define parse_no_state(c) (!c)
// Usage: if (parse_starting(c)) { return MHD_YES; }
// Set *con_cls and Also transition 0 -> 1
#define parse_starting(c) ((!*con_cls) && (!c->is_parsing) && (c->is_parsing = 1) && (*con_cls = c))
// Usage: if (parse_running(c))  { return MHD_YES; }
// Keep state, no transitions
#define parse_running(c)  ((*con_cls) && (c->is_parsing == 1))
// Usage: parse_ending(c) . TODO: consider if we should stick with setting state (2) inside POST parser (NOT macro)
#define parse_ending(c)   ((*con_cls) && (!*upload_data_size) && (c->is_parsing) && (c->is_parsing = 2)) // COMPLETE
/** MHD POST Handler example.
 * Echoes back the request content (w/o parsing it in between).
 */
int answer_to_connection (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  if (!strcmp (method, "GET")) { return send_page(connection, "No GET allowed !!!\n", MHD_HTTP_OK); }
  // Must be POST or PUT w. Body
  
  printf("URL(%s): %s Datasize: %lu, con_cls: %p\n", method, url, *upload_data_size, *con_cls);
  
  CONNINFO * con_info = *con_cls ? (CONNINFO *)*con_cls : con_info_create(connection);
  // Checks that we have con_info
  if (parse_no_state(con_info)) { return MHD_NO; } // 0 con_info_create() failed ?
  /* */
  if (parse_starting(con_info)) { return MHD_YES; } // 1 
  int pret = post_body_parse(connection, upload_data, upload_data_size, con_cls);
  if (!pret) { if (con_info) { con_info_destroy(con_info); } return MHD_NO; } // Fatal error - terminate request
  if (parse_running(con_info))  { return MHD_YES; }
  /* coverity[assign_where_compare_meant] */
  parse_ending(con_info);
  ////////// Handle Request: Send the request content as response content /////////////
  // TODO: Should get handler from  app level (cls) or request level (con_cls). Haldler should be set earlier by ... (?)
  struct MHD_Response *
    response = MHD_create_response_from_buffer(strlen(con_info->postdata), (void*)con_info->postdata, MHD_RESPMEM_MUST_COPY);
  if (con_info) { con_info_destroy(con_info); } // Must free !
  int ok   = MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json"); if (!ok) { return MHD_NO; }
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response); // Model

  return ret;
}

/** Legacy: Generic POST Handler with POST Body parsing.
* See: MHD_post_process and MHD_create_post_processor
* MHD_ContentReaderFreeCallback
* MHD_RequestCompletedCallback set by  MHD_OPTION_NOTIFY_COMPLETED (2 pointer params)
*/
int answer_to_connection1 (void *cls, struct MHD_Connection *connection,
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
* @return MHD_Response (pointer) for static file or NULL
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
  struct MHD_Response * response = MHD_create_response_from_fd(size, fd); // Model
  // Extract suffix
  char * suff = strrchr(url, '.');
  if (suff) { printf("Found suffix: '%s'\n", ++suff); }
  //char * defmime = "text/plain"; // DECLARED_BUT_NOT_REFERENCED
  //MHD_add_response_header (response, "Content-Type", defmime);
  // close(fd);
  return response;
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
/** Extract pid number from (relative) Web URL.
 * @param url - procserver kill process URL
 * @return pid number or 0 on failure to extract pid properly
 */
int pid_extract(const char * url) {
  int pid = 0;
  if (!url) { return 0; }
  int len = strlen(url);
  if (len > 15) {return 0; }
  int i = len-1;
  for(;(i > 4) && isdigit(url[i]);) { printf("Isdig: %d\n", i); i--; }
  printf("Scanned pid start to: %d\n", i);
  if (i) { pid = atoi(&(url[i+1])); }
  return pid;
}
/** MHD Response handler for creating OS Process listing.
* Example of simple GET handling of HTTP Request.
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
  if ( !strcmp(url, "/proclist")) { // , 9  !strncmp(url, "/procs", 6) ||
    //
    ctype = "application/json";
    // struct MHD_Response * create_proclisting_json2(const char * url) {
    json_t * array = proc_list_json2(0); // Produce Process list content (OLD: page = ...)
    page = json_dumps(array, jflags);
    memmode = MHD_RESPMEM_MUST_FREE;
    json_decref(array); // array = NULL; // Model
    if (!page || !*page) {
      printf("Failed to produce content for client !\n"); page = errpage; memmode = MHD_RESPMEM_MUST_COPY; // Error
    }
  }
  else if (!strcmp(url, "/proctree")) { // 6? , 9
    ctype = "application/json";
    proc_t * root = proc_tree();
    json_t * obj = NULL;
    // PW.BRANCH_PAST_INITIALIZATION - event: "A goto jumps past the initialization of a variable"
    if (!root) { page = errpage; memmode = MHD_RESPMEM_MUST_COPY; goto CHECKPAGE; }
    //json_t *
    obj = ptree_json(root, 0);
    if (!obj) { ptree_free(root, 0); page = errpage; memmode = MHD_RESPMEM_MUST_COPY; goto CHECKPAGE; }
    page = json_dumps(obj, jflags);
    memmode = MHD_RESPMEM_MUST_FREE;
    ptree_free(root, 0);  // Model
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
  // Allow cross-domain ref (example from docker headers setup w. daemon cl opts --api-cors-header *)
  
  ok = MHD_add_response_header (response, "Access-Control-Allow-Methods", "HEAD, GET, POST, DELETE, PUT, OPTIONS");
  ok = MHD_add_response_header (response, "Access-Control-Allow-Origin", "*");
  ok = MHD_add_response_header (response, "Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, X-Registry-Auth");
  ok = MHD_add_response_header (response, "Cache-Control", "no-store");
  if (!ok) { httpcode = 203;}
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
void req_term_cb(void *cls, struct MHD_Connection * connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
  fprintf(stderr, "Request Terminated (con_cls=%p, cls=%p) !\n", con_cls, cls); // Was *con_cls: "Directly dereferencing pointer \"con_cls\"."
  if (! con_cls || !*con_cls) { return; } // Important. See manual p. 18
  con_info_destroy((CONNINFO *) *con_cls);
  *con_cls = NULL; // Comm. to MHD
  // NOT: return NULL;
  return;
}

int savepid(json_t * json) {
  if (!json) { return 1; }
  json_t * jpidfn = json_object_get(json, "pidfname");
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

void daemon_prep() {
#include <sys/time.h>
#include <sys/resource.h>
  struct rlimit r;
  getrlimit(RLIMIT_NOFILE, &r);
  // Leave open 0,1,2
  for (int i=3;i<r.rlim_max;i++) { close(i); }
  printf("daemon_prep(): Closed files (>=3), Turn output off 1,2\n");
  //return;
  int fd1 = open("/dev/null", O_RDWR);
  if (fd1 < 0) { return; }
  //dup2(fd1, 0); // Extra trial. if this is dup2()d, There's immediate exit !
  dup2(fd1, 1);
  dup2(fd1, 2);
  int pid = setsid(); // pg leader, session leader
  printf("PG/Sess Leader: %d\n", pid);
}
/* Daemonize
 * - Close files
 * - Set as session leader
 */
void daemon_launch(int port, json_t * json) {
  struct MHD_Daemon * mhd = NULL;
  int log = 1;
  char * logfn = "/tmp/procster.log";
  FILE * fh = stdout;
  if (log) {
    fh = fopen(logfn, "wb");
    setbuf(fh, NULL);
  }
  // Refresh FD:s ? Systemd can capture output
  int piderr = savepid(json);
  if (piderr) { fprintf(fh, "Error %d saving PID\n", piderr); }
  // apc - Accept Policy Callback
  // TODO: Have this as config string (or array) member in JSON file (e.g. "mhdrunflags")
  // MHD_USE_TCP_FASTOPEN, MHD_USE_AUTO (poll mode), MHD_USE_DEBUG == MHD_USE_ERROR_LOG
  int flags = MHD_USE_SELECT_INTERNALLY | MHD_USE_INTERNAL_POLLING_THREAD; 
  mhd = MHD_start_daemon (flags, port, NULL, NULL,
    // NOTE: Connection handler: answer_to_connection*
    &answer_to_connection0, NULL,
    MHD_OPTION_NOTIFY_COMPLETED,
    NULL, NULL, // req_term_cb, userdata MHD Manual p. 18 ()
    
    MHD_OPTION_END); 
  if (NULL == mhd) { fprintf(fh, "Could not start MHD (check if port %d is taken)\n", port); } // return 3;
  fprintf(fh, "Starting Micro HTTP daemon at port=%d, pid=%d (Try: http://localhost:%d/)\n", port, getpid(), port);
  int ok = 100; // MHD_run(mhd); // MHD_YES on succ
  // Note:
  // - Starting w. systemd we always receive EOF (-1) here immediately (!?)
  // Starting from terminal (even with --daemon) we receieve the char we first typed, even if *only*
  // '\n' triggers return (line-buffering).
  //(void) getchar ();
  //int c = getchar();
  int c;
  // 
  while (c = getchar()) {
    if (c == 255) { break; }
  }
  fprintf(fh, "Stopping Daemon (getchar=%d, ok=%d)\n", c, ok);
  MHD_stop_daemon (mhd);
}
/** Main for (Micro HTTP Daemon) process app.
* 5th param to MHD_start_daemon() defines the main connection handler
* (that should do respective dispatching if app handles many different actions).
*/
int main (int argc, char *argv[]) {
  
  if (argc < 2) { printf("No args, pass port. (e.g. %s 8181)\n", argv[0]); return 1; }
  int port = atoi(argv[1]);
  int daemon = 0;
  for (int i=0;i<argc;i++) { if (!strcmp(argv[i], "--daemon")) { daemon = 1; break; }} // printf("%s\n", argv[i]);
  printf("Daemonize=%d (main pid=%d)\n", daemon, getpid());
  char * docr = getcwd(docroot, sizeof(docroot));
  if (!docr) { printf("No docroot gotten (!?)\n"); return 2; }
  printf("Docroot: %s\n", docroot);
  json_error_t error = {0};
  /* the error variable contains error information */
  json_t * json = json_load_file("./procster.conf.json", 0, &error);
  if (!json) { printf("JSON parsing error: %s\n", error.text); }
  else { printf("Parsed JSON: %llu\n", (unsigned long long)json); }
  if (!daemon) { goto RUN; }
  pid_t pid = fork();
  if (pid == 0) { printf("Parent exiting\n"); return 0; }
  daemon_prep(); // In child only
  RUN:
  daemon_launch(port, json);
  
  return 0;
}
