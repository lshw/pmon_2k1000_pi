#include <linux/types.h>
#include <types.h>
#include <sys/param.h>
#include <stdbool.h>
#include "target/bonito.h"
#include "sys/dev/pci/pcireg.h"
#include <dev/pci/pcivar.h>
#include <dev/pci/nppbreg.h>

#define CKSEG1ADDR(x) (x|0xa0000000)

typedef unsigned long device_t;

#define HT_CONF_TYPE0_ADDR 0x90000efdfe000000
#define HT_MAP_TYPE0_CONF_ADDR  BONITO_PCICFG0_BASE_VA

//typedef unsigned long long u64;
extern struct pci_device *_pci_bus[16];
extern int _max_pci_bus;

static int is_pcie_root_port(int bus)
{
	int i, exp;
	struct pci_device *pd;

	if (bus == 0)
		return 0;
	for (i = 1; i < _max_pci_bus; i++) {
		pd = _pci_bus[i];
		if (!pd)
			break;
		if (pd->bridge.pribus_num == 0 && pd->bridge.secbus_num == bus)
			return 1;
	}
	return 0;
}

//typedef unsigned long long u64;
u32 pci_read_type0_config32(u32 dev, u32 func, u32 reg){
	u32 data;
	//u64 addr = HT_CONF_TYPE0_ADDR;
	u32 addr = HT_MAP_TYPE0_CONF_ADDR;
 
	addr |= (dev << 11 | func << 8 | reg);

	data = *((u32 *) addr);
	return data;
}

void pci_write_type0_config32(u32 dev, u32 func, u32 reg, u32 val){
	//u64 addr = HT_CONF_TYPE0_ADDR;
	u32 addr = HT_MAP_TYPE0_CONF_ADDR;
    
	addr |= (dev << 11 | func << 8 | reg);

	*((u32 *) addr) = val;
}


#define HT_CONF_TYPE1_ADDR 0x90000efdff000000
#define HT_MAP_TYPE1_CONF_ADDR  BONITO_PCICFG1_BASE_VA

u32 pci_read_type1_config32(u32 bus, u32 dev, u32 func, u32 reg){
//	u64 addr = HT_CONF_TYPE1_ADDR;
	u32 addr = HT_MAP_TYPE1_CONF_ADDR;

	addr |= (bus << 16 | dev << 11 | func << 8 | reg);

	return *((u32 *) addr);
}

void pci_write_type1_config32(u32 bus, u32 dev, u32 func, u32 reg, u32 val){
//	u64 addr = HT_CONF_TYPE1_ADDR;
	u32 addr = HT_MAP_TYPE1_CONF_ADDR;

	addr |= (bus << 16 | dev << 11 | func << 8 | reg);

	*((u32 *) addr) = val;
}

u32 _pci_conf_readn(device_t tag, int reg, int width)
{
	int bus, device, function;
    u32 val_raw;

	if ((width != 4) || (reg & 3) || reg < 0 || reg >= 0x100) {
		printf("_pci_conf_readn: bad reg 0x%x, tag 0x%x, width 0x%x\n", reg, tag, width);
		return ~0;
	}

	_pci_break_tag (tag, &bus, &device, &function); 
	
	if (is_pcie_root_port(bus) && device > 0)
		return ~0;		/* device out of range */

	if (bus > 255 || device > 31 || function > 7)
	{
		printf("_pci_conf_readn: bad bus 0x%x, device 0x%x, function 0x%x\n", bus, device, function);
		return ~0;		/* device out of range */
	}

	//workaround pcie header
	if(bus == 0 && (device >=9 && device <= 20) && reg == 0x8) {
		return 0x06040001;
	}
	//workaround LPC BAR4/5
	if(bus == 0 && device == 23 && function == 0 && (reg >= 0x10 && reg <= 0x24)){
		return 0;
	}

	if (bus == 0) {
		if (reg == 0x78) {
			val_raw = pci_read_type0_config32(device, function, reg);
			if (ls7a_version ()) {
				unsigned int tmp = val_raw;
				val_raw &= ~0x7000;
				/*set brige mrrs to 128*/
				if (device == 9 || device == 10 || device == 14 || device == 16 || device == 18 || device == 20)
					val_raw |= 0x1000; //256
				else if (device == 11 || device == 12)
					val_raw |= 0x5000; //4k
				else if (device == 13 || device == 15 || device == 17 || device == 19)
					val_raw |= 0x3000; //1k
				else
					return tmp;
				//printf("val_raw 0x%x \n",val_raw);
			} else {
				val_raw &= ~0x7000;
				/*set mrrs same with mps so mps must be setted*/
				val_raw |= (val_raw & 0xe0) << 7;
				//printf("val_raw 0x%x \n",val_raw);
			}
			return val_raw;
		}
		/* Type 0 configuration on onboard PCI bus */
		return pci_read_type0_config32(device, function, reg);
	} else {
		/* Type 1 configuration on offboard PCI bus */
		val_raw = pci_read_type1_config32(bus, device, function, reg);
		if(reg == 0 && val_raw == 0)
		    return -1;
		else
		    return val_raw;
	}
        
	return 0;
}


u32 _pci_conf_read(device_t tag,  int reg)
{
	return _pci_conf_readn(tag,reg,4);
}

void _pci_conf_write(device_t tag, int reg, u32 data)
{
    return _pci_conf_writen(tag,reg,data,4);
}

void
_pci_conf_write8(device_t tag, int reg, u8 data)
{
	return _pci_conf_writen(tag,reg,data,1);
}

void _pci_conf_writen(device_t tag, int reg, u32 data,int width)
{
	int bus, device, function;
	u32 ori;
	u32 mask = 0x0;

	if ((reg & (width -1)) || reg < 0 || reg >= 0x100) {
		printf("_pci_conf_writen: bad reg 0x%x, tag 0x%x, width 0x%x\n", reg, tag, width);
		return;
	}

	_pci_break_tag (tag, &bus, &device, &function);

	if (bus == 0) {
 		/* Type 0 configuration on onboard PCI bus */
		if (device > 31 || function > 7)
		{	
			printf("_pci_conf_writen: bad device 0x%x, function 0x%x\n", device, function);
			return;		/* device out of range */
		}
	} else {
	/* Type 1 configuration on offboard PCI bus */
		if (is_pcie_root_port(bus) && device > 0)
			return ;		/* device out of range */

		if (bus > 255 || device > 31 || function > 7)
		{	
			printf("_pci_conf_writen: bad bus 0x%x, device 0x%x, function 0x%x\n", bus, device, function);
			return;		/* device out of range */
		}
	}

	ori = _pci_conf_read(tag, reg & 0xfc);
	if(width == 2){
		if(reg & 2){
			mask = 0xffff;
		} else {
			mask = 0xffff0000;
		}
	}
	else if(width == 1){
		if ((reg & 3) == 1) {
			mask = 0xffff00ff;
		}else if ((reg & 3) == 2) {
			mask = 0xff00ffff;
		}else if ((reg & 3) == 3) {
			mask = 0x00ffffff;
		} else {
			mask = 0xffffff00;
		}
	}

	data = data << ((reg & 3) * 8);
	data = (ori & mask) | data;

	if (bus == 0) {
		return pci_write_type0_config32(device, function, reg & 0xfc, data);
	} else {
		return pci_write_type1_config32(bus, device, function, reg & 0xfc, data);
	}
}
