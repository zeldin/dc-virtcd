#include "video.h"

static void *halffontbase, *fullfontbase;

struct sysinfo {
  unsigned char id[8];
  unsigned char settings[16];
};

int syscall_read_flash(int offs, void *ptr, int cnt)
{
  return (*(int (**)(int, void *, int, int))0x8c0000b8)(offs,ptr,cnt,1);  
}

static void get_sysinfo(struct sysinfo *si)
{
  int i;

  syscall_read_flash(0x1a056, si->id, 8);
  syscall_read_flash(0x1a000, si->settings, 5);
  for(i=0; i<11; i++)
    si->settings[5+i] = 0;
}

static int query_tv()
{
  //PALM (Brazil) is 60Hz. The sweep frequency is closer to NTSC than
  //PAL and it only has 525 lines.
  //
  //PALM (Uruguay, Paraguay and Argentina) has Lower bandwith than
  //PAL, but otherwise pretty much the same.
  //
  //http://www.alkenmrs.com/video/standards.html for details

  struct sysinfo si;

  get_sysinfo(&si);
  return si.settings[4]-'0';
}

int video_check_cable(int *tv)
{
  volatile unsigned int *porta = (volatile unsigned int *)0xff80002c;
  unsigned short v;
  
  /* PORT8 and PORT9 is input */
  *porta = (*porta & ~0xf0000) | 0xa0000;
  
  /* Read PORT8 and PORT9 */
  v = ((*(volatile unsigned short *)(porta+1))>>8)&3;

  if(tv)
    if (! (v&2) )
      *tv = 0;
    else
      *tv = query_tv();

  return v;
}


/* Initialize the PVR subsystem to a known state */

static unsigned int scrn_params[] = {
	0x80e8, 0x00160000,	/* screen control */
	0x8044, 0x00800000,	/* pixel mode (vb+0x11) */
	0x805c, 0x00000000,	/* Size modulo and display lines (vb+0x17) */
	0x80d0, 0x00000100,	/* interlace flags */
	0x80d8, 0x020c0359,	/* M */
	0x80cc, 0x001501fe,	/* M */
	0x80d4, 0x007e0345,	/* horizontal border */
	0x80dc, 0x00240204,	/* vertical position */
        0x80e0, 0x07d6c63f,	/* sync control */
	0x80ec, 0x000000a4,	/* horizontal position */
	0x80f0, 0x00120012,	/* vertical border */
	0x80c8, 0x03450000,	/* set to same as border H in 80d4 */
	0x8068, 0x027f0000,	/* (X resolution - 1) << 16 */
	0x806c, 0x01df0000,	/* (Y resolution - 1) << 16 */
	0x804c, 0x000000a0,	/* display align */
	0x8118, 0x00008040,	/* M */
	0x80f4, 0x00000401,	/* anti-aliasing */
	0x8048, 0x00000009,	/* alpha config */
	0x7814, 0x00000000,	/* More interrupt control stuff (so it seems)*/
	0x7834, 0x00000000,
	0x7854, 0x00000000,
	0x7874, 0x00000000,
	0x78bc, 0x4659404f,
	0x8040, 0x00000000	/* border color */
};

static void set_regs(unsigned int *values, int cnt)
{
  volatile unsigned char *regs = (volatile unsigned char *)(void *)0xa05f0000;
  unsigned int r, v;
  
  while(cnt--) {
    r = *values++;
    v = *values++;
    *(volatile unsigned int *)(regs+r) = v;
  }
}


static unsigned long syscall_b4(int n)
{
  unsigned long r;
  asm("jsr @%1\nmov %2,r1\nmov r0,%0" :
      "=r" (r) : "r" (*(void **)0x8c0000b4), "r" (n) :
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "pr");
  return r;
}

void video_init_pvr()
{
  volatile unsigned int *vbl = (volatile unsigned int *)(void *)0xa05f810c;

  while (!(*vbl & 0x01ff));
  while (*vbl & 0x01ff);
  set_regs(scrn_params, sizeof(scrn_params)/sizeof(scrn_params[0])/2);

  halffontbase = (void*)syscall_b4(0);
  fullfontbase = ((char *)halffontbase) + (96*3)*(24*3/2);
}


