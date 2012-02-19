int video_check_cable(int *tv);
void video_init_pvr(void);
void video_init_video(int cabletype, int mode, int tvmode, int res,
		      int hz50, int pal, int voffset);
void video_clrscr(unsigned short c);
void video_plot_char(int x, int y, int ch, unsigned short colour);
void video_clear_area(int x, int y, int w, int h);
void video_fill_bar(int x, int y, int w, int h, unsigned short c);

#define C_RED   (31 << 11)
#define C_GREEN (63 << 5)
#define C_BLUE  (31 << 0)
#define C_MAGENTA (C_RED | C_BLUE)
#define C_YELLOW (C_RED | C_GREEN)
#define C_ORANGE (C_RED | (31<<5))
#define C_WHITE 65535
#define C_BLACK 0

#define C_KHAKI (30<<11 | (56<<5) | 17)

#define C_GREY   (15<<11 | 31<<5 | 15 )

