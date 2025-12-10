#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "upd_list.h"

void
enhanced_list_init(struct enhanced_list *l)
{
    l->prev = l;
    l->next = l;
    l->rcount = 2;
}

// Increment reference count for a list element
void
enhanced_list_acquire(struct enhanced_list *l)
{
    l->rcount++;
}

// Decrement reference count and potentially free memory
// Recursive for cases where removing one node leads to removing neighbors
void
enhanced_list_release(struct enhanced_list *l, struct enhanced_list *head)
{   
    if(l == head)
        return;

    l->rcount--;

    // If no references remain, recursively free neighbors and free memory
    if(l->rcount == 0){
        enhanced_list_release(l->next, head);
        enhanced_list_release(l->prev, head);
        bd_free(l);
    }
}

// Add element to the list
void
enhanced_list_push(struct enhanced_list *l, struct enhanced_list *head)
{
    l->rcount = 2;

    l->next = head->next;
    l->prev = head;
    head->next->prev = l;
    head->next = l;
}

// Remove element from list
// Not physical memory deallocation since references may steel exist
// Memory will only be freed when reference count reaches zero
void
enhanced_list_remove(struct enhanced_list *l, struct enhanced_list *head)
{
    // Remove element from list
    l->prev->next = l->next;
    l->next->prev = l->prev;

    // Increment neighbor reference counts to prevent premature freeing
    if(l->next != head)
        enhanced_list_acquire(l->next);
    if(l->prev != head)
        enhanced_list_acquire(l->prev);

    // Decrease reference count due to removal
    l->rcount -= 2;

    // Free memory if no references remain
    if(l->rcount == 0)
        bd_free(l);
}

// Move to next element in list
// Increments reference count for the next element
struct enhanced_list*
enhanced_list_next(struct enhanced_list *l, struct enhanced_list *head)
{
    if(l->next != head)
        enhanced_list_acquire(l->next);
    return l->next;
}

// Begin iterating through the list
struct enhanced_list*
enhanced_list_begin(struct enhanced_list *l)
{
    return enhanced_list_next(l, l);
}

// Iterate to next element in list
void
enhanced_list_iterate(struct enhanced_list **l, struct enhanced_list *head)
{
    struct enhanced_list *next = enhanced_list_next(*l, head);

    enhanced_list_release(*l, head);

    *l = next;
}
