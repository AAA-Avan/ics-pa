#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// 内部辅助函数：整数转字符串，并返回写入的长度
// ai实现的
static int itoa(int value, char *str, int base) {
    char *ptr = str;
    char *start = str;

    // 1. 特判 0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return 1; // 长度为 1
    }

    // 2. 处理负数（极其关键的防溢出操作）
    int is_negative = 0;
    unsigned int uvalue;
    if (value < 0 && base == 10) {
        is_negative = 1;
        // 用无符号数做减法，完美避开 -2147483648 取绝对值时溢出的灾难
        uvalue = (unsigned int)0 - (unsigned int)value; 
    } else {
        uvalue = (unsigned int)value;
    }

    // 3. 剥洋葱：取余数（此时是倒序的）
    while (uvalue > 0) {
        int rem = uvalue % base;
        *ptr++ = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        uvalue /= base;
    }

    // 4. 补上负号
    if (is_negative) {
        *ptr++ = '-';
    }

    // 5. 封口并计算长度
    *ptr = '\0';
    int len = ptr - str;

    // 6. 反转字符串（因为之前是倒着存的）
    ptr--; // 退回 \0 的前一个字符
    while (start < ptr) {
        char tmp = *start;
        *start = *ptr;
        *ptr = tmp;
        start++;
        ptr--;
    }

    return len;
}

int printf(const char *fmt, ...) {
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  return vsnprintf(out, (size_t)-1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsprintf(out, fmt, ap);
  va_end(ap);
  return n;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  // 如果缓冲池大小为 0，直接返回（根据 C 标准要求）
  if (n == 0) return 0;

  size_t count = 0; // 记录真实写入了多少个字符
  
  while (*fmt != '\0' && count < n - 1) { // 核心：绝对不能超过 n - 1
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 's': {
          char *s = va_arg(ap, char *); 
          if (s == NULL) s = "(null)"; // 防御性编程：防止传入空指针引发缺页异常
          while (*s && count < n - 1) {
            out[count++] = *s++;
          }
          break;
        }
        case 'd': {
          int val = va_arg(ap, int);
          char tmp_buf[32]; // 32 字节足够装下 32 位整数的字符串了
          itoa(val, tmp_buf, 10); // 先转到临时缓冲区
          
          char *t = tmp_buf;
          while (*t && count < n - 1) { // 安全拷贝
            out[count++] = *t++;
          }
          break;
        }
        case 'x': {
          int val = va_arg(ap, int);
          char tmp_buf[32];
          itoa(val, tmp_buf, 16);
          
          char *t = tmp_buf;
          while (*t && count < n - 1) {
            out[count++] = *t++;
          }
          break;
        }
        // 你还可以在这里实现 %c 等其他格式
      }
    } else {
      out[count++] = *fmt;
    }
    fmt++;
  }
  
  out[count] = '\0'; // 最后的封口，极其重要！
  return count;
}

#endif
