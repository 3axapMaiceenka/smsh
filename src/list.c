#include "list.h"
#include <stdlib.h>

struct List* create_list(void (*free_data)(void*))
{
	struct List* list = calloc(1, sizeof(struct List));
	list->free_data = free_data;

	return list;
}

void push_back(struct List* list, void* data)
{
	struct Node* last = calloc(1, sizeof(struct Node));
	last->data = data;

	if (!list->head)
	{
		list->head = list->tail = last;
		return;
	}

	last->prev = list->tail;
	list->tail->next = last;
	list->tail = last;
}

void push_forward(struct List* list, void* data)
{
	struct Node* first = calloc(1, sizeof(struct Node));
	first->data = data;

	if (!list->head)
	{
		list->head = list->tail = first;
		return;
	}

	first->next = list->head;
	list->head->prev = first;
	list->head = first;
}

static struct Node* find(struct List* list, void* data)
{
	if (data)
	{
		for (struct Node* node = list->head; node; node = node->next)
		{
			if (node->data == data)
			{
				return node;
			}
		}	
	}

	return NULL;
}

void remove_node(struct List* list, void* data)
{
	struct Node* node = find(list, data);

	if (node)
	{
		struct Node** prev = list->head != node ? &node->prev->next : &list->head;
		struct Node** next = list->tail != node ? &node->next->prev : &list->tail;

		*prev = node->next;
		*next = node->prev;

		list->free_data(data);
	}
}

void destroy_list(struct List** list)
{
	if (*list)
	{
		free_elements(&(*list)->head, (*list)->free_data);
		free(*list);
		*list = NULL;
	}
}

void free_elements(struct Node** head, void (*free_data)(void*))
{
	if (*head)
	{
		if ((*head)->next)
		{
			free_elements(&(*head)->next, free_data);
			free((*head)->next);
		}

		free_data((*head)->data);
	}
}
