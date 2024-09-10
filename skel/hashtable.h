/*
 * Copyright (c) 2024, Maxim Anca-Stefania <ancastef7@gmail.com>
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef struct node {
    void *data;
    struct node *next, *prev;
} Node;

typedef struct linked_list_t
{
	Node* head, *tail;
	unsigned int data_size;
	unsigned int size;
} linked_list_t;

typedef struct info info;
struct info {
	void *key;
	void *value;
};

typedef struct hashtable_t hashtable_t;
struct hashtable_t {
	linked_list_t **buckets;  // Array de liste simplu-inlantuite.
	// Nr. total de noduri existente curent in toate bucket-urile.
	unsigned int size;
	unsigned int hmax; /* Nr. de bucket-uri. */
	// (Pointer la) Functie pentru a calcula valoarea hash asociata cheilor.
	unsigned int (*hash_function)(void*);
	// (Pointer la) Functie pentru a compara doua chei.
	int (*compare_function)(void*, void*);
	// (Pointer la) Functie pentru a elibera memoria ocupata de cheie si valoare.
	void (*key_val_free_function)(void*);
	// (Pointer la) Functie pentru a copia un element
	void (*copy_func)(void **, void *, unsigned int);
};

linked_list_t* ll_create(unsigned int data_size);

void ll_add_nth_node(linked_list_t* list, unsigned int n, void* new_data);

Node* ll_remove_nth_node(linked_list_t* list, unsigned int n);

unsigned int ll_get_size(linked_list_t* list);

void ll_free(linked_list_t** pp_list);

int compare_function_strings(void *a, void *b);

void key_val_free_function(void *data);

hashtable_t *ht_create(unsigned int hmax, unsigned int (*hash_function)(void*),
		int (*compare_function)(void*, void*),
		void (*key_val_free_function)(void*),
		void (*copy_func)(void **, void *, unsigned int));

int ht_has_key(hashtable_t *ht, void *key);

void *ht_get(hashtable_t *ht, void *key);

void node_copy(void **dst, void *src, unsigned int src_size);

void simple_copy(void **dst, void *src, unsigned int src_size);

void ht_put(hashtable_t *ht, void *key, unsigned int key_size,
	void *value, unsigned int value_size);

void ht_remove_entry(hashtable_t *ht, void *key);

void ht_free(hashtable_t *ht);

unsigned int ht_get_size(hashtable_t *ht);

unsigned int ht_get_hmax(hashtable_t *ht);

#endif
