typedef struct {
  uint32_t entry[99];
  uint32_t first, last;
  uint32_t dunno;
} dc_toc;
#define MAKE_DC_TOC_ENTRY(lba, adr, ctrl) ((lba)|((adr)<<24)|((ctrl)<<28))
#define MAKE_DC_TOC_TRACK(n) (((n)&0xff)<<16)
