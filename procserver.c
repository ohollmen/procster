/**
* gcc -o procserver procserver.c -lmicrohttpd
* 
* # Additional info
* Google microhttpd examples or see (in Debian/Ubuntu) /usr/share/doc/libmicrohttpd-dev/examples/.
* Response from file:
*     //response = MHD_create_response_from_fd_at_offset64 (sbuf.st_size, fd, 0);
* https://lists.gnu.org/archive/html/libmicrohttpd/2014-10/msg00006.html - Important notes on parsing ANY POST body
* (Not just x-www-form-urlencoded or multipart/form-data) - Still a bit unclear and unpractical
* https://pastebin.com/SjVCkKAW
*/

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <microhttpd.h>
#define MHD_PLATFORM_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// MUST be min 256
// #define POSTBUFFERSIZE  512
#define POSTBUFFERSIZE  256
// Example ONLY
enum ConnectionType {GET = 0,POST = 1};
enum ParsingState {INITED =0, PARSING, DONE};
typedef struct connection_info_struct CONNINFO;
struct connection_info_struct {
  enum ConnectionType connectiontype;
  struct MHD_PostProcessor *postprocessor;
  // FILE *fp;
  const char *answerstring;
  int answercode;
  int is_parsing;
  char * postdata;
  int size;
  int used;
};

char * proc_list_json(int flags);
char * proc_list_json2(int flags);
/** 
See: https://www.gnu.org/software/libmicrohttpd/tutorial.html#Processing-POST-data
MHD_PostDataIterator
*/
static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, 
	      uint64_t off, size_t size)
{
  CONNINFO *con_info = coninfo_cls;
  printf("Got: %s\n", data);
  // Fill answer string
  
  return MHD_YES;
}
/** Generic string content sending.
* @todo Add len for binary data (allow -1 to measure string)
*/
static int send_page (struct MHD_Connection *connection,  const char* page, int status_code) {
  int ret;
  struct MHD_Response *response;
  response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_MUST_COPY);
  if (!response) { return MHD_NO; }
  // Correct place for setting headers
  // MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
  ret = MHD_queue_response (connection, status_code, response);
  MHD_destroy_response (response);
  return ret;
}
// @return true when body parsing has been completed.
int post_body_parse(void) {}

/** Generic POST Body parsing.
*/
int answer_to_connection (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  if (!strcmp (method, "GET")) {return send_page(connection, "No GET allowed !!!\n", MHD_HTTP_OK);}
  // Must be POST
  const char * ct   = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Content-type");
  const char * clen = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, "Content-length");
  printf("URL(%s): %s (Datasize: %lu, CT: %s, CLEN: %s)\n", method, url, *upload_data_size, ct, clen);
  CONNINFO *con_info = NULL;
  // Establishing con_cls (last param of request handler (answer_to_connection()))
  // Pass connection, *upload_data, *upload_data_size, ** con_cls, 
  // TODO: Return private values, not MHD_... (or 
  if (!*con_cls) {
    int initsize = 128;
    // TODO: Possibly Mark content-type to con_info
    con_info = calloc (1, sizeof (CONNINFO));
    if (!con_info) { return MHD_NO; } // 0
    con_info->postdata = (char*)calloc(initsize, sizeof(char)); // TODO: Decide on initial size (config?)
    // con_info->is_parsing = 0; con_info->used = 0; // Redundant w. calloc
    con_info->size = initsize;
    *con_cls = con_info;
  }
  else { con_info = *con_cls; }
  ///////////// Transition to parsing and getting (first fragment of) body content /////
  if (!con_info->is_parsing) { con_info->is_parsing = 1; return MHD_YES; } // 1
  // POST Body Chunk/fragment coming in OR all fragments parsed
  else { // else is redundant
    // New chunk coming
    if (*upload_data_size) {
      int cnt = *upload_data_size;
      int need = (con_info->used + cnt + 1);
      if (need >= con_info->size) {
        int newsize = 2*con_info->size; // 2X may not be enough
	if (need > newsize) { newsize = need; }
        con_info->postdata = realloc(con_info->postdata, newsize);
	if (!con_info->postdata) { return MHD_NO; } // Check what should be returned
	fprintf(stderr, "Realloc from %d to: %d\n", con_info->size, newsize);
	con_info->size = newsize;
	
      }
      // Receive the post data and write them into the bufffer
      strncat(con_info->postdata, upload_data, cnt); // string(upload_data, *upload_data_size);
      con_info->used += cnt;
      *upload_data_size = 0; // Mark/flag processed / consumed ?
      return MHD_YES;
    }
    else { // Done !
      // *upload_data_size == 0 so all data have been received
      //output = "Received data:\n\n";
      //output += connection_data->read_post_data.str();
      ///////// Data now in con_info->postdata //////////
      fprintf(stderr, "Got POST Body:\n%s\n", con_info->postdata);

      //delete connection_data;
      //connection_data = NULL;
      *con_cls = NULL; // Mark/flag "Done" ?
      con_info->is_parsing = 2;
      // return 1;
    }
  }
  struct MHD_Response *
    response = MHD_create_response_from_buffer(strlen(con_info->postdata), (void*)con_info->postdata, MHD_RESPMEM_MUST_COPY);
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json");
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

  MHD_destroy_response(response);

  return ret;
}

int answer_to_connection0 (void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {
  // const char *page  = "<html><body>Hello, browser!</body></html>";
  struct MHD_Response * response = NULL;
  int ret; // = MHD_YES;
  printf("URL(%s): %s (Datasize: %lu)\n", method, url, *upload_data_size);
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
    char * page = proc_list_json2(0); // Produce Content
    // MHD_add_response_header (response, "Content-Type", "application/json");
    response = MHD_create_response_from_buffer (
      strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT); // MHD_RESPMEM_MUST_COPY
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    // if (ret != MHD_YES) { } // Don't check, just return (per examples, see below)
    MHD_destroy_response(response);
    return ret;
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
}
int main (int argc, char *argv[]) {
  struct MHD_Daemon * mhd = NULL;
  if (argc < 2) { printf("No args, pass port."); return 1; }
  int port = atoi(argv[1]);
  // apc - Accept Policy Callback
  int flags = MHD_USE_SELECT_INTERNALLY;
  mhd = MHD_start_daemon (flags, port, NULL, NULL,
    &answer_to_connection, NULL, MHD_OPTION_END);
  if (NULL == mhd) {printf("Could not start\n");return 2;}
  printf("Starting Micro HTTP daemon at port %d\n", port);
  (void) getchar ();
  MHD_stop_daemon (mhd);
  return 0;
}
