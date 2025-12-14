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
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  // print all general purpose registers
  int length = ARRLEN(regs);
  for (int i = 0; i < length; i ++) {
    printf("reg_name: %s, value: 0x%x\n", regs[i], cpu.gpr[i]);
    //printf("%-3s = 0x%08x\n", regs[i], cpu.gpr[i]);
  }
  // print pc
  printf("pc: 0x%x\n", cpu.pc);
  //printf("%-3s = 0x%08x\n", "pc", cpu.pc);

}

word_t isa_reg_str2val(const char *s, bool *success) {
  *success = true; // 默认假设能找到

  // 1. 检查 PC 指针
  if (strcmp(s, "pc") == 0) {
    return cpu.pc;
  }

  // 2. 特殊处理 $0 寄存器
  // 用户的 regs 定义里是 "$0"，但 s 传进来通常是 "0" (因为去掉了 $)
  // 如果不加这个判断，strcmp("0", "$0") 会失败
  if (strcmp(s, "0") == 0) {
    return cpu.gpr[0];
  }

  // 3. 扫描通用寄存器
  int length = ARRLEN(regs);
  for (int i = 0; i < length; i ++) {
    // 对比名字 (注意：这里对比的是 ra, sp, a0 等标准名字)
    if (strcmp(s, regs[i]) == 0) {
      return cpu.gpr[i];
    }
  }

  // 4. 找不到的情况
  *success = false;
  return 0;
}
