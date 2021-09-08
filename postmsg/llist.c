/*
 *  llist.c
 *
 *  Simple doubly linked-list management functions.
 *
 *  Adapted from 1995 public domain C code by Scott Pitcher.
 *  Modified 1995-2002 by Andrew Clarke and released to the public domain.
 */

#include <stdlib.h>
#include "llist.h"

list_t *list_init(void)
{
    list_t *list;

    list = malloc(sizeof *list);

    if (list != NULL)
    {
        list->first = NULL;
        list->last = NULL;
        list->items = 0;
    }

    return list;
}

int list_term(list_t *list)
{
    if (list == NULL)
    {
        return 0;
    }

    if (list->items != 0)
    {
        node_t *node;

        node = list->first;

        while (node != NULL)
        {
            list->first = node->next;
            free(node);
            node = list->first;
        }
    }

    free(list);

    return 1;
}

void *list_get_item(node_t *node)
{
    if (node == NULL)
    {
        return NULL;
    }

    return node->item;
}

int list_set_item(node_t *node, void *item)
{
    if (node == NULL)
    {
        return 0;
    }

    node->item = item;

    return 1;
}

node_t *list_node_init(void *item)
{
    node_t *node;

    node = malloc(sizeof *node);

    if (node != NULL)
    {
        node->next = NULL;
        node->prev = NULL;
        node->item = item;
    }

    return node;
}

int list_node_term(node_t *node)
{
    if (node == NULL)
    {
        return 0;
    }

    free(node);

    return 1;
}

int list_add_node(list_t *list, node_t *node)
{
    if (list == NULL)
    {
        return 0;
    }

    node->prev = list->last;

    if (node->prev != NULL)
    {
        list->last->next = node;
    }

    node->next = NULL;

    if (list->first == NULL)
    {
        list->first = node;
    }

    list->last = node;
    list->items++;

    return 1;
}

int list_add_item(list_t *list, void *item)
{
    node_t *node;

    node = list_node_init(item);

    if (node == NULL)
    {
        return 0;
    }

    return list_add_node(list, node);
}

int list_delete_node(list_t *list, node_t *node)
{
    node_t *old_next;

    if (list == NULL)
    {
        return 0;
    }

    old_next = node->next;

    if (node == list->first)
    {
        list->first = node->next;
    }

    if (node == list->last)
    {
        list->last = node->prev;
    }

    if (node->next != NULL)
    {
        node->next->prev = node->prev;
        node->next = NULL;
    }

    if (node->prev != NULL)
    {
        node->prev->next = old_next;
        node->prev = NULL;
    }

    list->items--;

    return 1;
}

node_t *list_node_first(list_t *list)
{
    if (list == NULL)
    {
        return NULL;
    }

    return list->first;
}

node_t *list_node_last(list_t *list)
{
    if (list == NULL)
    {
        return NULL;
    }

    return list->last;
}

node_t *list_node_prev(node_t *node)
{
    if (node == NULL)
    {
        return NULL;
    }

    return node->prev;
}

node_t *list_node_next(node_t *node)
{
    if (node == NULL)
    {
        return NULL;
    }

    return node->next;
}

int list_compare_nodes(node_t *node_1, node_t *node_2, int (*fcmp) (const void *, const void *))
{
    return fcmp(node_1->item, node_2->item);
}

int list_swap_nodes(node_t *node_1, node_t *node_2)
{
    node_t *temp;

    if (node_1 == NULL || node_2 == NULL)
    {
        return 0;
    }

    temp = node_1->next;
    node_1->next = node_2->next;
    node_2->next = temp;
    temp = node_1->prev;
    node_1->prev = node_2->prev;
    node_2->prev = temp;

    return 1;
}

node_t *list_search(list_t *list, void *item, int (*fcmp) (const void *, const void *))
{
    node_t *node;

    if (list == NULL)
    {
        return NULL;
    }

    if (list->items == 0)
    {
        return NULL;
    }

    node = list->first;

    while (node != NULL)
    {
        if (fcmp(node->item, item) == 0)
        {
            return node;
        }
        else
        {
            node = node->next;
        }
    }

    return NULL;
}

int list_total_items(list_t *list)
{
    if (list == NULL)
    {
        return 0;
    }

    return list->items;
}

int list_is_empty(list_t *list)
{
    return list->items == 0;
}

#ifdef TEST_LLIST

#include <stdio.h>

int main(void)
{
    list_t *list;
    node_t *node;

    list = list_init();

    list_add_item(list, "One banana");
    list_add_item(list, "Two banana");
    list_add_item(list, "Three banana");

    node = list_node_first(list);

    while (node != NULL)
    {
        char *str;

        str = list_get_item(node);

        if (str != NULL)
        {
            puts(str);
        }

        node = list_node_next(node);
    }

    list_term(list);

    return 0;
}

#endif
