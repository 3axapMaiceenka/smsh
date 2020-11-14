#ifndef LISH_H
#define LIST_H

struct Node
{
	void* data;
	struct Node* next;
};

// TODO: add pointer to the last element
struct List
{
	struct Node* root;
	void (*free_data)(void*);
};

struct List* create_list(void (*free_data)(void*));
struct Node* push_back(struct List* list, void* data); // returns pointer to the last member
void destroy_list(struct List** list);
void free_elements(struct Node** root, void (*free_data)(void*)); // is called inside destroy_list

#endif