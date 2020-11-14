#include "list.h"
#include <stdlib.h>

struct List* create_list(void (*free_data)(void*))
{
	struct List* list = calloc(1, sizeof(struct List));
	list->free_data = free_data;

	return list;
}

struct Node* push_back(struct List* list, void* data)
{
	struct Node* last = malloc(sizeof(struct Node));
	last->data = data;
	last->next = NULL;

	if (!list->root)
	{
		list->root = last;
		return last;
	}

	struct Node* temp = list->root;
	for (; temp->next; temp = temp->next)
		;

	temp->next = last;

	return last;
}

void destroy_list(struct List** list)
{
	if (*list)
	{
		free_elements(&(*list)->root, (*list)->free_data);
		free(*list);
		*list = NULL;
	}
}

void free_elements(struct Node** root, void (*free_data)(void*))
{
	if (*root)
	{
		if ((*root)->next)
		{
			free_elements(&(*root)->next, free_data);
			free((*root)->next);
		}

		free_data((*root)->data);
	}
}
