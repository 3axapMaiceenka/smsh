#include "hashtable.h"
#include "utility.h"
#include <string.h>

size_t hashf(struct Hashtable* hashtable, const char* key)
{
	size_t hash = 1315423911;

	while (*key)
	{
		hash ^= ((hash << 5) + *key + (hash >> 2));
		key++;
	}

	return (hash % hashtable->capacity);
}

struct Hashtable* create_hashtable(size_t capacity)
{
	struct Hashtable* hashtable = calloc(1, sizeof(struct Hashtable));

	hashtable->table = calloc(capacity, sizeof(struct Bucket*));
	hashtable->capacity = capacity;
	hashtable->size = 0;

	return hashtable;
}

void destroy_hashtable(struct Hashtable* hashtable)
{
	for (size_t i = 0, count = 0; i < hashtable->capacity && count < hashtable->size; i++)
	{
		if (hashtable->table[i])
		{
			count += hashtable->table[i]->size;
			destroy_list(&hashtable->table[i]->entries);
			free(hashtable->table[i]);
		}
	}

	free(hashtable);
}

void insert(struct Hashtable* hashtable, const char* key, const char* value)
{
	size_t indx = hashf(hashtable, key);

	struct Entry* entry = malloc(sizeof(struct Entry)); 
	entry->key = copy_string(key);
	entry->value = copy_string(value);

	if (!hashtable->table[indx])
	{
		hashtable->table[indx] = calloc(1, sizeof(struct Bucket));
		hashtable->table[indx]->entries = create_list(&free_entry);
	}

	push_back(hashtable->table[indx]->entries, entry);
	hashtable->table[indx]->size++;
	hashtable->size++;
}

struct Entry* get(struct Hashtable* hashtable, const char* key)
{
	size_t indx = hashf(hashtable, key);

	if (!hashtable->table[indx])
	{
		return NULL;
	}

	struct Entry* entry = NULL;

	for (struct Node* node = hashtable->table[indx]->entries->root; node; node = node->next)
	{
		entry = (struct Entry*)(node->data);

		if (!strcmp(entry->key, key))
		{
			break;
		}
	}

	return entry;
}

// entry is a pointer to struct Entry
void free_entry(void* entry)
{
	struct Entry* e = (struct Entry*)entry;

	if (e)
	{
		free(e->key);
		free(e->value);
		free(e);
	}
}