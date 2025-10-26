#include "linked_list.h"
#include "memory_manager.h"
#include "common_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t head_mutex = PTHREAD_MUTEX_INITIALIZER;

// Memory pool setup.vMakes sure mem_init() runs before using mem_alloc().

static int memory_initialized = 0;
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

static void ensure_memory_initialized(void) {
    pthread_mutex_lock(&init_lock);
    if (!memory_initialized) {
        mem_init(128 * 1024 * 1024); // 128 MB pool
        memory_initialized = 1;
    }
    pthread_mutex_unlock(&init_lock);
}

//list_init: initialize or reset the list

void list_init(Node **head, size_t size)
{
    (void)size;
    ensure_memory_initialized();
    if (!head) return;

    pthread_mutex_lock(&head_mutex);
    if (*head != NULL) {
        pthread_mutex_unlock(&head_mutex);
        list_cleanup(head);
    } else {
        *head = NULL;
        pthread_mutex_unlock(&head_mutex);
    }
}

// list_insert: add a new node at the end
void list_insert(Node **head, uint16_t data)
{
    ensure_memory_initialized();
    if (!head) return;

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf_red("ERROR: Allocation failed.\n");
        return;
    }

    new_node->data = data;
    new_node->next = NULL;
    pthread_mutex_init(&new_node->lock, NULL);

    pthread_mutex_lock(&head_mutex);

    if (*head == NULL) {
        *head = new_node;
        pthread_mutex_unlock(&head_mutex);
        return;
    }

    Node *curr = *head;
    while (curr->next)
        curr = curr->next;

    curr->next = new_node;
    pthread_mutex_unlock(&head_mutex);
}


// list_insert_after: insert node after given one

void list_insert_after(Node *prev_node, uint16_t data)
{
    ensure_memory_initialized();
    if (!prev_node) return;

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf_red("ERROR: Allocation failed.\n");
        return;
    }

    new_node->data = data;
    new_node->next = NULL;
    pthread_mutex_init(&new_node->lock, NULL);

    pthread_mutex_lock(&prev_node->lock);
    new_node->next = prev_node->next;
    prev_node->next = new_node;
    pthread_mutex_unlock(&prev_node->lock);
}


// list_insert_before: insert node before another

void list_insert_before(Node **head, Node *next_node, uint16_t data)
{
    ensure_memory_initialized();
    if (!head || !next_node) return;

    pthread_mutex_lock(&head_mutex);

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf_red("ERROR: Allocation failed.\n");
        pthread_mutex_unlock(&head_mutex);
        return;
    }

    new_node->data = data;
    new_node->next = NULL;
    pthread_mutex_init(&new_node->lock, NULL);

    if (*head == next_node) {
        new_node->next = *head;
        *head = new_node;
        pthread_mutex_unlock(&head_mutex);
        return;
    }

    Node *curr = *head;
    Node *prev = NULL;

    while (curr && curr != next_node) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == next_node && prev)
        prev->next = new_node, new_node->next = curr;
    else {
        pthread_mutex_destroy(&new_node->lock);
        mem_free(new_node);
    }

    pthread_mutex_unlock(&head_mutex);
}


// list_delete: remove first node with given value

void list_delete(Node **head, uint16_t data)
{
    ensure_memory_initialized();
    if (!head) return;

    pthread_mutex_lock(&head_mutex);

    if (*head == NULL) {
        pthread_mutex_unlock(&head_mutex);
        return;
    }

    Node *curr = *head;
    Node *prev = NULL;

    while (curr && curr->data != data) {
        prev = curr;
        curr = curr->next;
    }

    if (!curr) {
        pthread_mutex_unlock(&head_mutex);
        return;
    }

    if (prev == NULL)
        *head = curr->next;
    else
        prev->next = curr->next;

    pthread_mutex_destroy(&curr->lock);
    mem_free(curr);

    pthread_mutex_unlock(&head_mutex);
}

// list_search: find first node with given value

Node *list_search(Node **head, uint16_t data)
{
    if (!head) return NULL;

    pthread_mutex_lock(&head_mutex);

    Node *curr = *head;
    while (curr) {
        if (curr->data == data) {
            Node *res = curr;
            pthread_mutex_unlock(&head_mutex);
            return res;
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&head_mutex);
    return NULL;
}


// list_display_range: print nodes between two

void list_display_range(Node **head, Node *start_node, Node *end_node)
{
    pthread_mutex_lock(&head_mutex);

    Node *curr = (head ? *head : NULL);
    int started = (start_node == NULL);
    int first = 1;

    printf("[");

    while (curr) {
        if (!started && curr == start_node)
            started = 1;

        if (started) {
            if (!first) printf(", ");
            printf("%u", curr->data);
            first = 0;
        }

        if (end_node && curr == end_node)
            break;

        curr = curr->next;
    }

    printf("]\n");

    pthread_mutex_unlock(&head_mutex);
}

// list_display: print full list
void list_display(Node **head)
{
    list_display_range(head, NULL, NULL);
}

// list_count_nodes: count nodes safely
int list_count_nodes(Node **head)
{
    if (!head) return 0;

    pthread_mutex_lock(&head_mutex);

    Node *curr = *head;
    int count = 0;
    int safety = 0;

    while (curr) {
        count++;
        curr = curr->next;

        if (++safety > 1000000) {
            fprintf(stderr, "ERROR: Possible cycle detected in list_count_nodes()\n");
            break;
        }
    }

    pthread_mutex_unlock(&head_mutex);
    return count;
}

// list_cleanup: free all nodes and reset memory
void list_cleanup(Node **head)
{
    if (!head) return;

    pthread_mutex_lock(&head_mutex);
    Node *curr = *head;
    *head = NULL;
    pthread_mutex_unlock(&head_mutex);

    while (curr) {
        Node *next = curr->next;
        pthread_mutex_destroy(&curr->lock);
        mem_free(curr);
        curr = next;
    }

    pthread_mutex_lock(&init_lock);
    if (memory_initialized) {
        mem_deinit();
        memory_initialized = 0;
    }
    pthread_mutex_unlock(&init_lock);
}