/* Set up video registers to the desired
   video mode
  
   in:
   	cabletype  (0=VGA, 2=RGB, 3=Composite)
   	pixel mode (0=RGB555, 1=RGB565, 3=RGB888)
   	tvmode     (0 = off, 1 = on)
   	res        (0 = 320 x 240, 1 = 640 x 240, 2 = 640 x 480) 
        hz50       (0 = 60Hz, 1 = 50 Hz)
        pal        (0 = NTSC, 1 = PAL, 2 = PALM, 3 = PALN)
        voffset    (vertical offset of screen in TV mode. Added to the
                    base offset.)
*/
void video_init_video(int cabletype, int mode, int tvmode, int res,
		      int hz50, int pal, int voffset)
{
  static int bppshifttab[]= { 1,1,0,2 };
  static int videobase=0xa05f8000;
  static int cvbsbase=0xa0702c00;
  static int hpos=0xa4;
  static int hvcounter31=0x020c0359;
  static int hvcounter15=0x01060359;
  static int hvcounter3150=0x02700359;
  static int hvcounter1550=0x01380359;
  static int hborder=0x007e0345;
  int laceoffset=0;

  int shift, lines, hvcounter, modulo, words_per_line, vpos;
  unsigned int tmp, videoflags, displaytmp, attribs;

  volatile unsigned int *vbl = (volatile unsigned int *)(void *)0xa05f810c;

  while (!(*vbl & 0x01ff));
  while (*vbl & 0x01ff);

  *(int *)(videobase+0x44)=0;
  *(int *)(videobase+0xd0)=0;

  if(!(cabletype&2))
    hz50 = pal = 0;

  if(res==0 || res==1)
    hvcounter=(hz50? hvcounter1550 : hvcounter15);
  else
    hvcounter=(hz50? hvcounter3150 : hvcounter31);
    
  // Look up bytes per pixel as shift value
  mode=mode&3; //&3 is safety left over from asm.
  shift=bppshifttab[mode]; 
  // Get video HW address
  *(int *)(videobase+8)=0;	// Reset???
  *(int *)(videobase+0x40)=0;	// Set border colour to black
  // Set pixel clock and colour mode
  mode = (mode<<2)+1;
  lines = 240;			// Non-VGA screen has 240 display lines
  if(!(cabletype&2))		// VGA
  {
    if((res==0 && !tvmode) || res == 1)
      mode+=2;

    hvcounter=hvcounter31;

    lines = lines<<1;		// Double # of display lines for VGA
    mode  = mode|0x800000;	// Set double pixel clock
  } else
    tvmode=0;	// Running TV-mode on a TV isn't really helpful.

  *(int *)(videobase+0x50)=0;	// Set video base address
  // Video base address for short fields should be offset by one line
  *(int *)(videobase+0x54)=640<<shift;	// Set short fields video base address

  // Set screen size, modulo, and interlace flag
  videoflags=1<<8;			// Video enabled
  if(res==0)
    words_per_line=(320/4)<<shift;	// Two pixels per words normally
  else
    words_per_line=(640/4)<<shift;
  modulo=1;


  if(!(cabletype&2))	// VGA => No interlace
  {
    if(res==0 && !tvmode)
      modulo+=words_per_line;	//Render black on every other line.
  } else {
    if(res!=1)
      modulo+=words_per_line;	//Skip the black part (lores) or
                                //add one line to offset => display
				//every other line (hires)
    if(res==2)
      videoflags|=1<<4; //enable LACE 
    
#if 0
    if(!pal)
      videoflags|=1<<6; //enable NTSC (doesn't matter on jp systems,
	                // european systems seems to be able to produce 
                        // it, US systems are unknown)

    if(hz50)
      videoflags|=1<<7;	//50Hz
#else
    videoflags|=(pal&3)<<6;
#endif
  }

  //modulo, height, width
  *(int *)(videobase+0x5c)=(((modulo<<10)+lines-1)<<10)+words_per_line-1;

  // Set vertical pos and border

  if(!(cabletype&2)) //VGA
    voffset += 36;
  else
    voffset += (hz50? 44 : 18);

#if 0    
  if(res==2 && pal)       // PAL Lace 
    laceoffset = 0;
  else if(res==2 && hz50) // NTSC Lace 50Hz (tested on EU,JP machine. strange)
    laceoffset = 0;
#endif

  vpos=(voffset<<16)|(voffset+laceoffset);
    
  *(int *)(videobase+0xf0)=vpos;	// V start
  *(int *)(videobase+0xdc)=vpos+lines;	// start and end border
  *(int *)(videobase+0xec)=hpos;	// Horizontal pos
  *(int *)(videobase+0xd8)=hvcounter;	// HV counter
  *(int *)(videobase+0xd4)=hborder;	// Horizontal border
  if(res==0)
    attribs=((22<<8)+1)<<8;		//X-way pixel doubler
  else
    attribs=22<<16;
  *(int *)(videobase+0xe8)=attribs;	// Screen attributes

#if 0
  if(!(cabletype&2))
    *(int *)(videobase+0xe0)=(0x0f<<22)|(793<<12)|(3<<8)|0x3f;
  else {
    attribs = 0x3f;
    if(hz50)
      attribs |= 5<<8;
    else
      attribs |= (res==2? 6:3)<<8;
    if(hz50)
      attribs |= (res==2? 362:799)<<12;
    else
      attribs |= (res==2? 364:793)<<12;
    attribs |= 0x1f<<22;
    *(int *)(videobase+0xe0)=attribs;
  }
#endif

  vpos = (hz50? 310:260);
  if(!(cabletype&2))
    vpos = 510;

  /* Set up vertical blank event */
  vpos = 242+voffset;
  if(!(cabletype&2))
    vpos = 482+voffset;
  *(int *)(videobase+0xcc)=((voffset-2)<<16)|(voffset+lines+2);

  // Select RGB/CVBS
  if(cabletype&1) //!rgbmode
    tmp=3;
  else
    tmp=0;
  tmp=tmp<<8;
  *(int *)cvbsbase = tmp;  

  *(int *)(videobase+0x44)=mode;// Set bpp
  *(int *)(videobase+0xd0)=videoflags;	//video enable, 60Hz, NTSC, lace

  *(volatile unsigned int *)(void*)0xa05f6900 = 0x08;

  return;
}


