/** @file
* ## Coverity models
* 
* Model free()ing functions for branching data structures, where
* coverity does not recognize the free()ing functions or
* their behavior.
* Allocating and freeing functions should be modeled as pairs
* 
* Try compile by (Also: -of ):
* - Test Compile: gcc -c covmodels.c
*   - Be prepared for "warning: implicit declaration of function ..."
* - cov-make-library --output-file covint/covmodels.xmldb covmodels.c
* 
* See headers:
* - /usr/include/microhttpd.h
* - /usr/include/jansson.h
* - /usr/include/glib-2.0/glib/gslist.h
* # TODO
* Create headers for  __coverity_... functions ?
*/
/*
#include <microhttpd.h> // MHD
#include <unistd.h> // close()
#include <jansson.h> // json_decref
#include <glib.h> // g_slist_free
//#include <glib-2.0/glib.h>
*/
#define MHD_YES 1
typedef void * json_t;
typedef void * GSList;

struct MHD_Response *
MHD_create_response_from_buffer (size_t size, void *buffer,
    enum MHD_ResponseMemoryMode mode) {
  struct MHD_Response * response;
  __coverity_mark_as_afm_allocated__(response, "MHD_destroy_response");
  return response;
}

struct MHD_Response * MHD_create_response_from_fd (size_t size, int fd) {
  struct MHD_Response * response;
  __coverity_mark_as_afm_allocated__(response, "MHD_destroy_response");
  //close(fd); // Fancier notation ?
  __coverity_close__(fd);
  return response;
}


void MHD_destroy_response(struct MHD_Response *response) {
  __coverity_mark_as_afm_freed__(response, "MHD_destroy_response");
  return;
}


int MHD_add_response_header (struct MHD_Response *response,
                         const char *header,
                         const char *content) {
  return MHD_YES;
}

//json_decref is JSON_INLINE  => error: redefinition of json_decref
/*
void json_decref(json_t *json) {
  // This model may be needed instead of json_delete()
  // ... as inline json_decref() has conditionality
  __coverity_free__(json);
  return;
}
*/
// json_delete
void json_delete(json_t *json) {
  __coverity_free__(json);
}
// g_slist_free
void g_slist_free(GSList *list) {
  __coverity_free__(list);
  return;
}
