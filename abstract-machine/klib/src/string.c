#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  const char *p = s;
  while(*p != '\0') {
    p++;
  }
  return p - s;
}

char *strcpy(char *dst, const char *src) {
  size_t i;
  for (i = 0; src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  size_t dst_len = strlen(dst);
  const char *p = src;
  while(*p != '\0') {
    dst[dst_len ++] = *p;
    p++;
  }
  dst[dst_len] = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 == *s2 && *s1 != '\0') {
      s1++;
      s2++;
  }
  return (unsigned char)(*s1) - (unsigned char)(*s2);
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n > 0 && *s1 == *s2 && *s1 != '\0') {
    s1++;
    s2++;
    n--;
  }
  if (n == 0) {
    return 0;
  }
  return (unsigned char)(*s1) - (unsigned char)(*s2);
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n-- > 0) {
    *p++ = (unsigned char)c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;

 if (d < s) {
    while (n-- > 0) {
      *d++ = *s++;
    }
  } else if (d > s) {
    d += n;
    s += n;
    while (n-- > 0) {
      *(--d) = *(--s); 
    }
  }
  
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *d = (unsigned char *)out;
  const unsigned char *s = (const unsigned char *)in;
  while (n-- > 0) {
    *d++ = *s++;
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

#endif
