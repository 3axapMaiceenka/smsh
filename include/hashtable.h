#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include "list.h"

struct Entry
{
	char* key;
	char* value;
};

struct Bucket
{
	struct List* entries;
	size_t size;
};

struct Hashtable
{
	struct Bucket** table;
	size_t size;
	size_t capacity;
};

size_t hashf(struct Hashtable* hashtable, const char* key);
struct Hashtable* create_hashtable(size_t capacity);
void destroy_hashtable(struct Hashtable* hashtable);
void insert(struct Hashtable* hashtable, const char* key, const char* value); 
struct Entry* get(struct Hashtable* hashtable, const char* key);
void free_entry(void* entry); // entry is a pointer to struct Entry

#endif