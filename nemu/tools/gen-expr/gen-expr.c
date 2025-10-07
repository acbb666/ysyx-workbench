// tools/gen-expr/gen-expr.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

static char buf[1 << 16]; // 表达式缓冲
static int  cur;

static inline uint32_t choose(uint32_t n) { return (uint32_t)(rand() % n); }

static inline bool ensure(int need) {
  return cur + need < (int)sizeof(buf) - 1;
}

static void emit(const char *s) {
  int n = (int)strlen(s);
  if (ensure(n)) { memcpy(buf + cur, s, n); cur += n; }
}

static void emit_ch(char c) {
  if (ensure(1)) buf[cur++] = c;
}

static void gen_space(void) {
  // 随机插入 0~3 个空白，包含制表符，覆盖 tokenizer
  static const char *opt[] = {"", " ", "  ", "\t", "   "};
  emit(opt[choose(5)]);
}

// ---------- 原子：无符号常量（带 u 后缀） ----------
static void gen_num_nonzero(void) {
  unsigned x;
  do { x = (unsigned)rand(); } while (x == 0);
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%uu", x); // 关键：u 后缀
  emit(tmp);
}

static void gen_num(void) {
  unsigned x = (unsigned)rand();
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%uu", x); // 关键：u 后缀
  emit(tmp);
}

// ---------- 一元负号（覆盖 TK_NEG 的测试） ----------
static void gen_unary_minus_maybe(void) {
  if (choose(5) == 0) { emit("-"); gen_space(); }
}

// ---------- 递归生成 ----------
static void gen_rand_expr(int depth);

static void gen_paren(int depth) {
  gen_space(); emit_ch('(');
  gen_rand_expr(depth + 1);
  emit_ch(')'); gen_space();
}

static void gen_op_and_right(int depth, char *out_op) {
  // 降低 '/' 的除 0 概率（仍保留一定随机性）
  static const char ops[] = "+-**//"; // * 和 / 稍微多点也可
  char op = ops[choose(sizeof(ops) - 1)];
  emit_ch(op); gen_space();
  *out_op = op;

  if (op == '/') {
    if (choose(10) < 7) { // 70% 直接使用非零原子
      gen_num_nonzero(); gen_space(); return;
    }
  }
  gen_rand_expr(depth + 1);
}

static void gen_rand_expr(int depth) {
  // 控制深度与长度，避免 buf 溢出
  if (depth > 8 || !ensure(64)) {
    gen_unary_minus_maybe();
    gen_num();
    return;
  }

  switch (choose(3)) {
    case 0: // 原子
      gen_unary_minus_maybe();
      gen_num();
      break;
    case 1: // 括号
      gen_paren(depth);
      break;
    default: { // 二元
      gen_rand_expr(depth + 1); gen_space();
      char op; gen_op_and_right(depth, &op);
      (void)op;
      break;
    }
  }
}

static void gen_rand_expr_top(void) {
  cur = 0;
  gen_rand_expr(0);
  buf[cur] = '\0';
}

// ---------- 用 gcc 计算表达式值（无符号 32 位） ----------
static bool eval_by_gcc(const char *expr, uint32_t *out) {
  // 写临时 C 文件
  char cpath[] = "/tmp/geXXXXXX.c";
  int fd = mkstemps(cpath, 2); // 保留 .c 后缀
  if (fd < 0) return false;
  FILE *cf = fdopen(fd, "w");
  if (!cf) { unlink(cpath); return false; }

  // 强制整体按无符号并截断到 32 位，保持与 NEMU 约定一致
  fprintf(cf,
    "#include <stdio.h>\n"
    "#include <stdint.h>\n"
    "int main(){\n"
    "  uint32_t result = (uint32_t)((unsigned)(%s));\n"
    "  printf(\"%%u\", result);\n"
    "  return 0;\n"
    "}\n", expr);
  fclose(cf);

  // 编译
  char bpath[] = "/tmp/geXXXXXX.out";
  int bfd = mkstemps(bpath, 4);
  if (bfd >= 0) close(bfd);
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "gcc -std=c11 -O2 -w \"%s\" -o \"%s\" 2>/dev/null", cpath, bpath);
  int rc = system(cmd);
  unlink(cpath);
  if (rc != 0) { unlink(bpath); return false; }

  // 运行
  char run_cmd[512];
  snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bpath);
  FILE *pp = popen(run_cmd, "r");
  if (!pp) { unlink(bpath); return false; }

  char outbuf[64] = {0};
  size_t n = fread(outbuf, 1, sizeof(outbuf) - 1, pp);
  int ec = pclose(pp);
  unlink(bpath);

  if (ec != 0 || n == 0) return false;
  *out = (uint32_t)strtoul(outbuf, NULL, 10);
  return true;
}

// ---------- 主程序 ----------
// 用法：./gen-expr N  -> 输出 N 行「<结果> <表达式>」
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <N>\n", argv[0]);
    return 1;
  }
  int N = atoi(argv[1]);
  // 混入 pid，避免多个并发进程生成相同序列
  srand((unsigned)time(NULL) ^ (unsigned)getpid());

  int ok = 0;
  while (ok < N) {
    gen_rand_expr_top();
    if (cur <= 0) continue;

    uint32_t val;
    if (!eval_by_gcc(buf, &val)) continue; // 过滤除 0 / 运行失败样例

    printf("%u %s\n", val, buf);
    ok++;
  }
  return 0;
}
