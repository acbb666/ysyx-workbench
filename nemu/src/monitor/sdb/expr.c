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

#include <common.h>
#include <isa.h>

#include <stdint.h>
#include <stdbool.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

extern uint32_t vaddr_read(uint32_t addr, int len);
// 与 reg.c 中的实现保持一致，返回 word_t
extern word_t isa_reg_str2val(const char *s, bool *success);

/* ---------------- token 定义 ---------------- */

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_NUM, TK_HEX, TK_REG,
  TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
  TK_NEQ, TK_AND, TK_LPAREN, TK_RPAREN,
  TK_DEREF, TK_NEG
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  { "([[:space:]]+)", TK_NOTYPE },           // spaces
  { "(0[xX][0-9a-fA-F]+[uU]?)", TK_HEX },    // hex number (可带 u/U)
  { "([0-9]+[uU]?)", TK_NUM },               // dec number (可带 u/U)
  { "\\$[A-Za-z][A-Za-z0-9]*", TK_REG },     // $pc/$a0...
  { "&&", TK_AND },                          // &&
  { "==", TK_EQ },                           // ==
  { "!=", TK_NEQ },                          // !=
  { "\\+", TK_PLUS },                        // +
  { "-",  TK_MINUS },                        // -  (可能变 TK_NEG)
  { "\\*", TK_MUL },                         // *  (可能变 TK_DEREF)
  { "/",  TK_DIV },                          // /
  { "\\(", TK_LPAREN },                      // (
  { "\\)", TK_RPAREN },                      // )
};

#define NR_REGEX ARRLEN(rules)
static regex_t re[NR_REGEX];

void init_regex(void) {
  int i, ret;
  char error_msg[128];
  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, sizeof(error_msg));
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];   // 对于 NUM/HEX/REG 存源文本（REG 含 '$'）
} Token;

static Token tokens[1 << 16];  // 放宽，容纳长表达式
static int nr_token;

/* ---------------- 词法分析：只分词，不做一元识别 ---------------- */

static bool make_token(const char *e) {
  int position = 0;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    bool matched = false;

    for (int i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        matched = true;
        int len = pmatch.rm_eo;
        const char *p = e + position;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, len, len, p);

        position += len;

        int t = rules[i].token_type;
        if (t == TK_NOTYPE) break; // 丢弃空白

        if (nr_token >= (int)ARRLEN(tokens)) { panic("too many tokens"); }
        tokens[nr_token].type = t;
        tokens[nr_token].str[0] = '\0';

        if (t == TK_NUM || t == TK_HEX || t == TK_REG) {
          int copy = len < (int)sizeof(tokens[nr_token].str) - 1
                     ? len : (int)sizeof(tokens[nr_token].str) - 1;
          memcpy(tokens[nr_token].str, p, copy);
          tokens[nr_token].str[copy] = '\0';
          assert(copy == len); // 若断言触发，可适当把 str 扩大
        }
        nr_token++;
        break;
      }
    }

    if (!matched) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

/* ---------------- 语法辅助 ---------------- */

static inline bool is_operand(int t) {
  return t == TK_NUM || t == TK_HEX || t == TK_REG || t == TK_RPAREN;
}

static bool check_parentheses(int p, int q) {
  if (p > q) return false;
  if (tokens[p].type != TK_LPAREN || tokens[q].type != TK_RPAREN) return false;
  int bal = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LPAREN) bal++;
    else if (tokens[i].type == TK_RPAREN) bal--;
    if (bal == 0 && i < q) return false; // 中途闭合 => 不是“整段外层括号”
    if (bal < 0) return false;
  }
  return bal == 0;
}

// 只考虑二元运算符：最低优先级，同级取最右；括号内忽略
static int find_main_op(int p, int q) {
  int op = -1, min_pri = 1000, bal = 0;
  for (int i = p; i <= q; i++) {
    int ty = tokens[i].type;

    if (ty == TK_LPAREN) { bal++; continue; }
    if (ty == TK_RPAREN) { bal--; continue; }
    if (bal > 0) continue;

    int pri = -1;
    switch (ty) {
      case TK_AND:                 pri = 1; break;
      case TK_EQ:  case TK_NEQ:    pri = 2; break;
      case TK_PLUS: case TK_MINUS: pri = 3; break;
      case TK_MUL:  case TK_DIV:   pri = 4; break;
      default: break; // 一元 TK_NEG/TK_DEREF 不参与
    }

    if (pri != -1 && pri <= min_pri) { // 同级取最右
      min_pri = pri;
      op = i;
    }
  }
  return op;
}

/* ---------------- 递归求值 ---------------- */

