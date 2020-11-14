#ifndef LIST_H
#define LIST_H

struct List
{
	void* data;
	struct List* next;
};

struct List* push_back(struct List** root, void* data); // returns pointer to the last member
void destroy_list(struct List** root);

#endif