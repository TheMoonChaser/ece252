#include "queue.h"
#include <stdlib.h>
#include <malloc.h>

Queue* Queue_Init(QueueIncreased queueIncreasedEvent)
{
	Queue* queue = NULL;
	queue = (Queue*)malloc(sizeof(Queue));
	if(queue == NULL)
	{
		return NULL;
	}
	queue->length = 0;
	queue->head = NULL;
	queue->tail = NULL;
	queue->onQueueIncreased = queueIncreasedEvent;
	return queue;
}

int Queue_AddToHead(Queue* queue,void* data)
{
	QueueNode* node = NULL;
	if(queue == NULL)
		return -1;
	node = (QueueNode*)malloc(sizeof(QueueNode));
	if(node == NULL)
		return -1;
	node->data = data;

	if(queue->tail == NULL && queue->head == NULL)
	{
		queue->tail = node;
		queue->head = node;
		node->next = NULL;
		node->prior = NULL;
	}
	else
	{
		queue->head->prior = node;
		node->next = queue->head;
		node->prior = NULL;
		queue->head = node;
	}
	queue->length++;
	if(queue->onQueueIncreased != NULL)
		return queue->onQueueIncreased(queue,data);
	return 0;
}

int Queue_AddToTail(Queue* queue,void* data)
{
	QueueNode* node = NULL;
	if(queue == NULL)
		return -1;
	node = (QueueNode*)malloc(sizeof(QueueNode));
	if(node == NULL)
		return -1;
	node->data = data;
	if(queue->tail == NULL && queue->head == NULL)
	{
		queue->tail = node;
		queue->head = node;
		node->next = NULL;
		node->prior = NULL;
	}
	else
	{
		queue->tail->next = node;
		node->prior = queue->tail;
		node->next = NULL;
		queue->tail = node;
	}
	queue->length++;
	if(queue->onQueueIncreased != NULL)
		return queue->onQueueIncreased(queue,data);
	return 0;
}

void* Queue_GetFromHead(Queue* queue)
{
	void* data = NULL;
	QueueNode* node = NULL;
	if(queue == NULL || queue->head == NULL)
	{
		return NULL;
	}
	node = queue->head;
	queue->head = node->next;
	if(queue->head != NULL)
	{
		queue->head->prior = NULL;
	}
	else
	{
		queue->tail = NULL;
		queue->head = NULL;
	}
	data = node->data;
	free(node);
	queue->length--;
	return data;
}


void* Queue_GetFromTail(Queue* queue)
{
	void* data = NULL;
	QueueNode* node = NULL;
	if(queue == NULL || queue->tail == NULL)
	{
		return NULL;
	}
	node = queue->tail;
	queue->tail = node->prior;
	if(queue->tail != NULL)
	{
		queue->tail->next = NULL;
	}
	else
	{
		queue->tail = NULL;
		queue->head = NULL;
	}
	data = node->data;
	free(node);
	queue->length--;
	return data;
}

void Queue_Free(Queue* queue,bool isFreeData)
{
	void* data = NULL;
	data = Queue_GetFromHead(queue);
	while(data != NULL)
	{
		if(isFreeData)
			free(data);
	}
	free(queue);

}


int OnQueueIncreasedEvent(void* queue,void* data)
{
	Queue* q = (Queue*)queue;
	char **p = (char**)data;
	return 0;
}
