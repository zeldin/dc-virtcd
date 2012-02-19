extern int memcmp(const void *s1, const void *s2, unsigned n);
extern void *memcpy(void *s1, const void *s2, unsigned n);
extern void *memset(void *s, int c, unsigned n);
extern void bzero(void *s, unsigned n);
extern void printf(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern int flash_read_sector(int partition, int sec, char *dst);

static __inline int getimask(void)
{
  register unsigned int sr;
  __asm__("stc sr,%0" : "=r" (sr));
  return (sr >> 4) & 0x0f;
}

static __inline void setimask(int m)
{
  register unsigned int sr;
  __asm__("stc sr,%0" : "=r" (sr));
  sr = (sr & ~0xf0) | (m << 4);
  __asm__("ldc %0,sr" : : "r" (sr));
}

