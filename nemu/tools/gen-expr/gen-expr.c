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


#include <ctype.h>

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

static void gen_num() {
    char num[16];
    int val = rand() % 100;
    sprintf(num, "%d", val);
    strcat(buf, num);
}

static char gen_op() {
    char ops[] = "+-*/";
    return ops[rand() % 4];
}

static void gen_rand_expr_inner(int depth);

static void gen_neg_expr(int depth) {
    if(rand() % 2) {  // 50%概率加负号
        strcat(buf, "(-");
        gen_rand_expr_inner(depth);
        strcat(buf, ")");
    } else {
        gen_rand_expr_inner(depth);
    }
}

static void gen_rand_expr_inner(int depth) {
    if(depth > 5) { 
        gen_num(); 
        return; 
    }

    int type = rand() % 3; // 0:数字, 1:括号, 2:二元运算
    switch(type) {
        case 0: 
            gen_num(); 
            break;
        case 1:
            strcat(buf, "(");
            gen_rand_expr_inner(depth + 1);
            strcat(buf, ")");
            break;
        case 2:
        {
            // 左操作数
            gen_neg_expr(depth + 1);

            // 操作符
            char op = gen_op();
            int len = strlen(buf);
            buf[len] = op;
            buf[len+1] = '\0';

            if(op == '/') {
                // 右操作数必须非零
                strcat(buf, "(");
                int right;
                do { right = rand() % 99 + 1; } while(right == 0);
                char tmp[16]; sprintf(tmp, "%d", right);
                strcat(buf, tmp);
                strcat(buf, ")");
            } else {
                // 右操作数，可能带负号
                gen_neg_expr(depth + 1);
            }
            break;
        }
    }
}

static void gen_rand_expr() {
    buf[0] = '\0';
    gen_rand_expr_inner(0);
}



typedef enum {
    TK_NUM, TK_HEX, TK_REG,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
    TK_EQ, TK_NEQ, TK_AND,
    TK_LPAREN, TK_RPAREN,
    TK_DEREF
} TokenType;

typedef struct {
    TokenType type;
    uint32_t val;
    char str[32];   // 寄存器名
}Token;

static Token tokens[65536];
static int tok_len;

static bool is_operator(TokenType t) {
    return t==TK_PLUS||t==TK_MINUS||t==TK_MUL||t==TK_DIV||
           t==TK_EQ||t==TK_NEQ||t==TK_AND;
}

static void make_token(const char *expr) {
    tok_len = 0;
    int i = 0;
    while(expr[i]) {
        if(isspace(expr[i])) { i++; continue; }

        if(isdigit(expr[i])) {
            // 十进制
            uint32_t val = 0;
            while(isdigit(expr[i])) { val = val*10 + (expr[i]-'0'); i++; }
            tokens[tok_len].type = TK_NUM;
            tokens[tok_len].val = val;
            tok_len++;
        } 
        else if(expr[i]=='0' && expr[i+1]=='x') {
            // 十六进制
            i += 2;
            uint32_t val = 0;
            while(isxdigit(expr[i])) {
                if(isdigit(expr[i])) val = val*16 + (expr[i]-'0');
                else val = val*16 + (tolower(expr[i])-'a'+10);
                i++;
            }
            tokens[tok_len].type = TK_HEX;
            tokens[tok_len].val = val;
            tok_len++;
        } 
        else if(expr[i]=='$') {
            // 寄存器
            i++;
            int j=0;
            while(isalnum(expr[i])) tokens[tok_len].str[j++] = expr[i++];
            tokens[tok_len].str[j] = '\0';
            tokens[tok_len].type = TK_REG;
            tok_len++;
        }
        else if(expr[i]=='*') {
            // 区分乘法与指针解引用
            if(tok_len==0 || tokens[tok_len-1].type==TK_LPAREN || is_operator(tokens[tok_len-1].type))
                tokens[tok_len].type = TK_DEREF;
            else
                tokens[tok_len].type = TK_MUL;
            tok_len++; i++;
        }
        else if(expr[i]=='+') { tokens[tok_len].type = TK_PLUS; tok_len++; i++; }
        else if(expr[i]=='-') { tokens[tok_len].type = TK_MINUS; tok_len++; i++; }
        else if(expr[i]=='/') { tokens[tok_len].type = TK_DIV; tok_len++; i++; }
        else if(expr[i]=='(') { tokens[tok_len].type = TK_LPAREN; tok_len++; i++; }
        else if(expr[i]==')') { tokens[tok_len].type = TK_RPAREN; tok_len++; i++; }
        else if(expr[i]=='=' && expr[i+1]=='=') { tokens[tok_len].type=TK_EQ; tok_len++; i+=2; }
        else if(expr[i]=='!' && expr[i+1]=='=') { tokens[tok_len].type=TK_NEQ; tok_len++; i+=2; }
        else if(expr[i]=='&' && expr[i+1]=='&') { tokens[tok_len].type=TK_AND; tok_len++; i+=2; }
        else { printf("Unknown char: %c\n", expr[i]); i++; }
    }
}

