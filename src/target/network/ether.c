#include "pci.h"
#include "ether.h"
#include "ip.h"
#include "arp.h"
#include "util.h"

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH 256	/* In bytes, rounded down to 32 byte units. */

/* The following settings are log_2(bytes)-4:  0 == 16 bytes .. 6==1024, 7==end of packet. */
#define RX_FIFO_THRESH	6	/* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN 16384

/* Maximum allowed input packet size (should be >= ethernet MTU) */
#define MAX_PKT_SIZE 2048

/* Number of Tx descriptor registers. */
#define NUM_TX_DESC	4

/* max supported ethernet frame size -- must be at least (dev->mtu+14+4).*/
#define MAX_ETH_FRAME_SIZE	1536

/* Symbolic offsets to registers. */
enum RTL8139_registers {
	MAC0 = 0,		/* Ethernet hardware address. */
	MAR0 = 8,		/* Multicast filter. */
	TxStatus0 = 0x10,	/* Transmit status (Four 32bit registers). */
	TxAddr0 = 0x20,		/* Tx descriptors (also four 32bit). */
	RxBuf = 0x30,
	RxEarlyCnt = 0x34,
	RxEarlyStatus = 0x36,
	ChipCmd = 0x37,
	RxBufPtr = 0x38,
	RxBufAddr = 0x3A,
	IntrMask = 0x3C,
	IntrStatus = 0x3E,
	TxConfig = 0x40,
	ChipVersion = 0x43,
	RxConfig = 0x44,
	Timer = 0x48,		/* A general-purpose counter. */
	RxMissed = 0x4C,	/* 24 bits valid, write clears. */
	Cfg9346 = 0x50,
	Config0 = 0x51,
	Config1 = 0x52,
	FlashReg = 0x54,
	MediaStatus = 0x58,
	Config3 = 0x59,
	Config4 = 0x5A,		/* absent on RTL-8139A */
	HltClk = 0x5B,
	MultiIntr = 0x5C,
	TxSummary = 0x60,
	BasicModeCtrl = 0x62,
	BasicModeStatus = 0x64,
	NWayAdvert = 0x66,
	NWayLPAR = 0x68,
	NWayExpansion = 0x6A,
	/* Undocumented registers, but required for proper operation. */
	FIFOTMS = 0x70,		/* FIFO Control and test. */
	CSCR = 0x74,		/* Chip Status and Configuration Register. */
	PARA78 = 0x78,
	PARA7c = 0x7c,		/* Magic transceiver parameter register. */
	Config5 = 0xD8,		/* absent on RTL-8139A */
};


enum ClearBitMasks {
	MultiIntrClear = 0xF000,
	ChipCmdClear = 0xE2,
	Config1Clear = (1<<7)|(1<<6)|(1<<3)|(1<<2)|(1<<1),
};

enum ChipCmdBits {
	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
	PCIErr = 0x8000,
	PCSTimeout = 0x4000,
	RxFIFOOver = 0x40,
	RxUnderrun = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,
};
enum TxStatusBits {
	TxHostOwns = 0x2000,
	TxUnderrun = 0x4000,
	TxStatOK = 0x8000,
	TxOutOfWindow = 0x20000000,
	TxAborted = 0x40000000,
	TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
	RxMulticast = 0x8000,
	RxPhysical = 0x4000,
	RxBroadcast = 0x2000,
	RxBadSymbol = 0x0020,
	RxRunt = 0x0010,
	RxTooLong = 0x0008,
	RxCRCErr = 0x0004,
	RxBadAlign = 0x0002,
	RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits {
	TxIFG1 = (1 << 25),	/* Interframe Gap Time */
	TxIFG0 = (1 << 24),	/* Enabling these bits violates IEEE 802.3 */
	TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
	TxCRC = (1 << 16),	/* DISABLE appending CRC to end of Tx packets */
	TxClearAbt = (1 << 0),	/* Clear abort (WO) */
	TxDMAShift = 8,		/* DMA burst value (0-7) is shift this many bits */

	TxVersionMask = 0x7C800000, /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum Config1Bits {
	Cfg1_PM_Enable = 0x01,
	Cfg1_VPD_Enable = 0x02,
	Cfg1_PIO = 0x04,
	Cfg1_MMIO = 0x08,
	Cfg1_LWAKE = 0x10,
	Cfg1_Driver_Load = 0x20,
	Cfg1_LED0 = 0x40,
	Cfg1_LED1 = 0x80,
};

enum RxConfigBits {
	/* Early Rx threshold, none or X/16 */
	RxCfgEarlyRxNone = 0,
	RxCfgEarlyRxShift = 24,

	/* rx fifo threshold */
	RxCfgFIFOShift = 13,
	RxCfgFIFONone = (7 << RxCfgFIFOShift),

	/* Max DMA burst */
	RxCfgDMAShift = 8,
	RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

	/* rx ring buffer length */
	RxCfgRcv8K = 0,
	RxCfgRcv16K = (1 << 11),
	RxCfgRcv32K = (1 << 12),
	RxCfgRcv64K = (1 << 11) | (1 << 12),

	/* Disable packet wrap at end of Rx buffer */
	RxNoWrap = (1 << 7),
};

enum Cfg9346Bits {
	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,
};


#define RTL_R8(reg) pci_read8(reg)
#define RTL_R16(reg) pci_read16(reg)
#define RTL_R32(reg) pci_read32(reg)
#define RTL_W8(reg,val8) pci_write8(reg,val8)
#define RTL_W16(reg,val16) pci_write16(reg,val16)
#define RTL_W32(reg,val32) pci_write32(reg,val32)
#define RTL_W8_F		RTL_W8
#define RTL_W16_F		RTL_W16
#define RTL_W32_F		RTL_W32


#define RXBASE 0x1840000
#define TXBASE 0x1844400


static int cur_rx;
static int curr_tx = 0;
static int tx_in_use = 0;
static char *rx_ring = (void *)(0xa0000000|RXBASE);


unsigned char ether_MAC[6];


static void ether_set_rx_mode(int up)
{
  unsigned int rx_mode;

  if(up)
    rx_mode =
      AcceptBroadcast | AcceptMyPhys;
  else
    rx_mode = 0;

  RTL_W32_F(RxConfig,   RxCfgEarlyRxNone | RxCfgRcv16K |
	    (RX_FIFO_THRESH << RxCfgFIFOShift) |
	    (RX_DMA_BURST << RxCfgDMAShift) |
	    rx_mode |
	    (RTL_R32 (RxConfig) & 0xf0fc0040));
  
  RTL_W32_F (MAR0 + 0, 0);
  RTL_W32_F (MAR0 + 4, 0);
}


static void ether_hw_start()
{
  int i;
  unsigned char tmp;
  union { unsigned int i[2]; unsigned char b[8]; } MAC;

  /* Soft reset the chip. */
  RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) | CmdReset);

