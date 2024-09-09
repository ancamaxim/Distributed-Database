/*
 * Copyright (c) 2024, <>
 */

#include <stdio.h>
#include "server.h"
#include "lru_cache.h"

#include "utils.h"

static response
*server_edit_document(server_t *s, char *doc_name, char *doc_content) {
    response *resp;
    resp = calloc(1, sizeof(response));
    DIE(!resp, "malloc() failed");
    resp->server_id = s->server_id;
    resp->server_response = calloc(1, MAX_RESPONSE_LENGTH);
    DIE(!resp->server_response, "malloc() failed");
    resp->server_log = calloc(1, MAX_LOG_LENGTH);
    DIE(!resp->server_log, "malloc() failed");

    void *evicted_key = NULL;

    // stocare doc in cache
    bool is = lru_cache_put(s->cache, doc_name, doc_content, &evicted_key);
    
    // cheia se afla deja in cache
    if (is == false) {
        sprintf(resp->server_response, MSG_B, doc_name);
        sprintf(resp->server_log, LOG_HIT, doc_name);
        // adaugare in database
        ht_put(s->database, doc_name, MAX_RESPONSE_LENGTH, doc_content, MAX_RESPONSE_LENGTH);
    }
    else {
        // verificare database
        if (ht_has_key(s->database, doc_name))
            sprintf(resp->server_response, MSG_B, doc_name);
        else
            sprintf(resp->server_response, MSG_C, doc_name);
        // verificare cache full
        if (evicted_key) {
            sprintf(resp->server_log, LOG_EVICT, doc_name, ((info *)evicted_key)->key);
            ht_put(s->database, ((info *)evicted_key)->key, MAX_RESPONSE_LENGTH,
            ((info *)evicted_key)->value, MAX_RESPONSE_LENGTH);
            free(((info *)evicted_key)->key);
            free(((info *)evicted_key)->value);
            free(evicted_key);
        }
        else
            sprintf(resp->server_log, LOG_MISS, doc_name);
        // adaugare in database
        ht_put(s->database, doc_name, MAX_RESPONSE_LENGTH, doc_content, MAX_RESPONSE_LENGTH);
    }
    return resp;
}

static response
*server_get_document(server_t *s, char *doc_name) {
    response *resp = calloc(1, sizeof(response));
    resp->server_id = s->server_id;
    resp->server_response = calloc(1, MAX_RESPONSE_LENGTH);
    resp->server_log = calloc(1, MAX_LOG_LENGTH);

    char *doc_content = lru_cache_get(s->cache, doc_name);
    if (doc_content) {
        strcpy(resp->server_response, doc_content);
        sprintf(resp->server_log, LOG_HIT, doc_name);
    }
    else {
        doc_content = ht_get(s->database, doc_name);
        if (doc_content) {
            void *evicted_key = NULL;
            strcpy(resp->server_response, doc_content);
            lru_cache_put(s->cache, doc_name, doc_content, &evicted_key);
            if (evicted_key) {
                sprintf(resp->server_log, LOG_EVICT, doc_name, ((info *)evicted_key)->key);
                ht_put(s->database, ((info *)evicted_key)->key, MAX_RESPONSE_LENGTH,
                ((info *)evicted_key)->value, MAX_RESPONSE_LENGTH);
                free(((info *)evicted_key)->key);
                free(((info *)evicted_key)->value);
                free(evicted_key);
            } else {
                sprintf(resp->server_log, LOG_MISS, doc_name);
            }
        } else {
            free(resp->server_response);
            resp->server_response = NULL;
            sprintf(resp->server_log, LOG_FAULT, doc_name);
        }
    }
    return resp;
}

server_t *init_server(unsigned int cache_size) {
    server_t *server = calloc(1, sizeof(server_t));
    server->cache = init_lru_cache(cache_size);
    server->task = q_create(sizeof(request), TASK_QUEUE_SIZE);
    server->database = ht_create(100, hash_string, compare_function_strings, key_val_free_function, simple_copy);
    return server;
}

response *server_handle_request(server_t *s, request *req) {

    if (req->type == EDIT_DOCUMENT) {
        request *new_req = calloc(1, sizeof(request));
        new_req->type = EDIT_DOCUMENT;

        new_req->doc_name = calloc(1, MAX_RESPONSE_LENGTH);
        strcpy(new_req->doc_name, req->doc_name);
        
        new_req->doc_content = calloc(1, MAX_RESPONSE_LENGTH);
        strcpy(new_req->doc_content, req->doc_content);

        q_enqueue(s->task, new_req);
    
        free(new_req);

        response *resp = calloc(1, sizeof(response));
        resp->server_id = s->server_id;
        resp->server_response = calloc(1, MAX_RESPONSE_LENGTH);
        resp->server_log = calloc(1, MAX_LOG_LENGTH);
        
        sprintf(resp->server_response, MSG_A, get_request_type_str(req->type), req->doc_name);
        sprintf(resp->server_log, LOG_LAZY_EXEC, s->task->size);

        return resp;
    }
    else {
        request *elem;
        while(s->task->size > 0) {
            elem = q_front(s->task);
            response *resp = server_edit_document(s, elem->doc_name, elem->doc_content);
            PRINT_RESPONSE(resp);
            q_dequeue(s->task);
        }
        if (req->type == GET_DOCUMENT)
            return server_get_document(s, req->doc_name);       
        return NULL;
    }
}

void free_server(server_t **s) {
    if ((*s)->server_id < 100000) {
        free_lru_cache(&((*s)->cache));
        ht_free((*s)->database);
        q_free((*s)->task);
    }
    free(*s);
    *s = NULL;
}
