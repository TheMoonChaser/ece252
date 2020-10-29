#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240  /* 1024*10 = 10K */

typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                    /* <0 indicates an invalid seq number */
} RECV_BUF;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int sizeof_recv_buf(size_t nbytes);
int recv_buf_init(RECV_BUF *ptr, size_t nbytes);
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

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

/**
  * @brief calculate the actual size of RECV_BUF
  * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
  * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
  */
int sizeof_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure.
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

int recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }

    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */

    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr){
	if (ptr == NULL){
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

int write_file(const char *path, const void *in, size_t len)
{
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

//download images
void produce(char** server_url){
    int server_num = 1;	
    //initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    char url[256];

    
    while(*counter < 50){  
        CURL *curl_handle;
	    CURLcode res;
        RECV_BUF recv_buf;

        //initialize buffer
        recv_buf_init(&recv_buf, BUF_SIZE);

		//initialize url everytime to make sure url is not overwrite.
		memset(url,'\0',sizeof(url));

        //semwait
		//if *counter >= 50, sempost, recv_buff_clean_up, then break to exit the while loop!!!!!!!!!!
        //concate server_url[server_num-1] with *count to get url
        //*count++
        //sempost
        
        /* init a curl session */
        curl_handle = curl_easy_init();
      
        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return;
        }
      
        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
      
        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
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
		/* Put struct recv_buf to Producer-consumer buffer. 
		 * Cannot put recv_buf's pointer because it will be cleanuped later*/


        /* cleaning up */
        curl_easy_cleanup(curl_handle);
		// cleanup recv_buf 
        recv_buf_cleanup(&recv_buf);
		// for using different server
		server_num+=1;
		if (server_num == 4){
			server_num = 1;
		}	
     }
	 //Detach all shared memory here becuase producer process will not have more things to do. 
	 //Ex. counter,sems,producer-consumer buffer, etc.
	 shmdt(counter);
	 //cleanup curl_global
	 curl_global_cleanup();
}

void consume(){
}

int main(int argc, char* argv[])
{
    if (argc < 6) {
       fprintf(stderr, "Usage: %s B P C X N\n", argv[0]);
       exit(1);
    }
	
    
    //handle inputs
    int B = atoi(argv[1]);       //number of segments stored in buff
    int P = atoi(argv[2]);       //number of producers
    int C = atoi(argv[3]);       //number of consumers
    int X = atoi(argv[4]);       //consumer sleep time in ms
    int N = atoi(argv[5]);       //image number
     
    //Image URLs
	char **modified_url;
    modified_url = malloc(3*sizeof(char*));
    for(int i = 0; i < 3; i++){
        modified_url[i] = malloc(256*sizeof(char));
    }

    //modify urls
    sprintf(modified_url[0], "http://ece252-1.uwaterloo.ca:2530/image?img=%d&part=", N);
    sprintf(modified_url[1], "http://ece252-2.uwaterloo.ca:2530/image?img=%d&part=", N);
    sprintf(modified_url[2], "http://ece252-3.uwaterloo.ca:2530/image?img=%d&part=", N);

    const int NUM_CHILD = P + C;
    int i = 0;
    pid_t pid = 0;
    pid_t cpids[NUM_CHILD];
    int state;
    double times[2];
    struct timeval tv;

	//Timer start.
	if (gettimeofday(&tv, NULL) != 0) {
         perror("gettimeofday");
         abort();
    }
	times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

	// Declaration of all elements in shared memory
    int *count;             //shared count fragement

    /* allocate shared memory regions */
    int shmid_count = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    /* attach to shared memory regions */
    count = (int*)shmat(shmid_count, NULL, 0);
    /* initialize shared memory variables */
    *count = 0;

    //Create processes.
    for ( i = 0; i < NUM_CHILD; i++) {

        pid = fork();

        if ( pid > 0 ) {             /* parent process */
            cpids[i] = pid;
        } else if ( pid == 0 && i < P) {     /* child process for producer */
            produce(modified_url);
            break;
		} else if ( pid == 0 && i >= P ) {   /* child process for consumer */
	        consume();
            break;
        } else {
            perror("fork");
            abort();
        }

    }

    if ( pid > 0 ) {                 /* parent process */
        for ( i = 0; i < NUM_CHILD; i++ ) {
            waitpid(cpids[i], &state, 0);
            if (WIFEXITED(state)) {
                printf("Child cpid[%d]=%d terminated with state: %d.\n", i, cpids[i], state);
            }
        }
		//For parent process, we need to concate the image.


        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);
        
        //deallocate urls
        for(int i = 0; i < 3; i++){
            free(modified_url[i]);
        }
        free(modified_url);
    }
    return 0;
}
