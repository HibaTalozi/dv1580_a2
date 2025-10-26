#include "linked_list.h"
#include "memory_manager.h"
#include "common_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Thread-safe linked list using custom memory manager and fine-grained locks.

static pthread_mutex_t head_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize the list (clears existing content)
void list_init(Node **head, size_t size)
{
    (void)size;
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

//Insert new node at the end
void list_insert(Node **head, uint16_t data)
{
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
    pthread_mutex_lock(&curr->lock);
    pthread_mutex_unlock(&head_mutex);
    while (curr->next) {
        Node *next = curr->next;
        pthread_mutex_lock(&next->lock);
        pthread_mutex_unlock(&curr->lock);
        curr = next;
    }
    curr->next = new_node;
    pthread_mutex_unlock(&curr->lock);
}

//Insert new node after a given one
void list_insert_after(Node *prev_node, uint16_t data)
{
    if (!prev_node) return;
    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf_red("ERROR: Allocation failed.\n");
        return;
    }
    new_node->data = data;
    pthread_mutex_init(&new_node->lock, NULL);
    pthread_mutex_lock(&prev_node->lock);
    new_node->next = prev_node->next;
    prev_node->next = new_node;
    pthread_mutex_unlock(&prev_node->lock);
}

//Insert a new node before a specific node
void list_insert_before(Node **head, Node *next_node, uint16_t data)
{
    if (!head || !next_node) return;
    Node *new_node = (Node *)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf_red("ERROR: Allocation failed.\n");
        return;
    }
    new_node->data = data;
    pthread_mutex_init(&new_node->lock, NULL);
    pthread_mutex_lock(&head_mutex);
    if (*head == next_node) {
        new_node->next = *head;
        *head = new_node;
        pthread_mutex_unlock(&head_mutex);
        return;
    }
    Node *curr = *head;
    pthread_mutex_lock(&curr->lock);
    pthread_mutex_unlock(&head_mutex);
    Node *prev = NULL;
    while (curr && curr != next_node) {
        Node *next = curr->next;
        if (next) pthread_mutex_lock(&next->lock);
        if (prev) pthread_mutex_unlock(&prev->lock);
        prev = curr;
        curr = next;
    }
    if (curr == next_node && prev) {
        pthread_mutex_lock(&prev->lock);
        new_node->next = curr;
        prev->next = new_node;
        pthread_mutex_unlock(&prev->lock);
    } else {
        mem_free(new_node);
    }
    if (curr) pthread_mutex_unlock(&curr->lock);
}

//Delete first node with matching data
void list_delete(Node **head, uint16_t data)
{
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
    mem_free(curr);
    pthread_mutex_unlock(&head_mutex);
}

//Search for a node by value (returns locked node)
Node *list_search(Node **head, uint16_t data)
{
    if (!head || !*head) return NULL;
    pthread_mutex_lock(&head_mutex);
    Node *curr = *head;
    if (!curr) {
        pthread_mutex_unlock(&head_mutex);
        return NULL;
    }

    pthread_mutex_lock(&curr->lock);
    pthread_mutex_unlock(&head_mutex);

    while (curr) {
        if (curr->data == data)
            return curr; // Caller must unlock later
        Node *next = curr->next;
        if (next) pthread_mutex_lock(&next->lock);
        pthread_mutex_unlock(&curr->lock);
        curr = next;
    }
    return NULL;
}

//Print list elements in range
void list_display_range(Node **head, Node *start_node, Node *end_node)
{
    if (!head || !*head) {
        printf("[]\n");
        return;
    }
    pthread_mutex_lock(&head_mutex);
    Node *curr = *head;
    pthread_mutex_unlock(&head_mutex);
    int started = (start_node == NULL);
    int first = 1;
    printf("[");

    while (curr) {
        if (!started && curr == start_node)
            started = 1;
        if (started) {
            if (!first) printf(", ");
            pthread_mutex_lock(&curr->lock);
            printf("%u", curr->data);
            pthread_mutex_unlock(&curr->lock);
            first = 0;
        }
        if (end_node && curr == end_node)
            break;
        curr = curr->next;
    }
    printf("]\n");
}

//Print the entire list
void list_display(Node **head)
{
    list_display_range(head, NULL, NULL);
}

//Count nodes safely
int list_count_nodes(Node **head)
{
    if (!head || !*head) return 0;

    pthread_mutex_lock(&head_mutex);
    Node *curr = *head;
    if (!curr) {
        pthread_mutex_unlock(&head_mutex);
        return 0;
    }

    pthread_mutex_lock(&curr->lock);
    pthread_mutex_unlock(&head_mutex);

    int count = 0;
    while (curr) {
        count++;
        Node *next = curr->next;
        if (next) pthread_mutex_lock(&next->lock);
        pthread_mutex_unlock(&curr->lock);
        curr = next;
    }
    return count;
}

//Free all nodes
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
}

