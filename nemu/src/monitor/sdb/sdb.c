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

#define NR_CMD ARRLEN(cmd_table)

static int is_batch_mode = false;

static int cmd_help(char *args);
static int cmd_c(char *args);
static int cmd_q(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args); 

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

void sdb_set_batch_mode() {
  is_batch_mode = true;
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
  // 改成正常退出，不要恐慌
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_x(char *args) {
  if (args == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  char *n_str = strtok(args, " ");
  if (n_str == NULL) {
    printf("Error: Missing argument N\n");
    return 0;
  }

  int n = 0;
  sscanf(n_str, "%d", &n);

  char *addr_str = strtok(NULL, " ");
  if (addr_str == NULL) {
    printf("Error: Missing argument ADDRESS\n");
    return 0;
  }

  unsigned long long addr_temp = 0;
  sscanf(addr_str, "%llx", &addr_temp);
  vaddr_t addr = (vaddr_t)addr_temp;

  printf("Memory content starting at 0x%lx:\n", (unsigned long)addr);
  
  for (int i = 0; i < n; i++) {
    word_t data = vaddr_read(addr, 4);
    printf("0x%08lx:  0x%08lx\n", (unsigned long)addr, (unsigned long)data);
    addr += 4;
  }

  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }

  bool success = true;
  word_t result = expr(args, &success); // 调用expr.c

  if (success) {
    printf("%u\n", (unsigned int) result);
  } else {
    printf("Expression evaluation failed.\n");
  }
  return 0;
}


static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Usage: d N \n");
    return 0;
  }

  int no;
  if (sscanf(args, "%d", &no) != 1) {
    printf("Invalid watchpoint number.\n");
    return 0;
  }

  bool success = delete_wp(no);
  if (success) {
    printf("Watchpoint %d deleted.\n ", no);
  } else {
    printf("Watchpoint %d not found. \n", no);
  }
  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) {
    printf("Usage: w EXPR\n");
    return 0;
  }
  new_wp(args);
  return 0;
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
    info_wp(); 
  }
  else {
    printf("Unknown info command '%s'\n", args);
  }
  return 0;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Step the program for n instructions", cmd_si },
  { "info", "Show infomation about registers (r) or watchpoint (w)", cmd_info},
  { "x", "Scan memory", cmd_x},
  { "p", "Evaluate expression", cmd_p},
  { "d", "Delete watchpoint", cmd_d},
  { "w", "Set a new watchpoint", cmd_w},


};

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