  /* Check that the chip has finished the reset. */
  for (i = 1000000; i > 0; i--)
    if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
      break;

  /* unlock Config[01234] and BMCR register writes */
  RTL_W8_F (Cfg9346, Cfg9346_Unlock);

#if 0
  /* Restore our idea of the MAC address. */
  RTL_W32_F (MAC0 + 0, mac1);
  RTL_W32_F (MAC0 + 4, mac2);
#endif

  MAC.i[0] = RTL_R32 (MAC0 + 0);
  MAC.i[1] = RTL_R32 (MAC0 + 4);

  for (i = 0; i < 6; i++)
    ether_MAC[i] = MAC.b[i];

  /* Must enable Tx/Rx before setting transfer thresholds! */
  RTL_W8_F (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) |
	    CmdRxEnb | CmdTxEnb);

  ether_set_rx_mode(0);

  /* Check this value: the documentation for IFG contradicts ifself. */
  RTL_W32 (TxConfig, (TX_DMA_BURST << TxDMAShift));

  cur_rx = 0;

  tmp = RTL_R8 (Config1) & Config1Clear;
  tmp |= Cfg1_Driver_Load;
  RTL_W8_F (Config1, tmp);

  tmp = RTL_R8 (Config4) & ~(1<<2);
  /* chip will clear Rx FIFO overflow automatically */
  tmp |= (1<<7);
  RTL_W8 (Config4, tmp);

  /* Lock Config[01234] and BMCR register writes */
  RTL_W8_F (Cfg9346, Cfg9346_Lock);

  for (i = 10000; i > 0; i--);

  /* init Rx ring buffer DMA address */
  RTL_W32_F (RxBuf, RXBASE);

  /* init Tx buffer DMA addresses */
  for (i = 0; i < NUM_TX_DESC; i++)
    RTL_W32_F (TxAddr0 + (i * 4), TXBASE + i*2048);

  RTL_W32_F (RxMissed, 0);

  ether_set_rx_mode(1);

  /* no early-rx interrupts */
  RTL_W16 (MultiIntr, RTL_R16 (MultiIntr) & MultiIntrClear);

  /* make sure RxTx has started */
  RTL_W8_F (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) |
	    CmdRxEnb | CmdTxEnb);

#if 0
  /* Enable all known interrupts by setting the interrupt mask. */
  RTL_W16_F (IntrMask, 	PCIErr | PCSTimeout | RxUnderrun | RxOverflow |
	     RxFIFOOver | TxErr | TxOK | RxErr | RxOK);
#endif
}


int ether_setup()
{
  int i;

  curr_tx = 0;
  tx_in_use = 0;

  RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) | CmdReset);

  /* Check that the chip has finished the reset. */
  for (i = 1000000; i > 0; i--)
    if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
      break;

  /* handle RTL8139A and RTL8139 cases */
  /* XXX from becker driver. is this right?? */
  RTL_W8 (Config1, 0);

  /* simplex, negotiate speed */
  RTL_W16 (BasicModeCtrl, /*0x2100*/0x1200);

  ether_hw_start();

  return 0;
}

