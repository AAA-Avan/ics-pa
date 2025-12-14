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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <memory/vaddr.h> // for vaddr_read

static bool check_parentheses(int p, int q);
static word_t eval(int p, int q);

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_DEC,  // 十进制整数 (Decimal)
  TK_HEX,  // 十六进制整数 (Hexadecimal)
  TK_REG,  // 寄存器 (Register)

  TK_DEREF,    // 指针解引用 (Pointer Dereference)

  TK_NEQ, TK_AND,

  // 如果你还想支持 !=, &&, || 等多字符运算符，也可以在这里加
  // TK_NEQ, TK_AND, ...
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},           // minus(这里不用转义符号)
  {"\\*", '*'},         // mutiply
  {"/", '/'},           // divide
  {"==", TK_EQ},        // equal
  {"!=", TK_NEQ},       // not equal
  {"&&", TK_AND},       // logical and
  {"\\(", '('},         // left parenthesis
  {"\\)", ')'},         // right parenthesis

  {"0x[0-9a-fA-F]+[uU]?", TK_HEX},    // Hexadecimal: 0x...
  {"[0-9]+[uU]?", TK_DEC},            // Decimal
  {"\\$[a-z0-9]+", TK_REG},      // Register




};

// ARRLEN是一个自动计算数组长度的宏，NR_REGEX是规则的数量
#define NR_REGEX ARRLEN(rules)

// 接受编译结果的数组，regex_t是regex.h库中定义的数据结构，用来存放编译规则
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    // 根据rules中的规则，将编译结果存入re数组
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      // 拿re中编译好的规则，去匹配e+position开始的字符串,pmatch存放匹配结果
      // pmatch.rm_so是匹配到的子串在e+position中的起始位置,不能跳过空格
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;

          case TK_DEC:
          case TK_HEX:
          case TK_REG:
            tokens[nr_token].type = rules[i].token_type;
            // 防溢出检查
            if (substr_len > 31) {
              panic("Buffer Overflow: token is too long!");
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            nr_token++;
            break;
          
          default:
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static bool check_parentheses(int p, int q) {
  /* 1. 最基本的检查：如果开头不是 '(' 或者结尾不是 ')', 那肯定不是被括号包围的 */
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  /* 2. 检查括号是否匹配，且“最外层”括号必须包裹整个表达式 */
  int level = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') {
      level++;
    } else if (tokens[i].type == ')') {
      level--;
    }

    /* 致命情况：如果还没扫描到最后 (i < q)，level 就变回 0 了
     * 这说明左边的括号已经闭合了。
     * 这虽然首尾是括号，但它们不是“一对”的，不能脱去。
     */
    if (level == 0 && i < q) {
      return false;
    }
  }

  /* 3. 如果扫描完 level 不为 0，说明括号不匹配（比如缺右括号），但这通常意味着非法表达式 */
  return level == 0;
}

word_t eval(int p, int q) {   // p,q是tokens的下标
  // 去除错误区间
  if (p > q) {
    return 0;
  }
  
  // 单个的token
  else if (p == q) {
    word_t num = 0;
    /* Extract the number from tokens[p].str */
    if (tokens[p].type == TK_DEC) {
      // string to unsigned long long
      num = strtoull(tokens[p].str, NULL, 10); // 十进制
    } else if (tokens[p].type == TK_HEX) {
      num = strtoull(tokens[p].str, NULL, 16); // 十六进制
    } else if (tokens[p].type == TK_REG) {
      bool success;
      // tokens[p].str + 1 用于跳过开头的 '$' 符号
      num = isa_reg_str2val(tokens[p].str + 1, &success);
      if (!success) {
        printf("Error: Unknown register '%s'\n", tokens[p].str);
        // 在实验阶段，直接 assert(0) 暂停方便调试
        // 如果是成品 shell，这里应该返回一个错误码
        assert(0); 
      }
    }
    return num;
  }
  
  // 去除括号
  else if (check_parentheses(p, q) == true) {
    return eval(p + 1, q - 1);
  }

  // 处理取址
  else if (tokens[p].type == TK_DEREF) {
    word_t addr = eval(p + 1, q);
    // 这里的 4 表示读 4 个字节 (word_t)。vaddr_read 需要包含 <memory/vaddr.h>
    return vaddr_read(addr, 4); 
  }
  
  // 分割求值 (核心逻辑，算法逻辑在讲义里给出了)
  else {
    // 寻找主运算符 (Main Operator)
    // 规则：1. 括号外的运算符 2. 优先级最低 3. 同优先级最右边
    
    int op = -1; // 主运算符的位置
    int level = 0; // 括号层级
    int op_priority = -1; // 当前记录的主运算符优先级 (数值越小优先级越高，或者反过来，看你怎么定)

    for (int i = p; i <= q; i++) {
      int type = tokens[i].type;

      if (type == '(') {
        level++;
      } else if (type == ')') {
        level--;
      } else if (level == 0 && (type == '+' || type == '-' || type == '*' || type == '/' || type == TK_EQ || type == TK_NEQ || type == TK_AND)) {
        int curr_priority = 0;
        if (type == TK_AND) curr_priority = 0;
        else if (type == TK_EQ || type == TK_NEQ) curr_priority = 1;
        else if (type == '+' || type == '-') curr_priority = 2;
        else if (type == '*' || type == '/') curr_priority = 3;

        // 核心判断：找优先级最低的，同级找最右边的
        if (op == -1 || curr_priority <= op_priority) {
          op = i;
          op_priority = curr_priority;
        }
      }
    }

    // 如果循环完没找到运算符（理论上不该发生，除非表达式非法）
    if (op == -1) assert(0);

    // 递归求值
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);

    // 根据主运算符类型进行计算
    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': 
        if (val2 == 0) panic("Division by zero"); // 防止除0
        return val1 / val2;
      case TK_EQ:  return val1 == val2;
      case TK_NEQ: return val1 != val2;
      case TK_AND: return val1 && val2;
      default: assert(0);
    }
  }
  return 0;
}



word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  // 区分乘法和指针解引用
  int i;
  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '*') {
      if (i == 0 
          || tokens[i-1].type == '+' 
          || tokens[i-1].type == '-' 
          || tokens[i-1].type == '*' 
          || tokens[i-1].type == '/' 
          || tokens[i-1].type == '('
          || tokens[i-1].type == TK_EQ 
          || tokens[i-1].type == TK_NEQ 
          || tokens[i-1].type == TK_AND 
          ) {
        tokens[i].type = TK_DEREF;
      }
    }
  }

  return eval(0, nr_token - 1);
}