/*
 * Copyright (c) 2024, Maxim Anca-Stefania <ancastef7@gmail.com>
 */

#include "load_balancer.h"
#include "server.h"

load_balancer *init_load_balancer(bool enable_vnodes) {
	load_balancer *main = calloc(1, sizeof(load_balancer));
    DIE(!main, "malloc() failed");
    main->servers = calloc(3 * MAX_SERVERS, sizeof(server_t *));
    main->hashring = calloc(3 * MAX_SERVERS, sizeof(int));
    main->size = 0;
    main->hash_function_docs = hash_string;
    main->hash_function_servers = hash_uint;
    main->enable_vnodes = enable_vnodes;
    return main;
}

void loader_add_single_server(load_balancer* main, int server_id,
                            int cache_size) {
    unsigned int index = main->size;
    unsigned int hash = main->hash_function_servers(&server_id);
    // cautare server cu hash-ul minim, mai mare decat hash-ul nou introdus
    for (unsigned int i = 0; i < main->size; i++) {
        if (hash < main->hashring[i]) {
            index = i;
            break;
        } else if (hash == main->hashring[i]) {
            if (server_id < main->servers[i]->server_id)
                index = i;
            else
                index = i + 1;
            break;
        }
    }
    // mutare server nou pe pozitia index
    if (main->size > 0 && index != main->size) {
        for (int i = main->size; i >= (int) (index + 1); i--) {
            main->servers[i] = main->servers[i - 1];
            main->hashring[i] = main->hashring[i - 1];
        }
    }
    main->size++;
    if (server_id < 100000) {
        main->servers[index] = init_server(cache_size);
        main->servers[index]->server_id = server_id;
        main->servers[index]->hash = hash;
        main->hashring[index] = hash;
    } else {
        unsigned int src_idx;
        main->servers[index] = calloc(1, sizeof(server_t));
        main->servers[index]->server_id = server_id;
        main->servers[index]->hash = hash;
        main->hashring[index] = hash;
        for (unsigned int i = 0; i < main->size; i++)
            if (main->servers[i]->server_id == server_id % 100000) {
                src_idx = i;
                break;
            }
        main->servers[index]->cache = main->servers[src_idx]->cache;
        main->servers[index]->database = main->servers[src_idx]->database;
        main->servers[index]->task = main->servers[src_idx]->task;
    }

    // mutare documente pe server-ul nou introdus
    // parcurgem documentele de pe server-ul succesor, si daca
    // hash_document < hash_new_server, il mutam

    if (main->size > 1) {
        unsigned int succ;
        succ = (index + 1) % main->size;
        if (main->servers[succ]->server_id % 100000 == server_id % 100000)
            return;
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
                if ((index > 0 && hash_doc <= main->hashring[index] &&
					hash_doc > main->hashring[index - 1]) ||
				    (index == 0 &&
					(hash_doc <= main->hashring[index] ||
					hash_doc > main->hashring[main->size - 1]))) {
                        // adaugare in noul database
                        ht_put(main->servers[index]->database,
                        ((info *)(node->data))->key, MAX_RESPONSE_LENGTH,
                        ((info *)(node->data))->value, MAX_RESPONSE_LENGTH);
                        // daca exista in cache, il vom elimina
                        lru_cache_remove(main->servers[succ]->cache,
                                        ((info *)(node->data))->key);
                        // stergere din database vechi
                        ht_remove_entry(ht, ((info *)(node->data))->key);
                }
                node = next;
            }
        }
    }
}

void loader_add_server(load_balancer* main, int server_id, int cache_size) {
    if (main->enable_vnodes == 1) {
        for (int i = 0; i < 3; i++) {
            unsigned int id = i * 100000 + server_id;
            loader_add_single_server(main, id, cache_size);
        }
    } else {
        loader_add_single_server(main, server_id, cache_size);
    }
}

void loader_remove_single_server(load_balancer* main, int server_id) {
    unsigned int index = 0, i;
    for (i = 0; i < main->size; i++)
        if (main->servers[i]->server_id == server_id) {
            index = i;
            break;
        }
    if (server_id < 100000) {
        // executie comenzi de pe server-ul ce se vrea sters
        request *req = calloc(1, sizeof(request));
        req->type = REMOVE_SERVER;
        server_handle_request(main->servers[index], req);
        free(req);
    }
    server_t *deleted_server = main->servers[index];
    hashtable_t *ht = main->servers[index]->database;

    for (i = index; i < main->size - 1; i++) {
        main->servers[i] = main->servers[i + 1];
        main->hashring[i] = main->hashring[i + 1];
    }
    main->servers[main->size - 1] = NULL;

    // mutare documente pe server-ul succesor de pe server-ul curent
    main->size--;
    if (index == main->size && main->size)
		index = 0;
    for (unsigned int i = 0; i < ht->hmax; i++) {
        Node *node = ht->buckets[i]->head;
        while (node) {
            unsigned int hash_doc = main->hash_function_docs(
                    ((info *)(node->data))->key);
            if ((index > 0 && hash_doc <= main->hashring[index] &&
            hash_doc > main->hashring[index - 1]) ||
            (index == 0 && (hash_doc <= main->hashring[index] ||
            hash_doc > main->hashring[main->size - 1]))) {
                // adaugare in noul database
                ht_put(main->servers[index]->database,
                ((info *)(node->data))->key,
                MAX_RESPONSE_LENGTH, ((info *)(node->data))->value,
                MAX_RESPONSE_LENGTH);
            }
            node = node->next;
        }
    }
    if (server_id >= 100000)
        free_server(&deleted_server);
}

void loader_remove_server(load_balancer* main, int server_id) {
    unsigned int index = 0, i;
    for (i = 0; i < main->size; i++)
        if (main->servers[i]->server_id == server_id) {
            index = i;
            break;
        }
    server_t *del_server = main->servers[index];
    if (main->enable_vnodes == 1) {
        for (i = 0; i <= 2; i++) {
            int id = i * 100000 + server_id;
            loader_remove_single_server(main, id);
        }
    } else {
        loader_remove_single_server(main, server_id);
    }
    free_server(&del_server);
}

response *loader_forward_request(load_balancer* main, request *req) {
    unsigned int i, index = 0, hash;
    hash = main->hash_function_docs(req->doc_name);
    // cautare server cu hash-ul minim, mai mare decat hash-ul documentului
    for (i = 0; i < main->size; i++)
        if (hash < main->hashring[i]) {
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


