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

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  for (int i = 0; i < 32; i++) {
    uint32_t v = (uint32_t)cpu.gpr[i];  // riscv32: 直接用 32 位
    printf("%-3s  0x%08x  (%10u, %11d)\n",
           reg_name(i), v, v, (int32_t)v);
  }
  printf("pc   0x%08x\n", (uint32_t)cpu.pc);
}

/*
 * 支持名称：
 *   - pc
 *   - x0..x31
 *   - ABI: zero, ra, sp, gp, tp, t0..t6, s0..s11, a0..a7
 *   - reg_name(i) 返回的名字（如 "$0"/"ra"/"sp"...）
 *   - 名字前可带 '$'（如 "$pc", "$ra"）
 */
uint32_t isa_reg_str2val(const char *s, bool *success) {
  if (success) *success = false;
  if (!s || !*s) return 0;

  const char *name = (*s == '$') ? s + 1 : s;

  // pc
  if (strcmp(name, "pc") == 0) {
    if (success) *success = true;
    return (uint32_t)cpu.pc;
  }

  // xN
  if (name[0] == 'x' && isdigit((unsigned char)name[1])) {
    char *endp = NULL;
    long idx = strtol(name + 1, &endp, 10);
    if (*endp == '\0' && 0 <= idx && idx < 32) {
      if (success) *success = true;
      return (uint32_t)cpu.gpr[idx];
    }
  }

  // ABI 名
  static const char *abi_names[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0","s1","a0","a1","a2","a3","a4","a5",
    "a6","a7","s2","s3","s4","s5","s6","s7",
    "s8","s9","s10","s11","t3","t4","t5","t6"
  };
  for (int i = 0; i < 32; i++) {
    if (strcmp(name, abi_names[i]) == 0) {
      if (success) *success = true;
      return (uint32_t)cpu.gpr[i];
    }
  }

  // 与 reg_name(i) 一致的名字（你本地定义）
  for (int i = 0; i < 32; i++) {
    const char *rn = reg_name(i);
    if (rn && strcmp(name, rn) == 0) {
      if (success) *success = true;
      return (uint32_t)cpu.gpr[i];
    }
  }

  // 额外兼容 "$0" -> zero
  if (strcmp(name, "$0") == 0) {
    if (success) *success = true;
    return (uint32_t)cpu.gpr[0];
  }

  return 0;
}