static word_t eval(int p, int q, bool *ok) {
  if (p > q) { *ok = false; return 0; }

  // 最外层括号
  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, ok);
  }

  // 先找“二元主运算符”
  int op = find_main_op(p, q);
  if (op >= 0) {
    word_t v1 = eval(p, op - 1, ok); if (!*ok) return 0;
    word_t v2 = eval(op + 1, q, ok); if (!*ok) return 0;

    switch (tokens[op].type) {
      case TK_PLUS:  return v1 + v2;
      case TK_MINUS: return v1 - v2;
      case TK_MUL:   return v1 * v2;
      case TK_DIV:   if (v2 == 0) { *ok = false; return 0; } else return v1 / v2;
      case TK_EQ:    return (word_t)(v1 == v2);
      case TK_NEQ:   return (word_t)(v1 != v2);
      case TK_AND:   return (word_t)((v1 != 0) && (v2 != 0));
      default: *ok = false; return 0;
    }
  }

  // 没有二元主运算符：只可能是
  // 1) 单个原子  2) 前缀一元 + 子表达式
  if (p == q) {
    switch (tokens[p].type) {
      case TK_NUM: return (word_t)strtoull(tokens[p].str, NULL, 10);
      case TK_HEX: return (word_t)strtoull(tokens[p].str, NULL, 16);
      case TK_REG: {
        bool ok2 = true;
        // 去掉前导 '$'
        const char *name = tokens[p].str[0] == '$' ? tokens[p].str + 1 : tokens[p].str;
        word_t v = isa_reg_str2val(name, &ok2);
        *ok = ok2;
        return v;
      }
      default: *ok = false; return 0;
    }
  }

  // 前缀一元（此时整段里没有二元主运算符，才能到这里）
  if (tokens[p].type == TK_NEG) {
    word_t v = eval(p + 1, q, ok);
    if (!*ok) return 0;
    // 对 word_t 取两补负；在无符号上等价于 0 - v
    return (word_t)(0 - v);
  }
  if (tokens[p].type == TK_DEREF) {
    word_t addr = eval(p + 1, q, ok);
    if (!*ok) return 0;
    // 讲义要求：总是读 4 字节，再零扩展
    uint32_t data = vaddr_read((uint32_t)addr, 4);
    return (word_t)data;
  }

  *ok = false;
  return 0;
}

/* ---------------- expr() 接口 ---------------- */

word_t expr(const char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  // —— 严格按讲义骨架：在递归求值前识别一元 * / - ——
  for (int i = 0; i < nr_token; i ++) {
    if (tokens[i].type == TK_MUL && (i == 0 || !is_operand(tokens[i - 1].type))) {
      tokens[i].type = TK_DEREF;
    } else if (tokens[i].type == TK_MINUS && (i == 0 || !is_operand(tokens[i - 1].type))) {
      tokens[i].type = TK_NEG;
    }
  }

  bool ok = true;
  word_t ans = eval(0, nr_token - 1, &ok);
  *success = ok;
  return ans;
}

/* ---------------- --batch 批量测试 ---------------- */

static void rstrip(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

// 每行: <期望值(十进制)> <空格> <表达式>
void expr_batch_test(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    printf("[batch] cannot open %s\n", path);
    return;
  }

  char line[1 << 16];
  int lineno = 0, total = 0, pass = 0, fail = 0;

  while (fgets(line, sizeof(line), fp)) {
    lineno++;
    rstrip(line);
    if (line[0] == '\0') continue;

    char *sp = strchr(line, ' ');
    if (!sp) {
      printf("[batch:%d] bad format: %s\n", lineno, line);
      continue;
    }
    *sp = '\0';
    const char *ex = sp + 1;
    while (*ex == ' ') ex++;

    uint64_t expect64 = strtoull(line, NULL, 10);
    word_t expect = (word_t)expect64;

    bool ok = true;
    word_t got = expr(ex, &ok);

    total++;
    if (!ok) {
      fail++;
      printf("[batch:%d] FAIL(invalid): expect=%" PRIu64 ", got=?, expr=%s\n",
             lineno, (uint64_t)expect, ex);
      continue;
    }
    if (got != expect) {
      fail++;
      printf("[batch:%d] FAIL: expect=%" PRIu64 ", got=%" PRIu64 ", expr=%s\n",
             lineno, (uint64_t)expect, (uint64_t)got, ex);
    } else {
      pass++;
    }
  }
  fclose(fp);

  printf("[batch] total=%d pass=%d fail=%d pass_rate=%.2f%%\n",
         total, pass, fail, total ? 100.0 * pass / total : 0.0);
}
