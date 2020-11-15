#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <getopt.h>
#include <semaphore.h>
#include "catpng.h"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
int fragment_counter = 0; //counts the number of fragments completed
int fragment_numbers[50]; //store the fragment numbers
/*
 *   Use semaphore to handle the sychronization issue.
 * */
sem_t sem;

typedef struct recv_buf2 {
      char *buf;       /* memory to hold a copy of received data */
      size_t size;     /* size of valid data in buf in bytes*/
      size_t max_size; /* max capacity of buf in bytes*/
      int seq;         /* >=0 sequence number extracted from http header */
                       /* <0 indicates an invalid seq number */
} RECV_BUF;


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);


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
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata) {
   int realsize = size * nmemb;
   RECV_BUF *p = (RECV_BUF*)userdata;

   if (realsize > (int)strlen(ECE252_HEADER) &&
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
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata) {
   size_t realsize = size * nmemb;
   RECV_BUF *p = (RECV_BUF *)p_userdata;

   if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
       /* received data is not 0 terminated, add one byte for terminating 0 */
       size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
       char *q = (char*) realloc(p->buf, new_size);
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

int recv_buf_init(RECV_BUF *ptr, size_t max_size) {
   void *p = NULL;

   if (ptr == NULL) {
      return 1;
   }

   p = malloc(max_size);
   if (p == NULL) {
      return 2;
   }

   ptr->buf = (char*) p;
   ptr->size = 0;
   ptr->max_size = max_size;
   ptr->seq = -1;              /* valid seq should be non-negative */
   return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr) {
   if (ptr == NULL) {
      return 1;
   }

   free(ptr->buf);
   ptr->size = 0;
   ptr->max_size = 0;
   return 0;
}

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len) {
   FILE *fp = NULL;

   if (path == NULL) {
       fprintf(stderr, "write_file: file name is null!\n");
       return -1;
   }

   if (in == NULL) {
       fprintf(stderr, "write_file: input data is null!\n");
       return -1;
   }

   fp = fopen(path, "wb");
   if (fp == NULL) {
       perror("fopen");
       return -2;
   }

   if (fwrite(in, 1, len, fp) != len) {
       fprintf(stderr, "write_file: imcomplete write!\n");
       return -3;
   }
   return fclose(fp);
}

/**
 * This is a function to create thread
 * Use cUrl to get the fragement numbers and file date to store in files
 * Create a file for a specific fragement of the image
 */

void* get_fragment(void* thread_input){
   int server_num = 1;
   char **server_urls = (char**) thread_input;
   
   while(fragment_counter < 50){
      CURL *curl_handle;
      CURLcode res;
      RECV_BUF recv_buf;
      char fname[256];

      //initialize buffer
      recv_buf_init(&recv_buf, BUF_SIZE);

      /* init a curl session */
      curl_handle = curl_easy_init();

      if (curl_handle == NULL) {
          fprintf(stderr, "curl_easy_init: returned NULL\n");
      }

      /* specify URL to get */
      curl_easy_setopt(curl_handle, CURLOPT_URL, server_urls[server_num-1]);

      /* register write call back function to process received data */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);

      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

      /* register header call back function to process received header data */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);

      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

      /* some servers requires a user-agent field */
      curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

      /* get it! */
      res = curl_easy_perform(curl_handle);

      if( res != CURLE_OK) {
          fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      }

      /* check whether the sequence number already exists */
	  sem_wait(&sem);
      if(fragment_numbers[recv_buf.seq] == -1){
         fragment_numbers[recv_buf.seq] = recv_buf.seq;
         sprintf(fname, "./%d.png", recv_buf.seq+1);
         write_file(fname, recv_buf.buf, recv_buf.size);
         fragment_counter += 1;
      }
	  sem_post(&sem);

      /* cleaning up */
      curl_easy_cleanup(curl_handle);
      recv_buf_cleanup(&recv_buf);

      //for using different server
	  server_num += 1;

      if(server_num == 4){
         server_num = 1;
      }
   }
   return NULL;
}

/**
 * Handle inputs
 * Modify urls based on user inputs
 * Create threads to get data
 * Cat pngs
 */ 
int main(int argc, char **argv) {
   int c;
   int t = 1;   //number of thread
   int n = 1;   //image number
   char *str = "option requires an argument";
   memset(&fragment_numbers, -1, sizeof(fragment_numbers));

   //handle inputs
   while ((c = getopt (argc, argv, "t:n:")) != -1) {
      switch (c) {
      case 't':
         t = strtoul(optarg, NULL, 10);
         if (t <= 0) {
            fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
            return -1;
         }
         break;

      case 'n':
         n = strtoul(optarg, NULL, 10);
         if (n <= 0 || n > 3) {
            fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
            return -1;
         }
         break;   

	  default:
         return -1;
      }
   }

   char **modified_url;
   modified_url = malloc(3*sizeof(char*));
   for(int i = 0; i < 3; i++){
      modified_url[i] = malloc(256*sizeof(char));
   }
   
   //modify urls 
   sprintf(modified_url[0], "http://ece252-1.uwaterloo.ca:2520/image?img=%d", n);
   sprintf(modified_url[1], "http://ece252-2.uwaterloo.ca:2520/image?img=%d", n);
   sprintf(modified_url[2], "http://ece252-3.uwaterloo.ca:2520/image?img=%d", n);
   
   //initialize semaphore
   sem_init(&sem,0,1);

   //initialize threads
   const int thread_num = t;
   pthread_t tid[thread_num];

   //initialize libcurl before any thread
   curl_global_init(CURL_GLOBAL_DEFAULT);
	
   //create threads
   int current_thread_num = t;
   for (int i=0;i<t;i++){
      if (pthread_create(&tid[i],NULL,get_fragment,modified_url) != 0){
	     current_thread_num = i;
		 //Thread num should be from 0 to t-1
		 printf("Thread #%d failed to create!\n",i);
		 //This issue could be mainly a capacity issue because number of threads we can generate is limited by ECEubuntu server. 
		 //We need to stop generating new threads.
		 break;
	  }
   }

   //join threads based on how many threads we generated.
   for (int i=0;i<current_thread_num;i++){
      pthread_join(tid[i],NULL);
   }
   
   //Use for single thread.
   //get_fragment(modified_url);

   //store 50 fragment files names
   char **fragment_files;
   fragment_files = malloc((fragment_counter)*sizeof(char*));
   for(int i = 0; i < (fragment_counter); i++){
      fragment_files[i] = malloc(256*sizeof(char));
      sprintf(fragment_files[i], "./%d.png", i+1);
   }

   //concate png segments into a single png file
   cat_png(fragment_files, fragment_counter);

   //printf("%d", fragment_counter);
   //deallocate and remove helper files
   for(int i = 0; i < (fragment_counter); i++){
      remove(fragment_files[i]);
      free(fragment_files[i]);
   }
   free(fragment_files);

   //deallocate urls
   for(int i = 0; i < 3; i++){
      free(modified_url[i]);
   }
   free(modified_url);

   sem_destroy(&sem);
   curl_global_cleanup();
   pthread_exit(0);
}

