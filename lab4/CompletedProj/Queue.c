#include "queue.h"
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main( int argc, char** argv){

	void* data = NULL;
	Queue* queue = NULL;
	char* i = (char*)"1";
	char* j = (char*)"2";
	char* k = (char*)"3";
	char* l = (char*)"4";
	char* p = NULL;
	queue = Queue_Init(OnQueueIncreasedEvent);
	Queue_AddToHead(queue,i);
	Queue_AddToHead(queue,j);
	Queue_AddToTail(queue,k);
	Queue_AddToTail(queue,l);
	data = Queue_GetFromHead(queue);
	while(data != NULL)
	{
		p = (char *)data;
		printf("%s\t", p);
		data = Queue_GetFromHead(queue);//从头取
	}
	printf("\n");
	Queue_Free(queue,true);
	queue = Queue_Init(NULL);
	Queue_AddToHead(queue,i);
	Queue_AddToHead(queue,j);
	Queue_AddToTail(queue,k);
	Queue_AddToTail(queue,l);
	data = Queue_GetFromHead(queue);
	while(data != NULL)
	{
		p = (char *)data;
		printf("%s\t", p);
		data = Queue_GetFromTail(queue);//从头取
	}
	printf("\n");
 
	Queue_AddToTail(queue,i);
	Queue_AddToTail(queue,j);
	Queue_AddToTail(queue,k);
	Queue_AddToTail(queue,l);
	data = Queue_GetFromHead(queue);
	while(data != NULL)
	{
		p = (char *)data;
		printf("%s\t", p);
		data = Queue_GetFromHead(queue);//从头取
	}
	printf("\n");
	Queue_Free(queue,true);
	//getchar();
	return 0;
}
