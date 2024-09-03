/*
 * Copyright (c) 2024, <>
 */

#include <stdio.h>
#include <string.h>
#include "lru_cache.h"
#include "hashtable.h"

#include "utils.h"
#include "server.h"

lru_cache *init_lru_cache(unsigned int cache_capacity) {
    /* TODO */
    lru_cache *cache = malloc(sizeof(lru_cache));
    cache->capacity = cache_capacity;
    cache->size = 0;
    cache->list = ll_create(sizeof(info));
    cache->name_node_map = ht_create(100, hash_string, compare_function_strings, key_val_free_function, node_copy);
    return cache;
}

bool lru_cache_is_full(lru_cache *cache) {
    if (cache->capacity == cache->size)
        return true;
    return false;
}

void free_lru_cache(lru_cache **cache) {
    ll_free(&((*cache)->list));
    ht_free((*cache)->name_node_map);
    free(*cache);
}

void move_to_front(lru_cache *cache, void *node) {
    Node *curr = (Node *)node;
    if (curr == NULL || curr->next == NULL)
        return;
    if (curr == cache->list->head) {
        curr->next->prev = NULL;
        cache->list->tail->next = curr;
        cache->list->head = curr->next;
    }
    else {
        curr->prev->next = curr->next;
        curr->next->prev = curr->prev;
    }
    curr->prev = cache->list->tail;
    curr->next = NULL;
    cache->list->tail->next = curr;
    cache->list->tail = curr;
}

bool lru_cache_put(lru_cache *cache, void *key, void *value,
                   void **evicted_key) {
    bool res;
    // cheia nu se afla in cache, trebuie adaugata
    if (lru_cache_get(cache, key) == NULL) {
        res = true;
        if (cache->capacity == cache->size) {

            Node *head = cache->list->head;
            char *out_key = (char *)(((info *)(head->data))->key);
            char *out_value = (char *)(((info *)(head->data))->value);
            *evicted_key = calloc(1, sizeof(info));
            simple_copy(&((*((info **)evicted_key))->key), out_key, MAX_RESPONSE_LENGTH);
            simple_copy(&((*((info **)evicted_key))->value), out_value, MAX_RESPONSE_LENGTH);

            // stergere head, adaugare la final
            head = ll_remove_nth_node(cache->list, 0);
            ht_remove_entry(cache->name_node_map, out_key);
            cache->size--;
        }
        cache->size++;
        cache->list->size++;
        // adaugare in linked list

        Node *new = calloc(1, sizeof(Node));
        new->data = calloc(1, sizeof(info));
        simple_copy(&(((info *)(new->data))->key), key, MAX_RESPONSE_LENGTH);
        simple_copy(&(((info *)(new->data))->value), value, MAX_RESPONSE_LENGTH);

        new->prev = cache->list->tail;
        if (cache->list->head == NULL)
            cache->list->head = new;
        else
            cache->list->tail->next = new;
        cache->list->tail = new;
    }
    else {
        move_to_front(cache, ht_get(cache->name_node_map, key));
        strcpy((char *)(((info *)(cache->list->tail->data))->value), (char *)value);
        res = false;
    }
    
    // adaugare / actualizare in ht_node_name map
    ht_put(cache->name_node_map, key, strlen((char *)key), cache->list->tail, sizeof(cache->list->tail));
    
    return res;
}

void *lru_cache_get(lru_cache *cache, void *key) {
    Node *node = ht_get(cache->name_node_map, key);
    if (node == NULL)
        return NULL;
    if(node == cache->list->tail)
        return ((info *)(node->data))->value;
    move_to_front(cache, node);
    return ((info *)(node->data))->value;
}

void lru_cache_remove(lru_cache *cache, void *key) {
    char *value = lru_cache_get(cache, key);
    if (!value)
        return;

    // sterge nodul din cache
    cache->size--;
    Node *curr = ht_get(cache->name_node_map, key);
    if(curr == cache->list->head) {
        cache->list->head = cache->list->head->next;
        if (cache->list->head)
            cache->list->head->prev = NULL;
        free(curr);
    }
    else if (curr == cache->list->tail) {
        cache->list->tail = cache->list->tail->prev;
        if (cache->list->tail)
            cache->list->tail->next = NULL;
        free(curr);
    }
    else {
        curr->prev->next = curr->next;
        curr->next->prev = curr->prev;
        free(curr);
    }
    ht_remove_entry(cache->name_node_map, key);

}
