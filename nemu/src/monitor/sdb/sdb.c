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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/vaddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  // 释放旧内存，防止内存泄漏
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  // 处理历史记录
  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;

  if (args != NULL) {
    if (sscanf(args, "%lu", &n) <= 1) {
      printf("Invalid step number: %s\n", args);
      return 0;
    }
  }

  cpu_exec(n);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info r/w\n");
    return 0;
  }

  if (strcmp(args, "r") == 0) {
    isa_reg_display();
  }
  else if (strcmp(args, "w") == 0) {
    /* 待实现 */
    printf("Watchpoint info not implemented yet.\n");
  }
  else {
    printf("Unknown info command '%s'\n", args);
  }
  return 0;
}

static int cmd_x(char *args) {
  /* 1. 检查是否有参数 */
  if (args == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  /* 2. 解析第一个参数 N (扫描长度) */
  // strtok 第一次调用传入 args
  char *n_str = strtok(args, " ");
  if (n_str == NULL) {
    printf("Error: Missing argument N\n");
    return 0;
  }

  int n = 0;
  // 读取整数 N
  sscanf(n_str, "%d", &n);

  /* 3. 解析第二个参数 EXPR (起始地址) */
  // strtok 后续调用传入 NULL
  char *addr_str = strtok(NULL, " ");
  if (addr_str == NULL) {
    printf("Error: Missing argument ADDRESS\n");
    return 0;
  }

  vaddr_t addr = 0;
  // 读取十六进制地址 (例如 "0x80000000")
  // 这里的 %lx 对应 unsigned long，适配 32/64 位地址比较通用
  // 也可以用 NEMU 定义的 FMT_WORD 宏，但 %lx 写起来最简单
  sscanf(addr_str, "%lx", (unsigned long *)&addr);

  /* 4. 循环读取并打印 */
  printf("Memory content starting at 0x%lx:\n", (unsigned long)addr);
  
  for (int i = 0; i < n; i++) {
    // A. 读取内存：每次读 4 字节 (Guest Memory)
    word_t data = vaddr_read(addr, 4);
    
    // B. 打印结果
    // 格式：地址(8位十六进制) : 数据(8位十六进制)
    printf("0x%08lx:  0x%08lx\n", (unsigned long)addr, (unsigned long)data);
    
    // C. 移动指针：地址 +4 (因为读了 4 字节)
    addr += 4;
  }

  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "帮帮我我要困死了\n Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Step the program for n instructions", cmd_si },
  { "info", "Show information about registers or watchpoints", cmd_info },
  { "x", "Scan memory", cmd_x},

};


#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
