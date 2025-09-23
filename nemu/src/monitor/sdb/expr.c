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
uint32_t vaddr_read(uint32_t addr, int len);
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
    TK_NOTYPE = 256, TK_EQ,
    TK_NUM, TK_HEX, TK_REG,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
    TK_NEQ, TK_AND, TK_LPAREN, TK_RPAREN, TK_DEREF,TK_NEG
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */
  {" +", TK_NOTYPE},       // spaces
  {"0x[0-9a-fA-F]+", TK_HEX}, // hex number
  {"[0-9]+", TK_NUM},      // decimal number
  {"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG}, // register
  {"\\+", TK_PLUS},        // plus
  {"-", TK_MINUS},         // minus
  {"\\*", TK_MUL},         // multiply
  {"/", TK_DIV},           // divide
  {"\\(", TK_LPAREN},      // left parenthesis
  {"\\)", TK_RPAREN},      // right parenthesis
  {"==", TK_EQ},           // equal
  {"!=", TK_NEQ},          // not equal
  {"&&", TK_AND}           // logical and
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
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
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type) {
          case TK_NOTYPE: break;
          case TK_NUM: case TK_HEX: case TK_REG:
            tokens[nr_token].type = rules[i].token_type;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          case TK_PLUS: case TK_MINUS: case TK_MUL: case TK_DIV:
          case TK_EQ: case TK_NEQ: case TK_AND:
          case TK_LPAREN: case TK_RPAREN:
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
          default: TODO();
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  // 处理单目运算符：* => deref, - => unary minus
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == TK_MUL) {
      if (i == 0 || (tokens[i-1].type != TK_NUM && tokens[i-1].type != TK_HEX &&
                     tokens[i-1].type != TK_REG && tokens[i-1].type != TK_RPAREN)) {
        tokens[i].type = TK_DEREF;
      }
    } else if (tokens[i].type == TK_MINUS) {
      if (i == 0 || (tokens[i-1].type != TK_NUM && tokens[i-1].type != TK_HEX &&
                     tokens[i-1].type != TK_REG && tokens[i-1].type != TK_RPAREN)) {
        tokens[i].type = TK_NEG;
      }
    }
  }

  return true;
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  uint32_t eval(int p, int q) {
    printf("eval: p=%d, q=%d\n", p, q);
    for(int i=p;i<=q;i++) printf("%d:%d ", i, tokens[i].type);
    printf("\n");
    assert(p <= q);
    if (p == q) {
      if (tokens[p].type == TK_NUM) return atoi(tokens[p].str);
      else if (tokens[p].type == TK_HEX) return strtoul(tokens[p].str, NULL, 16);
      else if (tokens[p].type == TK_REG) {
        bool ok = true;
        uint32_t val = isa_reg_str2val(tokens[p].str + 1, &ok);
        assert(ok);
        return val;
      } else assert(0);
    } else if (tokens[p].type == TK_LPAREN && tokens[q].type == TK_RPAREN) {
      return eval(p+1, q-1);
    } else {
      int op = -1, min_pri = 100, count = 0;
      for (int i = p; i <= q; i++) {
        if (tokens[i].type == TK_LPAREN) { count++; continue; }
        if (tokens[i].type == TK_RPAREN) { count--; continue; }
        if (count > 0) continue;
        int pri = -1;
        switch (tokens[i].type) {
          case TK_AND: pri = 1; break;
          case TK_EQ: case TK_NEQ: pri = 2; break;
          case TK_PLUS: case TK_MINUS: pri = 3; break;
          case TK_MUL: case TK_DIV: pri = 4; break;
          case TK_DEREF: case TK_NEG: pri = 5; break;
        }
        if (pri!=-1&&pri <= min_pri) { min_pri = pri; op = i; }
      }
      if (op == -1) {
  	assert(p == q); // 必须只剩一个 token
  	if (tokens[p].type == TK_NUM) return atoi(tokens[p].str);
  	else if (tokens[p].type == TK_HEX) return strtoul(tokens[p].str, NULL, 16);
  	else if (tokens[p].type == TK_REG) {
    	bool ok = true;
    	uint32_t val = isa_reg_str2val(tokens[p].str + 1, &ok);
    	assert(ok);
    	return val;
  	} else assert(0);
	}

      if (tokens[op].type == TK_DEREF) {
    	uint32_t addr = eval(op+1, q);
    	return vaddr_read(addr, 4);
      } else if (tokens[op].type == TK_NEG) {
    	return -eval(op+1, q);
      }	

      if (tokens[op].type == TK_NEG) {
        return -(int32_t)eval(op+1, q);
      }

      uint32_t val1 = eval(p, op-1);
      uint32_t val2 = eval(op+1, q);
      switch (tokens[op].type) {
        case TK_PLUS: return val1 + val2;
        case TK_MINUS: return val1 - val2;
        case TK_MUL: return val1 * val2;
        case TK_DIV: assert(val2 != 0); return val1 / val2;
        case TK_EQ: return val1 == val2;
        case TK_NEQ: return val1 != val2;
        case TK_AND: return val1 && val2;
        default: assert(0);
      }
    }
    return 0;
  }

  *success = true;
  return eval(0, nr_token-1);
}
