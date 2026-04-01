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

  // 1. 先验证表达式的合法性
  bool success = true;
  word_t val = expr(str, &success);

  if (!success) {
    printf("Error: Invalid expression when creating watchpoint. \n");
    // 🚨 完美回滚：把节点还给 free_ 链表，严禁挂入 head
    t->next = free_;
    free_ = t;
    return NULL;
  }

  // 2. 验证通过后，再执行字符串拷贝和挂载
  strncpy(t->expr, str, sizeof(t->expr) - 1);
  t->expr[sizeof(t->expr) - 1] = '\0'; // 防止缓冲区溢出
  t->old_val = val;

  t->next = head;
  head = t;

  printf("Watchpoint %d: %s\n", t->NO, t->expr);
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

bool scan_wp() {
  WP *t = head;
  bool changed = false; // 记录是否有任何一个断点被触发
  
  while (t != NULL) {
    bool success;
    word_t new_val = expr(t->expr, &success);

    // 增加 success 判断，防止运行期表达式失效导致误触发
    if (success && new_val != t->old_val) {
        printf("Watchpoint %d triggered!\n", t->NO);
        printf("Expr: %s\n", t->expr);
        // 使用 %08x 对齐十六进制输出，更符合黑客审美
        printf("Old value: %u (0x%08x)\n", t->old_val, t->old_val); 
        printf("New value: %u (0x%08x)\n", new_val, new_val);

        t->old_val = new_val;
        changed = true; // 标记已触发，但不立刻 return，继续扫描剩下的
    }
    t = t->next;
  }
  
  return changed;
}

void info_wp() {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("%-8s %-16s %s\n", "NO", "Expr", "Value");

  WP *p = head;
  while (p != NULL) {
    printf("%-8d %-16s %u (0x%x)\n", p->NO, p->expr, p->old_val, p->old_val);
    p = p->next;
  }
}

bool delete_wp(int no) {
  WP *p = head;
  while (p != NULL) {
    if (p->NO == no) {
      free_wp(p);
      return true;
    }
    p = p->next;
  }
  return false;
}