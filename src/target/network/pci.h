int pci_setup(void);
void pci_teardown(void);
unsigned int pci_read32(int reg);
unsigned short pci_read16(int reg);
unsigned char pci_read8(int reg);
void pci_write32(int reg, unsigned int val);
void pci_write16(int reg, unsigned short val);
void pci_write8(int reg, unsigned char val);





