#include "list.h"
#include <stdlib.h>

struct List* push_back(struct List** root, void* data)
{
	struct List* last = malloc(sizeof(struct List));
	last->data = data;
	last->next = NULL;

	if (!*root)
	{
		*root = last;
		return last;
	}

	struct List* temp = *root;
	for (; temp->next; temp = temp->next)
		;

	temp->next = last;

	return last;
}

void destroy_list(struct List** root)
{
	if (*root)
	{
		if ((*root)->next)
		{
			destroy_list(&(*root)->next);
			free((*root)->next);
		}

		if ((*root)->data)
		{
			free((*root)->data);
		}

		free(*root);
		*root = NULL;
	}
}
