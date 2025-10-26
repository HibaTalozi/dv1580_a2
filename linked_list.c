#include "linked_list.h"
#include "memory_manager.h"
#include "common_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Global mutex that protects the list head
static pthread_mutex_t head_mutex = PTHREAD_MUTEX_INITIALIZER;

// Memory pool setup. Ensures mem_init() is called once before any allocations.
static int memory_initialized = 0;
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * ensure_memory_initialized: make sure memory manager is ready
 * Initializes a large memory pool once. Thread-safe using a lock.
 */
static void ensure_memory_initialized(void) {
    pthread_mutex_lock(&init_lock);
    if (!memory_initialized) {
        mem_init(128 * 1024 * 1024); // 128 MB pool
        memory_initialized = 1;
    }
    pthread_mutex_unlock(&init_lock);
}

/*
 * list_init: initialize or reset a linked list
 * @head: pointer to the list head
 * @size: optional size (unused, but required for interface)
 * This function clears an existing list or initializes a new one.
 * If the list already exists, it calls list_cleanup() to free all nodes.
 */
void list_init(Node **head, size_t size)
{
    (void)size;
    ensure_memory_initialized();

    if (!head) {
        fprintf(stderr, "ERROR: list_init() called with NULL head\n");
        return;
    }

    pthread_mutex_lock(&head_mutex);
    if (*head != NULL) {
        pthread_mutex_unlock(&head_mutex);
        list_cleanup(head);
    } else {
        *head = NULL;
        pthread_mutex_unlock(&head_mutex);
    }
}

/*
 * list_insert: add a new node at the end
 * @head: pointer to the list head
 * @data: data value to store in the new node
 * Allocates a new node and appends it at the end of the list.
 * Handles memory allocation errors gracefully.
 */
void list_insert(Node **head, uint16_t data)
{
    ensure_memory_initialized();

    if (!head) {
        fprintf(stderr, "ERROR: list_insert() called with NULL head\n");
        return;
    }

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "ERROR: mem_alloc() failed in list_insert()\n");
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

/*
 * list_insert_after: insert a new node after a given one
 * @prev_node: node after which the new node should be inserted
 * @data: value for the new node
 * This function creates a new node and places it directly after
 * the specified node. Thread-safe at the node level.
 */
void list_insert_after(Node *prev_node, uint16_t data)
{
    ensure_memory_initialized();
    if (!prev_node) {
        fprintf(stderr, "ERROR: list_insert_after() called with NULL prev_node\n");
        return;
    }

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "ERROR: mem_alloc() failed in list_insert_after()\n");
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

/*
 * list_insert_before: insert a node before another
 * @head: pointer to the list head
 * @next_node: node before which to insert
 * @data: data value for the new node
 * Inserts a new node just before a given node.
 * If next_node is the head, the new node becomes the new head.
 */
void list_insert_before(Node **head, Node *next_node, uint16_t data)
{
    ensure_memory_initialized();
    if (!head || !next_node) return;

    pthread_mutex_lock(&head_mutex);

    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "ERROR: mem_alloc() failed in list_insert_before()\n");
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

/*
 * list_delete: delete the first node with matching value
 * @head: pointer to the list head
 * @data: value to search and remove
 * Removes the first node that matches 'data'.
 * Frees memory safely and handles edge cases (empty list, not found).
 */
void list_delete(Node **head, uint16_t data)
{
    ensure_memory_initialized();
    if (!head) {
        fprintf(stderr, "ERROR: list_delete() called with NULL head\n");
        return;
    }

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

/*
 * list_search: find a node by value
 * @head: pointer to the list head
 * @data: value to look for
 * Searches for the first node with the specified value.
 * Returns a pointer to the node or NULL if not found.
 */
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

/*
 * print nodes from start_node to end_node
 * @head: pointer to the list head
 * @start_node: start point (NULL for beginning)
 * @end_node: end point (NULL for full list)
 * Prints the list contents in order.
 */
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


//print the entire list
void list_display(Node **head)
{
    list_display_range(head, NULL, NULL);
}

/*
 * list_count_nodes: count how many nodes are in the list
 * @head: pointer to the list head
 * Traverses the list and counts nodes safely.
 * Includes a safety limit to detect cycles or corruption.
 */
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

/*
 * list_cleanup: free all nodes and reset memory manager
 * @head: pointer to the list head
 * Frees all nodes and destroys their locks.
 * Finally resets the memory pool safely.
 */
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
