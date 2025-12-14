/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[128];
  word_t old_val;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp(char *str) {
  if (free_ == NULL) {
    printf("Error: No more watchpoints available. \n");
    assert(0);
  }

  WP *t = free_;
  free_ = free_->next;
  t->next = head;
  head = t;

  strcpy(t->expr, str);

  bool success;
  t->old_val = expr(str, &success);

  if (!success) {
    printf("Error: Invalid expression when creating watchpoint. \n");
    // 实际上这里最好能回滚 free_wp 的操作，或者 assert(0)
  }

  return t;
}

void free_wp(WP *wp) {
  if (head == NULL || wp == NULL) return;

  if (wp == head) {
    head = head->next;
  } else {
    WP *prev = head;
    while (prev->next != NULL && prev->next != wp) {
      prev = prev->next;
    }

    if (prev->next == NULL) {
      printf("Error: Watchpoint not found in active list. \n");
      return;
    }

    prev->next = wp->next;
  }

  wp->next = free_;
  free_ = wp;

  wp->expr[0] = '\0';
  wp->old_val = 0;
  return ;
}

bool scan_watchpoints() {
  WP *t = head;
  
  while (t != NULL) {
    bool success;
    word_t new_val = expr(t->expr, &success);

    if (new_val != t->old_val) {
        printf("Watchpoint %d triggered!\n", t->NO);
        printf("Expr: %s\n", t->expr);
        printf("Old value: %u (0x%x)\n", t->old_val, t->old_val); // 假设 word_t 是 32位
        printf("New value: %u (0x%x)\n", new_val, new_val);

        // 更新旧值
        t->old_val = new_val;
        return true;
    }
    t = t->next;
  }
  return false;

}
