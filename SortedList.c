//NAME: Michael Bustillos
//EMAIL: mdbust24@icloud.com
//ID: 304929353

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "SortedList.h"
#include <signal.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{    
  if (list == NULL)
  {
      fprintf(stderr, "NULL element breaks list\n");
      exit(2);
  }
    if (opt_yield & INSERT_YIELD)
    {
        sched_yield();
    }
    
    SortedListElement_t* new_item = (SortedListElement_t *) malloc(sizeof(SortedListElement_t));
    if (new_item == NULL)
    {
      fprintf(stderr, "Not enough memory to allocate to new element\n");
        exit(1);
    }    
    
    new_item->key = element -> key;
    if (list -> prev == list && list-> next == list)
    {
        list -> next = new_item;
        list -> prev = new_item;
        new_item -> next = list;
        new_item -> prev = list;
        return;
    }
    
    SortedListElement_t * traverse = list;
    //head has data to be null
    //traverse -> key < toBeInserted -> key
    //if empty list
    
    const char* list_key =(traverse -> next) -> key;
    const char* new_key = new_item ->key;
    while (traverse -> next != list )
    {
      if (strcmp(list_key,new_key) <=0)
      {
        break;
      }
        traverse = traverse -> next;
    }
    //now traverse has to put to the element on it next
    SortedListElement_t* nextElement = traverse -> next;
    traverse -> next = new_item;
    nextElement -> prev = new_item;
    new_item -> prev = traverse;
    new_item -> next = nextElement;
}

int SortedList_delete( SortedListElement_t *element)
{
  if (element -> key == NULL)
  {
      fprintf(stderr, "Don't delete head\n");
      exit(2);
    }
    
    if (opt_yield & DELETE_YIELD)
    { 
        sched_yield();
    }
    
    if (element -> next -> prev != element || element -> prev -> next != element)
    {
        return 1;
    }
    
    element -> next -> prev = element -> prev;
    element -> prev -> next = element -> next;

    free (element); //this way allocated
    
    return 0;
}



SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
  if (list == NULL)
  {
    fprintf(stderr, "Can't search NULL\n");
    exit(1);
  }
  if (key == NULL)
  {
    fprintf(stderr, "Looking for dummny node\n");
    exit(1);
  }
    if (opt_yield & LOOKUP_YIELD)
    { //before the critical section
        sched_yield();
    }
    SortedListElement_t* traverse = list -> next;
    while (traverse -> key != NULL)
    {
        if (strcmp(traverse -> key, key) == 0)
        {
            return traverse;
        }
        traverse = traverse -> next;
    }
    return NULL;
}


int SortedList_length(SortedList_t *list)
{
  int length = 0;
    if (opt_yield & LOOKUP_YIELD)
    {
        sched_yield();
    }
    SortedListElement_t* traverse = list -> next; //get the second element
    while (traverse != list) {
        length++;
        traverse = traverse -> next;
    }
    return length;
}