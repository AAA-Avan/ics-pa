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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

// buffer position
static int buf_idx = 0;

static uint32_t choose(uint32_t n) {
  return rand() % n;
}

// 在buffer里追加字符串
static void gen(char *str) {
  int len = strlen(str);
  if (buf_idx + len > sizeof(buf) - 1) {
    return;
  }
  strcpy(buf + buf_idx, str);
  buf_idx += len;
}

// 产生随机空格
static void gen_rand_space() {
  if (choose(2) == 0) gen(" ");
}


static void gen_num() {
  char num_str[32];
  // 随机选择生成十进制或十六进制整数
  // 核心细节：加上 'u' 后缀 (例如 34u)，确保 C 程序进行无符号运算
  // 否则 GCC 可能会按照有符号数计算，导致结果与 NEMU 不一致
  sprintf(num_str, "%du", choose(1000)); 
  
  gen_rand_space();
  gen(num_str);
  gen_rand_space();
}

static void gen_rand_op() {
  switch (choose(4)) {
    case 0: gen("+"); break;
    case 1: gen("-"); break;
    case 2: gen("*"); break;
    case 3: gen("/"); break;
  }
}

// 用递归的方式“造树”来生成表达式
static void gen_rand_expr() {
  if (buf_idx > 60000) {
    gen_num();  // 防止表达式过长，直接生成数字
    return;
  }

  // 让 0-6 (70% 的概率) 都生成数字，直接结束递归
  switch (choose(10)) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: 
      gen_num(); 
      break;
    case 7: // 10% 概率生成括号
      gen("("); 
      gen_rand_expr(); 
      gen(")"); 
      break;
    default: // 20% 概率生成运算 (8 和 9)
      gen_rand_expr(); 
      gen_rand_op(); 
      gen_rand_expr(); 
      break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    // argv是用户输入的指令，其中第二个是测试用例的个数
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    // 每次生成前，必须重置 buf_idx
    buf_idx = 0;
    buf[0] = '\0'; // 清空 buffer
    
    gen_rand_expr();

    // 如果生成的表达式太长导致被截断或异常，跳过这次生成
    if (buf_idx >= 65535) continue;

    // 填之前code_format空
    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    // 把刚才sprintf填完的code写到fp指向的文件里
    fputs(code_buf, fp);
    fclose(fp);

    // 编译生成的 C 代码
    // 加上 -w 可以忽略 gcc 对除 0 等行为的警告
    int ret = system("gcc -w /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    // 运行生成的程序
    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    // 过滤除 0 错误
    // 如果程序里有 1/0，运行的时候会崩溃 (SIGFPE)，不会输出任何结果
    // fscanf 会读取失败，返回 -1 或 0 (不等于 1)
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    // 只有成功读到一个整数 (ret == 1)，才认为这是一个合法的测试用例
    if (ret != 1) continue;

    printf("%u %s\n", result, buf);
  }
  return 0;
}
