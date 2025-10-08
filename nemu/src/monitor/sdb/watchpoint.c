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
#include <string.h>
#include <assert.h>
#include <stdio.h>


#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[128];     // 记录监视的表达式
  uint32_t old_val;   // 表达式上一次的值

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
static WP* new_wp() {
  if (free_ == NULL) {
    assert(0 && "No free watchpoints available!");
  }
  WP *wp = free_;
  free_ = free_->next;

  wp->next = head;
  head = wp;

  return wp;
}

/* 修改3：实现 free_wp，把 wp 放回 free_ 链表 */
static void free_wp(WP *wp) {
  // 从 head 链表里删除
  if (head == wp) {
    head = head->next;
  } else {
    WP *prev = head;
    while (prev && prev->next != wp) {
      prev = prev->next;
    }
    if (prev) prev->next = wp->next;
  }

  // 插入到 free_ 链表头
  wp->next = free_;
  free_ = wp;
}

/* 修改4：添加设置监视点接口 */
void wp_set(char *e) {
  WP *wp = new_wp();
  strcpy(wp->expr, e);

  bool success = true;
  uint32_t val = expr(e, &success);
  assert(success);

  wp->old_val = val;
  printf("Set watchpoint %d: %s = %u\n", wp->NO, wp->expr, val);
}

/* 修改5：删除监视点接口 */
void wp_delete(int no) {
    WP *prev = NULL, *wp = head;
    while (wp) {
        if (wp->NO == no) {
            if (prev) prev->next = wp->next;
            else head = wp->next;
            free_wp(wp);
            printf("Watchpoint %d deleted\n", no);
            return;
        }
        prev = wp;
        wp = wp->next;
    }
    printf("No watchpoint with number %d\n", no);
}


/* 修改6：打印监视点 */
void wp_info() {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }
  WP *wp = head;
  while (wp) {
    printf("Watchpoint %d: %s (last value: %u)\n", wp->NO, wp->expr, wp->old_val);
    wp = wp->next;
  }
}

/* 修改7：在每条指令后检查监视点 */
bool check_watchpoints() {
  WP *wp = head;
  bool stop = false;
  while (wp) {
    bool success = true;
    uint32_t new_val = expr(wp->expr, &success);
    assert(success);

    if (new_val != wp->old_val) {
      printf("Watchpoint %d triggered: %s\n", wp->NO, wp->expr);
      printf("Watchpoint %d: %s\n", wp->NO, wp->expr);
      printf("Old value = 0x%08x (%u)\n", wp->old_val, wp->old_val);
      printf("New value = 0x%08x (%u)\n", new_val, new_val);
      wp->old_val = new_val;
      stop = true;
    }

    wp = wp->next;
  }
  return stop;
}
