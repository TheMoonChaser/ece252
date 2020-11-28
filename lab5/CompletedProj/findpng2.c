#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <curl/multi.h>
#include <getopt.h>
#include <sys/types.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <search.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include "queue.h"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define VISITED_SIZE 3000
#define MAX_WAIT_MSECS 30*1000 /* Wait max. 30 seconds */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9
#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef unsigned char U8;

ENTRY e;
ENTRY *ep;
struct hsearch_data htab;
Queue* queue;
char** fragment_urls; 			  //store all fragment urls
char** visited_urls;			  //store all visited urls 
int visited_counter;			  //track visited urls counter
int PNG_counter;				  //count number of PNGs already got
int max_png;					  //copy of m
int frontier_counter;


typedef struct recv_buf2 {
     char *buf;       /* memory to hold a copy of received data */
     size_t size;     /* size of valid data in buf in bytes*/
     size_t max_size; /* max capacity of buf in bytes*/
     int seq;         /* >=0 sequence number extracted from http header */
                      /* <0 indicates an invalid seq number */
} RECV_BUF;


htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)  ;
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf);
void* get_fragment_urls(void* thread_input);
int is_png(U8 *buf, size_t n);

int is_png(U8 *buf, size_t n){
    U8 correct_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    for(size_t i = 0; i < n; i++){
       if(correct_sig[i] != buf[i]){
          return 0;
       }
    }
    return 1;
}

htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);

    if ( doc == NULL ) {
        //fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
	}
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{

    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        //printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
        //printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        //printf("No result\n");
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
			   e.key = (char *)href;
			   int err = hsearch_r(e, FIND, &ep, &htab);
			   //if not found in visited
               if(err == 0){
				  int already_in_queue = 0;
                  //check whether already in the queue
                  QueueNode* start = queue->head;
				  while(start != NULL){
				     if(strcmp(start->data, (char*)href) == 0){
					    already_in_queue = 1;
						break;
				     }
					 start = start->next; 
				  }
				  if(already_in_queue == 0){
	                 char* unvisited_url = malloc(256*sizeof(char));
                     strcpy(unvisited_url, (char*)href);
				     Queue_AddToHead(queue, unvisited_url);
					 frontier_counter++;
				  }
			   }		   
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
    strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

    /* extract img sequence number */
    p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

/**
  * @brief write callback function to save a copy of received data in RAM.
  *        The received libcurl data are pointed by p_recv,
  *        which is provided by libcurl and is not user allocated memory.
  *        The user allocated memory is at p_userdata. One needs to
  *        cast it to the proper struct to make good use of it.
  *        This function maybe invoked more than once by one invokation of
  *        curl_easy_perform().
  */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
    return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be positive */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
    return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        recv_buf_cleanup(ptr);
}

/**
  * @brief create a curl easy handle and set the options.
  * @param RECV_BUF *ptr points to user data needed by the curl write call back function
  * @param const char *url is the target url to fetch resoruce
  * @return a valid CURL * handle upon sucess; NULL otherwise
  * Note: the caller is responsbile for cleaning the returned curl handle
  */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        //fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

	/* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    int follow_relative_link = 1;
    char *url = NULL;

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url);
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
	   //check whether the PNG url already exist
	   for(int i = 0; i < PNG_counter; i++){
          if(strcmp(fragment_urls[i], eurl) == 0){
	         return 0;
		  }
       }
	   
       U8 sig[PNG_SIG_SIZE];

       //read signature
       memcpy(sig, p_recv_buf->buf, PNG_SIG_SIZE);
       int png_signature_is_correct = is_png(sig, PNG_SIG_SIZE);

       //if signature not correct, output error
       if(!png_signature_is_correct){
		  return 0;
	   }
       if(PNG_counter < max_png){
          strcpy(fragment_urls[PNG_counter], eurl);
          PNG_counter++;
	   }
    }

    return 0;
}

/**
  * @brief process teh download data by curl
  * @param CURL *curl_handle is the curl handler
  * @param RECV_BUF p_recv_buf contains the received data.
  * @return 0 on success; non-zero otherwise
  */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
    }

    if ( response_code >= 400 ) {
        //fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    } else {
        //fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf);
    }

    return 0;
}

