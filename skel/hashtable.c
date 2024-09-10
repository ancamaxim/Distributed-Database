/*
 * Copyright (c) 2024, Maxim Anca-Stefania <ancastef7@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"
#include "utils.h"

linked_list_t* ll_create(unsigned int data_size)
{
	linked_list_t *list = (linked_list_t *)malloc(sizeof(linked_list_t));
	DIE(!list, "malloc() failed");
	list->head = list->tail = NULL;
	list->data_size = data_size;
	list->size = 0;
	return list;
}

// adauga un nou nod cu nod->data = new_data pe pozitia n;
void ll_add_nth_node(linked_list_t* list, unsigned int n, void* new_data)
{
	if (!list)
		return;
	Node *new_node = (Node *)malloc(sizeof(Node));
	DIE(!new_node, "malloc() failed");
	new_node->data = new_data;
	new_node->next = NULL;
	if (n == 0 || list->head == NULL) {
		// head-ul e inlocuit
		Node *aux = list->head;
		list->head = new_node;
		new_node->next = aux;
		list->size++;
		if (list->size == 1)
			list->tail = new_node;
	} else {
		Node *p = list->head;
		if (n > list->size)
			n = list->size;
		for (unsigned int i = 0; i < n && p->next != NULL; ++i)
			p = p->next;
		new_node->next = p->next;
		p->next = new_node;
		if (n == list->size)
			list->tail = new_node;
		list->size++;
	}
}

// elimina nodul de pe pozitia n, daca n < 0 se ignora, daca e mai mare decat
// numarul de noduri din lista, se elimina ultimul;
Node* ll_remove_nth_node(linked_list_t* list, unsigned int n)
{
	if (n == 0 && list->head != NULL) {
		// trebuie eliminat headul;
		Node *p = list->head;
		list->head = list->head->next;
		list->size--;
		p->next = NULL;
		if (list->head == NULL)
			list->tail = NULL;
		return p;
	} else if (n > 0) {
		Node *p = list->head;
		if (n > list->size)
			n = list->size - 1;
		for (unsigned int i = 0; i < n - 1 && p->next != NULL; ++i)
			p = p->next;
		Node *q = p->next;
		if (q != NULL) {
			if (q->next == NULL)
				list->tail = p;
			p->next = q->next;
			q->next = NULL;
			list->size--;
			return q;
		}
		else
			return NULL;
	}
	return NULL;
}

// Functia intoarce nr de noduri din lista
unsigned int ll_get_size(linked_list_t* list)
{
	if (!list)
		return -1;
	return list->size;
}

// elibereaza tot
void ll_free(linked_list_t** pp_list)
{
	if (!pp_list || !*pp_list)
		return;
	Node *aux;
	while ((*pp_list)->head) {
		aux = ll_remove_nth_node((*pp_list), 0);
		free(((info *)(aux->data))->key);
		free(((info *)(aux->data))->value);
		free(aux->data);
		aux->data = NULL;
		free(aux);
		aux = NULL;
	}
	free(*pp_list);
	*pp_list = NULL;
}

int compare_function_ints(void *a, void *b)
{
	int int_a = *((int *)a);
	int int_b = *((int *)b);
	if (int_a == int_b)
		return 0;
	else if (int_a < int_b)
		return -1;
	return 1;
}

int compare_function_strings(void *a, void *b)
{
	char *str_a = (char *)a;
	char *str_b = (char *)b;
	return strcmp(str_a, str_b);
}

// elibereaza memoria folosita de data;
// in acest caz stiu ca lucrez cu structuri de tipul info;
void key_val_free_function(void *data) {
	info *pair = (info*)data;
	if (pair->key != NULL) {
		free(pair->key);
		pair->key = NULL;
	}
	if (pair->value != NULL) {
		free(pair->value);
		pair->value = NULL;
	}
	free(pair);
}

// creeaza un hashtable;
hashtable_t *ht_create(unsigned int hmax, unsigned int (*hash_function)(void*),
		int (*compare_function)(void*, void*),
		void (*key_val_free_function)(void*),
		void (*copy_func)(void **, void *, unsigned int))
{
	hashtable_t *ht = (hashtable_t*)malloc(sizeof(hashtable_t));
	DIE(!ht, "malloc() failed");
	ht->hmax = hmax;
	ht->buckets = (linked_list_t **)malloc(hmax * sizeof(linked_list_t*));
	DIE(!ht->buckets, "malloc() failed");
	for (unsigned int i = 0; i < ht->hmax; ++i) {
		ht->buckets[i] = malloc(sizeof(linked_list_t));
		if (!ht->buckets[i]) {
			DIE(!ht->buckets[i], "malloc() failed");
			for (int j = i - 1; j >= 0; --j)
				free(ht->buckets[i]);
			free(ht->buckets);
			free(ht);
			return NULL;
		}
		ht->buckets[i]->head = NULL;
		ht->buckets[i]->data_size = sizeof(info);
		ht->buckets[i]->size = 0;
	}
	ht->hash_function = hash_function;
	ht->compare_function = compare_function;
	ht->key_val_free_function = key_val_free_function;
	ht->copy_func = copy_func;
	return ht;
}

// verifica tot tabelul dupa cheie;
int ht_has_key(hashtable_t *ht, void *key)
{
	unsigned int index = ht->hash_function(key) % ht->hmax;
	Node *node = ht->buckets[index]->head;
	while (node) {
		if (ht->compare_function(((info*)(node->data))->key, key) == 0)
			return 1;
		node = node->next;
	}
	return 0;
}

// intoarce pointerul catre data asociata cheii key;
void *ht_get(hashtable_t *ht, void *key)
{
	if (!ht)
		return NULL;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	Node *node = ht->buckets[index]->head;
	while (node) {
		info *pair = (info *)(node->data);
		if (ht->compare_function(pair->key, key) == 0)
			return pair->value;
		node = node->next;
	}
	return NULL;
}

void node_copy(void **dst, void *src, unsigned int src_size) {
	*dst = src;
	(void)src_size;
}

void simple_copy(void **dst, void *src, unsigned int src_size) {
	*dst = calloc(1, src_size);
	memcpy((*dst), src, src_size);
}

// incarca / updateaza un camp din hashtable;
void ht_put(hashtable_t *ht, void *key, unsigned int key_size,
	void *value, unsigned int value_size)
{
	unsigned int index = ht->hash_function(key) % ht->hmax;
	Node *node = ht->buckets[index]->head;
	while (node) {
		if (!ht->compare_function(((info*)(node->data))->key, key)) {
			if (((info *)node->data)->value != value &&
				strcmp(((info *)node->data)->value, value)) {
				free(((info*)(node->data))->value);
				((info *)node->data)->value = calloc(1, value_size);
				memcpy(((info *)node->data)->value, value, value_size);
			}
			return;
		}
		node = node->next;
	}
	// nu am gasit deci e noua cheia asta
	info *data = (info *)calloc(1, sizeof(info));
	DIE(!data, "malloc() failed");
	data->key = calloc(1, key_size);
	strcpy(data->key, key);
	ht->copy_func(&data->value, value, value_size);
	ht->size++;
	ll_add_nth_node(ht->buckets[index], 0, data);
}

// elibereaza un camp din hashtable pe baza cheii
void ht_remove_entry(hashtable_t *ht, void *key)
{
	if (!ht)
		return;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	Node *node = ht->buckets[index]->head;
	for (int i = 0; node; i++) {
		if (ht->compare_function(((info*)(node->data))->key, key) == 0) {
			// am gasit cheia
			Node *aux = ll_remove_nth_node(ht->buckets[index], i);
			if (ht->copy_func == simple_copy) {
				free(((info *)aux->data)->value);
				((info *)aux->data)->value = NULL;
			}
			free(((info*)(node->data))->key);
			((info*)(node->data))->key = NULL;
			free(aux->data);
			aux->data = NULL;
			free(aux);
			aux = NULL;
			if (ht->buckets[index]->size == 0)
				ht->size--;
			return;
		}
		node = node->next;
	}
}

void ht_free(hashtable_t *ht)
{
	for (unsigned int i = 0; i < ht->hmax; ++i) {
		Node* node = ht->buckets[i]->head;
		while(node) {
			if(node->data) {
				Node *aux = ll_remove_nth_node(ht->buckets[i], 0);
				if (aux && aux->data) {
					if (ht->copy_func == simple_copy) {
						free(((info *)aux->data)->value);
						((info *)aux->data)->value = NULL;
					}
					free(((info *)aux->data)->key);
					((info *)aux->data)->key = NULL;
					free(aux->data);
					aux->data = NULL;
					free(aux);
					aux = NULL;
				}
			}
			node = ht->buckets[i]->head;
		}
		free(ht->buckets[i]);
	}
	free(ht->buckets);
	free(ht);
}

unsigned int ht_get_size(hashtable_t *ht)
{
	if (!ht)
		return 0;
	return ht->size;
}

unsigned int ht_get_hmax(hashtable_t *ht)
{
	if (!ht)
		return 0;

	return ht->hmax;
}
