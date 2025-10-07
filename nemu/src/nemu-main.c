#include <common.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

/* 这两个在 expr.c 里实现 */
void init_regex(void);                // 声明 init_regex
void expr_batch_test(const char *path);

int main(int argc, char *argv[]) {
  /* 批量模式：./<exe> --batch tools/gen-expr/input */
  if (argc == 3 && strcmp(argv[1], "--batch") == 0) {
    init_regex();                     // ★ 必须先初始化正则
    expr_batch_test(argv[2]);         // 再读入 input 做批测
    return 0;
  }

#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif

  engine_start();
  return is_exit_status_bad();
}
