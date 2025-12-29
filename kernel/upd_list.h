#pragma once

struct enhanced_list {
  struct enhanced_list *prev;
  struct enhanced_list *next;
  int rcount;
};

void enhanced_list_init(struct enhanced_list *l);
void enhanced_list_acquire(struct enhanced_list *l);
void enhanced_list_release(struct enhanced_list *l, struct enhanced_list *head);
void enhanced_list_push(struct enhanced_list *l, struct enhanced_list *head);
void enhanced_list_remove(struct enhanced_list *l, struct enhanced_list *head);
struct enhanced_list* enhanced_list_next(struct enhanced_list *l, struct enhanced_list *head);
struct enhanced_list* enhanced_list_begin(struct enhanced_list *l);
void enhanced_list_iterate(struct enhanced_list **l, struct enhanced_list *head);
