#include "SortedList.h"
#include <pthread.h>
#include <string.h>
#include <sched.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	if( list == NULL || list == NULL ) 
		return;
	SortedListElement_t *current = list;
	while( current->next != list ) {
		if( strcmp( element->key, current->next->key ) <= 0 ) 
			break;
		current = current->next;
	}
	if( opt_yield & INSERT_YIELD )
		sched_yield();
	element->next = current->next;
	element->prev = current;
	current->next = element;
	current->next->prev = element;
}

int SortedList_delete( SortedListElement_t *element) {
	// Check corruption
	if( element != NULL && element->next->prev == element->prev->next ) {
		if( opt_yield & DELETE_YIELD )
			sched_yield();
		element->prev->next = element->next;
		element->next->prev = element->prev;
		return 0;
	}
	return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	if( list == NULL || key == NULL )
		return NULL;
	SortedListElement_t *current = list;
	while( current->next != list ) {
		if( strcmp( current->key, key ) == 0 )
			return current;
		if( opt_yield & LOOKUP_YIELD )
			sched_yield();
		current = current->next;
	}
	return NULL;
}

int SortedList_length(SortedList_t *list) {
	if( list == NULL )
		return -1;
	int length = 0;
	SortedListElement_t *current = list;
	while( current->next != list ) {
		length++;
		if( opt_yield & LOOKUP_YIELD )
			sched_yield();
		current = current->next;
	}
	return length;
}
