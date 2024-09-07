/*
 * Copyright (c) 2024, <>
 */

#include "load_balancer.h"
#include "server.h"

load_balancer *init_load_balancer(bool enable_vnodes) {
	load_balancer *main = calloc(1, sizeof(load_balancer));
    DIE(!main, "malloc() failed");
    main->servers = calloc(MAX_SERVERS, sizeof(server_t *));
    main->hashring = calloc(MAX_SERVERS, sizeof(int));
    main->size = 0;
    main->hash_function_docs = hash_string;
    main->hash_function_servers = hash_uint;
    return main;
}

void loader_add_server(load_balancer* main, int server_id, int cache_size) {
    unsigned int index = main->size;
    unsigned int hash = main->hash_function_servers(&server_id);
    // cautare server cu hash-ul minim, mai mare decat hash-ul nou introdus
    for (unsigned int i = 0; i < main->size; i++) {
        if (hash < main->servers[i]->hash) {
            index = i;
            break;
        } else if (hash == main->servers[i]->hash) {
            if (server_id < main->servers[i]->server_id)
                index = i;
            else
                index = i + 1;
            break;
        }
    }
    // mutare server nou pe pozitia index
    if (main->size > 0 && index != main->size) {
        for (int i = main->size; i >= (int) (index + 1); i--)
            main->servers[i] = main->servers[i - 1];
    }
    main->servers[index] = init_server(cache_size);
    main->servers[index]->server_id = server_id;
    main->servers[index]->hash = hash;

    // mutare documente pe server-ul nou introdus
    // parcurgem documentele de pe server-ul succesor, si daca
    // hash_document < hash_new_server, il mutam

    main->size++;
    if (main->size > 1) {
        unsigned int succ;
        succ = (index + 1) % main->size;

        // executie comenzi de pe server-ul succesor
        request *req = calloc(1, sizeof(request));
        req->type = ADD_SERVER;
        server_handle_request(main->servers[succ], req);
        free(req);

        hashtable_t *ht = main->servers[succ]->database;
        unsigned int hash_doc;
        for (unsigned int i = 0; i < ht->hmax; i++) {
            Node *node = ht->buckets[i]->head, *next;
            while (node) {
                next = node->next;
                hash_doc = main->hash_function_docs(
                    ((info *)(node->data))->key);
                if (succ == 0 && hash_doc < main->servers[0]->hash) {
                    node = next;
                } else {
                    if (hash_doc < hash ||
                    (index == 0 && hash_doc > main->servers[succ]->hash)) {
                        // adaugare in noul database
                        ht_put(main->servers[index]->database,
                        ((info *)(node->data))->key, MAX_RESPONSE_LENGTH,
                        ((info *)(node->data))->value, MAX_RESPONSE_LENGTH);
                        // daca exista in cache, il vom elimina
                        if (ht_get(main->servers[succ]->cache->name_node_map, ((info *)(node->data))->key))
                            lru_cache_remove(main->servers[succ]->cache, ((info *)(node->data))->key);
                        // stergere din database vechi
                        ht_remove_entry(ht, ((info *)(node->data))->key);
                    }
                    node = next;
                }
            }
        }
    }
}

void loader_remove_server(load_balancer* main, int server_id) {
    unsigned int index = 0, i;
    for (i = 0; i < main->size; i++)
        if (main->servers[i]->server_id == server_id) {
            index = i;
            break;
        }
    
    // executie comenzi de pe server-ul ce se vrea sters
    request *req = calloc(1, sizeof(request));
    req->type = REMOVE_SERVER;
    server_handle_request(main->servers[index], req);
    free(req);

    // mutare documente pe server-ul succesor de pe server-ul curent
    unsigned int succ;
    succ = (index + 1) % main->size;
    hashtable_t *ht = main->servers[index]->database;
    for (unsigned int i = 0; i < ht->hmax; i++) {
        Node *node = ht->buckets[i]->head;
        while (node) {
            // adaugare in noul database
            ht_put(main->servers[succ]->database,((info *)(node->data))->key, MAX_RESPONSE_LENGTH,
            ((info *)(node->data))->value, MAX_RESPONSE_LENGTH);
            node = node->next;
        }
    }
    free_server(&(main->servers[index]));
    main->servers[index] = NULL;
    for (i = index; i < main->size - 1; i++)
        main->servers[i] = main->servers[i + 1];
    main->servers[main->size - 1] = NULL;
    main->size--;
}

response *loader_forward_request(load_balancer* main, request *req) {
    unsigned int i, index = 0, hash;
    hash = main->hash_function_docs(req->doc_name);
    // cautare server cu hash-ul minim, mai mare decat hash-ul documentului
    for (i = 0; i < main->size; i++)
        if (hash < main->servers[i]->hash) {
            index = i;
            break;
        }
    
    return server_handle_request(main->servers[index], req);
}

void free_load_balancer(load_balancer** main) {
    for (unsigned int i = 0; i < MAX_SERVERS; i++) {
        if ((*main)->servers[i])
            free_server(&((*main)->servers[i]));
    }
    free((*main)->servers);
    free((*main)->hashring);
    free(*main);

    *main = NULL;
}