// -------------------------- 括号检查 --------------------------
static bool check_parentheses(int p, int q) {
    if(tokens[p].type != TK_LPAREN || tokens[q].type != TK_RPAREN) return false;
    int count = 0;
    for(int i=p;i<=q;i++){
        if(tokens[i].type==TK_LPAREN) count++;
        else if(tokens[i].type==TK_RPAREN) count--;
        if(count<0) return false;
        if(count==0 && i<q) return false;
    }
    return count==0;
}

// -------------------------- 主运算符选择 --------------------------
static int get_priority(TokenType t) {
    switch(t){
        case TK_DEREF: return 5;
        case TK_MUL:
        case TK_DIV: return 4;
        case TK_PLUS:
        case TK_MINUS: return 3;
        case TK_EQ:
        case TK_NEQ: return 2;
        case TK_AND: return 1;
        default: return 0;
    }
}

static int find_main_op(int p,int q) {
    int op=-1, min_pri=10, count=0;
    for(int i=p;i<=q;i++){
        if(tokens[i].type==TK_LPAREN) {count++; continue;}
        if(tokens[i].type==TK_RPAREN) {count--; continue;}
        if(count>0) continue;
        int pri = get_priority(tokens[i].type);
        if(pri && pri <= min_pri) {min_pri=pri; op=i;}
    }
    return op;
}

// -------------------------- 递归求值 --------------------------

//增加寄存器和指针解引用处理
extern uint32_t isa_reg_str2val(const char *s, bool *success);
extern uint32_t vaddr_read(uint32_t addr, int len);

static uint32_t eval(int p,int q) {
    assert(p<=q);
    if(p==q){
        switch(tokens[p].type){
            case TK_NUM: return tokens[p].val;
            case TK_HEX: return tokens[p].val;
            case TK_REG: {
                bool ok=false;
                uint32_t v = isa_reg_str2val(tokens[p].str,&ok);
                assert(ok);
                return v;
            }
            default: assert(0);
        }
    }
    else if(check_parentheses(p,q)){
        return eval(p+1,q-1);
    }
    else{
        int op = find_main_op(p,q);
        assert(op!=-1);

        if(tokens[op].type==TK_DEREF){
            uint32_t addr = eval(op+1,q);
            return vaddr_read(addr,4);  // 4字节
        }

        uint32_t val1 = eval(p, op-1);
        uint32_t val2 = eval(op+1, q);

        switch(tokens[op].type){
            case TK_PLUS: return val1+val2;
            case TK_MINUS: return val1-val2;
            case TK_MUL: return val1*val2;
            case TK_DIV: assert(val2!=0); return val1/val2;
            case TK_EQ: return val1==val2;
            case TK_NEQ: return val1!=val2;
            case TK_AND: return val1 && val2;
            default: assert(0);
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
