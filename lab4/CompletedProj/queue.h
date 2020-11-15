/*********************************************************************************
 *  Author：代浩然
 *	Time：2017-08-05
 *	Description：该文件为队列的实现，实现方式是链表的双向队列，可以从链表的两端存，也可以从链表的两端取
 *
 *
 *********************************************************************************/

#pragma once
#ifndef _QUEUE_H
#define _QUEUE_H
#ifdef __cplusplus
extern "C" {
#endif
 
#ifndef bool
#define bool int
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

typedef int(*QueueIncreased)(void* queue,void* data);
 
typedef struct _QueueNode{
	void* data;
	struct _QueueNode* next;
	struct _QueueNode* prior;
}QueueNode;

typedef struct _Queue{
	QueueNode* head;
	QueueNode* tail;
	unsigned long length;
	QueueIncreased onQueueIncreased;
}Queue;

Queue* Queue_Init(QueueIncreased queueIncreasedEvent);
int Queue_AddToHead(Queue* queue,void* data);
int Queue_AddToTail(Queue* queue,void* data);
void* Queue_GetFromHead(Queue* queue);
void* Queue_GetFromTail(Queue* queue);
void Queue_Free(Queue* queue,bool isFreeData);
int OnQueueIncreasedEvent(void* queue,void* data);

#ifdef __cplusplus
}
#endif
#endif
