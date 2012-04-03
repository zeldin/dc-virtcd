typedef struct {
  uint32_t entry[99];
  uint32_t first, last;
  uint32_t dunno;
} dc_toc;
#define MAKE_DC_TOC_ENTRY(lba, adr, ctrl) ((lba)|((adr)<<24)|((ctrl)<<28))
#define MAKE_DC_TOC_TRACK(n) (((n)&0xff)<<16)
#define GET_DC_TOC_ENTRY_LBA(n) ((n)&0x00ffffff)
#define GET_DC_TOC_ENTRY_ADR(n) (((n)&0x0f000000)>>24)
#define GET_DC_TOC_ENTRY_CTRL(n) (((n)&0xf0000000)>>28)
#define GET_DC_TOC_TRACK(n) (((n)&0x00ff0000)>>16)
