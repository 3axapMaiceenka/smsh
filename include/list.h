#ifndef LIST_H
#define LIST_H

struct Node
{
	void* data;
	struct Node* next;
	struct Node* prev;
};

struct List
{
	struct Node* head;
	struct Node* tail;
	void (*free_data)(void*);
};

struct List* create_list(void (*free_data)(void*));
void remove_node(struct List* list, void* data); 
void push_back(struct List* list, void* data);
void push_forward(struct List* list, void* data);
void destroy_list(struct List** list);
void free_elements(struct Node** head, void (*free_data)(void*)); // is called inside destroy_list

#endif