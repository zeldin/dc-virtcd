#include "util.h"

int memcmp(const void *s1, const void *s2, unsigned n)
{
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  while(n--)
    if(*p1 != *p2)
      return *p1 - *p2;
    else {
      p1++;
      p2++;
    }
  return 0;
}

void *memcpy(void *s1, const void *s2, unsigned n)
{
  char *p1 = s1;
  const char *p2 = s2;
  while(n--)
    *p1++ = *p2++;
}

extern void *memset(void *s, int c, unsigned n)
{
  char *p = s;
  while(n--)
    *p++ = c;
  return s;
}

extern void bzero(void *s, unsigned n)
{
  memset(s, 0, n);
}

