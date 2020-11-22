#include "hashtable.h"
#include "utility.h"
#include <string.h>

static size_t hashf(struct Hashtable* hashtable, const char* key)
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

	return hashtable;
}

static void free_entry(void* entry)
{
	struct Entry* e = (struct Entry*)entry;
	free(e->key);
	free(e->value);
	free(e);
}

static struct Entry* _get(struct Hashtable* hashtable, const char* key, size_t indx)
{
	if (!hashtable->table[indx])
	{
		return NULL;
	}

	struct Entry* entry = NULL;
	struct Node* node = hashtable->table[indx]->entries->head;

	for (; node; node = node->next)
	{
		entry = (struct Entry*)(node->data);

		if (!strcmp(entry->key, key))
		{
			break;
		}
	}

	return node ? entry : NULL;
}

char** get(struct Hashtable* hashtable, const char* key)
{
	struct Entry* entry = _get(hashtable, key, hashf(hashtable, key));

	if (entry)
	{
		return &entry->value;
	}

	return NULL;
}

void insert(struct Hashtable* hashtable, const char* key, const char* value)
{
	size_t indx = hashf(hashtable, key);

	if (_get(hashtable, key, indx))
	{
		return;
	}

	struct Entry* entry = malloc(sizeof(struct Entry)); 
	entry->key = copy_string(key);
	entry->value = copy_string(value);

	if (!hashtable->table[indx])
	{
		hashtable->table[indx] = calloc(1, sizeof(struct Bucket));
		hashtable->table[indx]->entries = create_list(free_entry);
		hashtable->size++;
	}

	push_back(hashtable->table[indx]->entries, entry);
	hashtable->table[indx]->size++;
}

static void destroy_bucket(struct Bucket** bucket)
{
	destroy_list(&(*bucket)->entries);
	free(*bucket);
	*bucket = NULL;
}

void destroy_hashtable(struct Hashtable** hashtable)
{
	if (*hashtable)
	{
		struct Hashtable* h = *hashtable;

		for (size_t i = 0, count = 0; i < h->capacity && count < h->size; i++)
		{
			if (h->table[i])
			{
				count++;
				destroy_bucket(&h->table[i]);
			}
		}

		free(*hashtable);
		*hashtable = NULL;
	}
}

void erase(struct Hashtable* hashtable, const char* key)
{
	size_t indx = hashf(hashtable, key);
	struct Entry* entry = _get(hashtable, key, indx);

	if (entry)
	{
		if (!(--hashtable->table[indx]->size))
		{
			hashtable->size--;
			destroy_bucket(&hashtable->table[indx]);
		}
		else
		{
			remove_node(hashtable->table[indx]->entries, entry);
		}
	}
}