int main( int argc, char** argv){
   double times[2];                   //for time count
   struct timeval tv;                 //for time count
   int c;
   int t = 1;                         //max number of easy handler
   int m = 50;                        //max number of pngs
   char log_file_name[256];           //LOGFILE
   memset(log_file_name, '\0', 100);
   char* SEED_URL = argv[argc-1]; 
   char *str = "option requires an argument";
   visited_counter = 0;
   PNG_counter = 0;
  
   //Timer start.
   if (gettimeofday(&tv, NULL) != 0) {
      perror("gettimeofday");
      abort();
   }
   times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
 
   //handle inputs
   while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
      switch (c) {
      case 't':
         t = strtoul(optarg, NULL, 10);
         if (t <= 0) {
            fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
            return -1;
         }
         break;

      case 'm':
         m = strtoul(optarg, NULL, 10);
         if (m <= 0 || m > 100) {
            fprintf(stderr, "%s: %s greater than 0 and less than or equal to 100 -- 'n'\n", argv[0], str);
            return -1;
         }
         break;

	  case 'v':
         strcpy(log_file_name, optarg);
		 break;
         
      default:
         return -1;
      }
   }

   //copy m
   max_png = m;

   //initialize frontier counter
   frontier_counter = 0;

   //Initialize hash table
   //htab = calloc(VISITED_SIZE, sizeof(ENTRY));
   memset(&htab, 0, sizeof(struct hsearch_data));
   hcreate_r(VISITED_SIZE, &htab);

   //initialize Queue
   queue = Queue_Init(OnQueueIncreasedEvent);
   char* initial_url = malloc(256*sizeof(char));
   strcpy(initial_url, SEED_URL);
   Queue_AddToHead(queue, initial_url); 
   frontier_counter++;

   //initialize m fragment urls store
   fragment_urls = malloc((m*sizeof(char*)));
   for(int i = 0; i < m; i++){
      fragment_urls[i] = malloc(256*sizeof(char));
      memset(fragment_urls[i], '\0', 256);
   }

   //initialize visited urls store
   visited_urls = malloc((VISITED_SIZE*sizeof(char*)));
   
   //initialize libcurl before any thread
   curl_global_init(CURL_GLOBAL_DEFAULT);

   //initialize necessary variables
   CURLM *cm=NULL;
   CURL *eh=NULL;
   CURLMsg *msg=NULL;
   CURLcode return_code=0;
   int still_running=0, msgs_left=0;

   //initialize multi curl
   cm = curl_multi_init();
   
   //Find PNG urls
   while(PNG_counter < max_png && queue->head != NULL){
       int counter = 0;
	   if(frontier_counter > t){
         counter = t;
	   } else {
		 counter = frontier_counter; 
	   }
	   int const recv_buf_size = counter;
	   RECV_BUF recv_buf_array[recv_buf_size];
       CURL* curl_handle_array[recv_buf_size];
	   for(int i = 0; i < counter; i++){
  	      CURL *curl_handle;
  	  	  char *url;
  	  	  
  	  	  url = (char*)Queue_GetFromTail(queue); 
          frontier_counter--;
  	  	  //put the url into hash table
  	      e.key = url;
  	  	  e.data = (void*)url;
  	  	  int resp = hsearch_r(e, ENTER, &ep, &htab);
  	  	  //no more room
  	  	  if(resp == 0){
  	  	    //fprintf(stderr, "entry failed\n");
  	  	  } else {
  	        //put the url into visited store
  	        visited_urls[visited_counter] = url;
  	  	    visited_counter++; 
  	  	  }
  	  	  curl_handle = easy_handle_init(&recv_buf_array[i], url);
		  curl_handle_array[i] = curl_handle;
  	      if( curl_handle == NULL ) {
  	        //fprintf(stderr, "Curl initialization failed. Exiting...\n");
  	      }
		  curl_multi_add_handle(cm, curl_handle);
       }
	   /* get it! */
       curl_multi_perform(cm, &still_running);

	   do {
           int numfds=0;
           int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
           if(res != CURLM_OK) {
               //fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
               return EXIT_FAILURE;
           }
           curl_multi_perform(cm, &still_running);

       } while(still_running);
	   
       while ((msg = curl_multi_info_read(cm, &msgs_left))) {
         if (msg->msg == CURLMSG_DONE) {
		    int i;
            eh = msg->easy_handle;
	        /* process the download data */
            for(i = 0; i < counter; i++){
	            if(curl_handle_array[i] == eh){
                    process_data(eh, &recv_buf_array[i]);
					break;
			    }
		    }
            return_code = msg->data.result;
            if(return_code!=CURLE_OK) {
                //fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                continue;
            }

            curl_multi_remove_handle(cm, eh);
            /* cleaning up */
            cleanup(eh, &recv_buf_array[i]);

         } else {
            //fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
         }
	   }
   }

   //get_fragment_urls(NULL);

   if(!(log_file_name[0] == '\0')){
      //Open a file
      FILE *f = fopen(log_file_name, "w");
      if(f == NULL){
         printf("Unable to open file!\n");
      } else {
         for(int i = 0; i < visited_counter; i++){
            //put all visited urls into a file
		    fprintf(f, "%s\n", visited_urls[i]);
		 }
		 //Close the file
		 fclose(f); 
	  }
   }

   //put all fragment urls into a file
   //Open a file
   FILE *f_png = fopen("png_urls.txt", "w");
   if(f_png == NULL){
      printf("Unable to open file!\n");
   } else {
      for(int i = 0; i < PNG_counter ; i++){
         //Writes to the file
         if(!(fragment_urls[i][0] == '\0')) {
            fprintf(f_png, "%s\n", fragment_urls[i]);
	     }
      }
      //Close the file
      fclose(f_png);
   }

   //deallocate fragment urls
   for(int i = 0; i < m; i++){
      free(fragment_urls[i]);
   }
   free(fragment_urls);

   //deallocate visited urls
   for(int i = 0; i < visited_counter; i++){
      free(visited_urls[i]);
   }
   free(visited_urls);

   //Clean up
   void* data = NULL;
   data = Queue_GetFromHead(queue);
   while(data != NULL){
	  free(data);
	  data = NULL;
      data = Queue_GetFromHead(queue);
   }
   free(queue);
   //Queue_Free(queue, true);
   hdestroy_r(&htab);
   curl_multi_cleanup(cm);
   curl_global_cleanup();

   //time count
   if (gettimeofday(&tv, NULL) != 0) {
       perror("gettimeofday");
       abort();
   }
   times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
   printf("findpng2 execution time: %.2lf seconds\n", times[1] - times[0]);

   return 0;
}
