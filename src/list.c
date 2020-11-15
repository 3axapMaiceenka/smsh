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

	list->tail->next = last;
	list->tail = last;
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