void video_clrscr(unsigned short c)
{
  short *p = (void*)0xa5000000;
  int i;
  for(i=0; i<640*480; i++)
    *p++ = c;
}



#define SCREENADDR(y,x) \
	(((unsigned short *)0xa5000000)+640*(y)+(x))
#define NEXTLINE(a) ((a)+640)
#define FONTADDR(b,n,w) (((unsigned char *)(b))+(n)*((w)*3))

#define PLOT \
	do {			\
	  s[640] = fg;		\
	  s[1] = fg;		\
	  *s++ = fg;		\
	} while(0)

static void drawhalf(int y, int x, int n, unsigned short fg)
{
  unsigned short *s = SCREENADDR(y, x);
  unsigned char *f = FONTADDR(halffontbase, n, 12);
  int i, j;
  asm("pref @%0" : : "r" (f));
  asm("pref @%0" : : "r" (f+32));
  for(i=0; i<12; i++) {
    int d = (*f++)<<16;
    d |= (*f++)<<8;
    d |= *f++;
    for(j=0; j<12; j++) {
      if(d&0x800000)
	PLOT;
      else
	s++;
      d<<=1;
    }
    s = NEXTLINE(s-12);
    for(j=0; j<12; j++) {
      if(d&0x800000)
	PLOT;
      else
	s++;
      d<<=1;
    }
    s = NEXTLINE(s-12);
  }
}

static void drawfull(int y, int x, int n, unsigned short fg)
{
  unsigned short *s = SCREENADDR(y, x);
  unsigned char *f = FONTADDR(fullfontbase, n, 24);
  int i, j;
  asm("pref @%0" : : "r" (f));
  asm("pref @%0" : : "r" (f+32));
  asm("pref @%0" : : "r" (f+64));
  asm("pref @%0" : : "r" (f+68));
  for(i=0; i<24; i++) {
    int d = (*f++)<<16;
    d |= (*f++)<<8;
    d |= *f++;
    for(j=0; j<24; j++) {
      if(d&0x800000)
	PLOT;
      else
	s++;
      d<<=1;
    }
    s = NEXTLINE(s-24);
  }
}

static int kuten(int sj)
{
  unsigned char lo = sj;
  unsigned char hi = ((unsigned int)sj)>>8;
  if(sj<0x8140 || sj>0xeaa4 || lo<0x40 || lo>0xfc || lo==0x7f)
    return -1;
  if(sj>=0xa000 && sj<0xe000)
    return -1;
  hi = ((hi-0x81)*2+0x21)&0x7f;
  if(lo>=0x9f) {
    hi++;
    lo -= 0x9f;
  } else if(lo>0x7f) {
    lo -= 0x41;
  } else
    lo -= 0x40;
  return (hi<40? (hi-33)*94+lo : (hi>=48? (hi-41)*94+lo : -1));
}

void video_plot_char(int x, int y, int ch, unsigned short colour)
{
  if(ch>=0x100)
    if((ch&0xff9f)<=0x115)
      drawfull(y, x, (ch&0x1f)+(7078-22), colour);
    else if((ch=kuten(ch))>=0)
      drawfull(y, x, ch, colour);
    else
      drawfull(y, x, 0, colour);
  else if(ch>=0xa0)
    drawhalf(y, x, ch+(1*96-0xa0), colour);
  else if(ch>32 && ch<127)
    drawhalf(y, x, ch-32, colour);
  else
    drawhalf(y, x, 288, colour);
}

void video_fill_bar(int x, int y, int w, int h, unsigned short c)
{
  unsigned short * p = SCREENADDR(y, x);
  while(h--) {
    int i;
    for(i=0; i<w; i++)
      *p++ = c;
    p += 640-w;
  }
}

void video_clear_area(int x, int y, int w, int h)
{
  video_fill_bar(x, y, w, h, C_KHAKI);
}
