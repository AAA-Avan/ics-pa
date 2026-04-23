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
  char *p = out;
  while (*fmt != '\0') {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 's': {
          char *s = va_arg(ap, char *); 
          while (*s) *p++ = *s++;     
          break;
        }
        case 'd': {
          int n = va_arg(ap, int);   
          p += itoa(n, p, 10);         
          break;
        }
        case 'x': {
          int n = va_arg(ap, int);   
          p += itoa(n, p, 16);         
          break;
        }

      }
    } else {
      *p++ = *fmt;
    }
    fmt++;
  }
  *p = '\0';
  return p - out;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsprintf(out, fmt, ap);
  va_end(ap);
  return n;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