void ether_teardown()
{
  unsigned int txstatus;
  int i;

  for(i=0; i<NUM_TX_DESC; i++)

    if(tx_in_use & (1<<curr_tx)) {

      do {
	txstatus = RTL_R32 (TxStatus0 + 4*curr_tx);
      } while(!(txstatus & (TxStatOK | TxUnderrun | TxAborted)));

      tx_in_use &= ~(1<<curr_tx);
      curr_tx ++;

      if(curr_tx == NUM_TX_DESC)
	curr_tx = 0;
    }
}

static void ether_got_packet(void *pkt, int size)
{
  unsigned char *to_mac = pkt;
  unsigned char *from_mac = to_mac + 6;
  unsigned short ethertype = htons(*(unsigned short *)(from_mac + 6));

  pkt = from_mac + 8;
  size -= 14;

  switch(ethertype) {
   case 0x800: ip_got_packet(pkt, size); break;
   case 0x806: arp_got_packet(pkt, size); break;
  }

}

static void ether_read_events()
{
  static int tmp_buf[MAX_PKT_SIZE / sizeof(int)];
  int tmp_work = 1000;

  while ((RTL_R8 (ChipCmd) & RxBufEmpty) == 0) {
    int ring_offset = cur_rx % RX_BUF_LEN;
    unsigned int rx_status;
    unsigned int rx_size;
    unsigned int pkt_size;

    /* read size+status of next frame from DMA ring buffer */
    rx_status = (*(unsigned int *) (rx_ring + ring_offset));
    rx_size = rx_status >> 16;
    pkt_size = rx_size - 4;
    
    /* E. Gill */
    /* Note from BSD driver:
     * Here's a totally undocumented fact for you. When the
     * RealTek chip is in the process of copying a packet into
     * RAM for you, the length will be 0xfff0. If you spot a
     * packet header with this value, you need to stop. The
     * datasheet makes absolutely no mention of this and
     * RealTek should be shot for this.
     */
    if (rx_size == 0xfff0)
      break;
    
    /* If Rx err or invalid rx_size/rx_status received
     * (which happens if we get lost in the ring),
     * Rx process gets reset, so we abort any further
     * Rx processing.
     */
    if ((rx_size > (MAX_ETH_FRAME_SIZE+4)) ||
	(!(rx_status & RxStatusOK))) {

        unsigned char tmp8;

	tmp8 = RTL_R8 (ChipCmd) & ChipCmdClear;
	RTL_W8_F (ChipCmd, tmp8 | CmdTxEnb);

	/* A.C.: Reset the multicast list. */
	ether_set_rx_mode(1);

	/* XXX potentially temporary hack to
	 * restart hung receiver */
	while (--tmp_work > 0) {
		tmp8 = RTL_R8 (ChipCmd);
		if ((tmp8 & CmdRxEnb) && (tmp8 & CmdTxEnb))
			break;
		RTL_W8_F (ChipCmd,
			  (tmp8 & ChipCmdClear) | CmdRxEnb | CmdTxEnb);
	}

	/* G.S.: Re-enable receiver */
	/* XXX temporary hack to work around receiver hang */
	ether_set_rx_mode (1);

    } else if(pkt_size <= MAX_PKT_SIZE) {

      int i;

      for(i=0; i<pkt_size; i++)
	((char *)tmp_buf)[i] = rx_ring[(ring_offset + 4 + i) % RX_BUF_LEN];

      ether_got_packet(tmp_buf, pkt_size);
    
    }
    cur_rx = (cur_rx + rx_size + 4 + 3) & ~3;
    RTL_W16_F (RxBufPtr, cur_rx - 16);
  }
}

void ether_check_events()
{
  int status = RTL_R16 (IntrStatus);

  RTL_W16_F (IntrStatus, (status & RxFIFOOver) ?
	     (status | RxOverflow) : status);

  if ((status &
       (PCIErr | PCSTimeout | RxUnderrun | RxOverflow |
	RxFIFOOver | TxErr | TxOK | RxErr | RxOK)) == 0)
    return;

  if (status & (RxOK | RxUnderrun | RxOverflow | RxFIFOOver))
    ether_read_events();
}

void ether_send_packet(void *pkt, int size)
{
  unsigned int txstatus;

  char *buf = ((char*)(0xa0000000|TXBASE))+curr_tx*2048;

  if(tx_in_use & (1<<curr_tx)) {

    do {
      txstatus = RTL_R32 (TxStatus0 + 4*curr_tx);
    } while(!(txstatus & (TxStatOK | TxUnderrun | TxAborted)));

    tx_in_use &= ~(1<<curr_tx);
  }

  memcpy(buf, pkt, size);

  if(size < 60) {
    bzero(buf+size, 60-size);
    size = 60;
  }

  RTL_W32 (TxStatus0 + 4*curr_tx,
	   ((TX_FIFO_THRESH << 11) & 0x003f0000) | size);
  
  tx_in_use |= (1<<curr_tx);

  curr_tx++;

  if(curr_tx == NUM_TX_DESC)
    curr_tx = 0;

}
