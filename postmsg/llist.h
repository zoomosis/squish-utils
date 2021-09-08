#ifndef __LLIST_H__
#define __LLIST_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct node_t
{
    struct node_t *prev;
    struct node_t *next;
    void *item;
}
node_t;

typedef struct
{
    struct node_t *first;
    struct node_t *last;
    int items;
}
list_t;

list_t *list_init(void);
int list_term(list_t *list);
void *list_get_item(node_t *node);
int list_set_item(node_t *node, void *item);
node_t *list_node_init(void *item);
int list_node_term(node_t *node);
int list_add_node(list_t *list, node_t *node);
int list_add_item(list_t *list, void *item);
int list_delete_node(list_t *list, node_t *node);
node_t *list_node_first(list_t *list);
node_t *list_node_last(list_t *list);
node_t *list_node_prev(node_t *node);
node_t *list_node_next(node_t *node);
int list_compare_nodes(node_t *node_1, node_t *node_2, int (*fcmp) (const void *, const void *));
int list_swap_nodes(node_t *node_1, node_t *node_2);
node_t *list_search(list_t *list, void *item, int (*fcmp) (const void *, const void *));
int list_total_items(list_t *list);
int list_is_empty(list_t *list);

#ifdef __cplusplus
};
#endif

#endif
