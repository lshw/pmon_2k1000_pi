/*	$Id: tgt_machdep.c,v 1.1.1.1 2006/09/14 01:59:08 root Exp $ */

/*
 * Copyright (c) 2001 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <include/stdarg.h>
#include <include/stdio.h>
#include <include/file.h>
#include <linux/io.h>
#include <sys/ioccom.h>
#include <sys/types.h>
#include <termio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <dev/pci/pcivar.h>

#include <autoconf.h>
#include <pmon.h>
#include <machine/cpu.h>
#include <machine/pio.h>

#include <pmon/dev/ns16550.h>
#include "pflash.h"
#include "dev/pflash_tgt.h"

#include "target/ls7a.h"

#include "mod_x86emu_int10.h"
#include "mod_x86emu.h"
#include "mod_vgacon.h"
#include "mod_framebuffer.h"
#include "mod_smi712.h"
#include "mod_smi502.h"
#include "mod_sisfb.h"

/* Generic file-descriptor ioctl's. */
#define	FIOCLEX		 _IO('f', 1)		/* set close on exec on fd */
#define	FIONCLEX	 _IO('f', 2)		/* remove close on exec */
#define	FIONREAD	_IOR('f', 127, int)	/* get # bytes to read */
#define	FIONBIO		_IOW('f', 126, int)	/* set/clear non-blocking i/o */
#define	FIOASYNC	_IOW('f', 125, int)	/* set/clear async i/o */
#define	FIOSETOWN	_IOW('f', 124, int)	/* set owner */
#define	FIOGETOWN	_IOR('f', 123, int)	/* get owner */

#define STDIN		((kbd_available|usb_kbd_available)?3:0)

extern char * nvram_offs;

void tgt_putchar (int);
int tgt_printf (const char *fmt, ...)
{
    int  n;
    char buf[1024];
    char *p=buf;
    char c;
    va_list     ap;
    va_start(ap, fmt);
    n = vsprintf (buf, fmt, ap);
    va_end(ap);
    while((c=*p++)) { 
	    if(c=='\n')tgt_putchar('\r');
	    tgt_putchar(c);
    }
    return (n);
}

#if (NMOD_X86EMU_INT10 > 0)||(NMOD_X86EMU >0)
extern int vga_bios_init(void);
#endif
extern int radeon_init(void);
extern int kbd_initialize(void);
extern int write_at_cursor(char val);
extern void hda_codec_set(void);
extern const char *kbd_error_msgs[];
#include "flash.h"
#if (NMOD_FLASH_AMD + NMOD_FLASH_INTEL + NMOD_FLASH_SST) == 0
#ifdef HAVE_FLASH
#undef HAVE_FLASH
#endif
#else
#define HAVE_FLASH
#endif

#if (NMOD_X86EMU_INT10 == 0)&&(NMOD_X86EMU == 0)
int vga_available=0;
#elif defined(VGAROM_IN_BIOS)
#include "vgarom.c"
#endif

extern struct trapframe DBGREG;
extern void *memset(void *, int, size_t);
#ifdef MULTI_CHIP
extern int usb_spi_init(void);
#endif
extern void gmac_mac_init();
#ifdef SLT
extern void slt_test();
#endif
int kbd_available;
int bios_available;
int usb_kbd_available;;
int vga_available;
int cmd_main_mutex = 0;
int bios_mutex = 0;

static int md_pipefreq = 0;
static int md_cpufreq = 0;
static int clk_invalid = 0;
static int nvram_invalid = 0;
static int cksum(void *p, size_t s, int set);
static void _probe_frequencies(void);
static void ls7a_pwm(int x,int y);

extern int vgaterm(int op, struct DevEntry * dev, unsigned long param, int data);
extern int fbterm(int op, struct DevEntry * dev, unsigned long param, int data);
void error(unsigned long *adr, unsigned long good, unsigned long bad);
void modtst(int offset, int iter, unsigned long p1, unsigned long p2);
void do_tick(void);
void print_hdr(void);
void ad_err2(unsigned long *adr, unsigned long bad);
void ad_err1(unsigned long *adr1, unsigned long *adr2, unsigned long good, unsigned long bad);
void mv_error(unsigned long *adr, unsigned long good, unsigned long bad);

static void init_legacy_rtc(void);
unsigned long long __raw__writeq(unsigned long long val, unsigned long long data);

#ifdef INTERFACE_3A780E
#define REV_ROW_LINE 560
#define INF_ROW_LINE 576

int afxIsReturnToPmon = 0;

  struct FackTermDev
  {
      int dev;
  };

#endif

ConfigEntry	ConfigTable[] =
{
    { (char *)GS3_UART_BASE, 0, ns16550, 256, CONS_BAUD, NS16550HZ },
#if NMOD_VGACON >0
#if NMOD_FRAMEBUFFER >0
    { (char *)1, 0, fbterm, 256, CONS_BAUD, NS16550HZ },
#else
    { (char *)1, 0, vgaterm, 256, CONS_BAUD, NS16550HZ },
#endif
#endif
#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE && defined(USE_SUPERIO_UART)
    { (char *)-1, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 ,0}, 
    { (char *)-1, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 ,0}, 
#endif
    { 0 }
};

unsigned long _filebase;

extern unsigned long long  memorysize;
extern unsigned long long  memorysize_high;
#ifdef MULTI_CHIP
extern unsigned long long  memorysize_high_n1;
extern unsigned long long  memorysize_high_n2;
extern unsigned long long  memorysize_high_n3;
#endif
unsigned int superio_base;

extern char MipsException[], MipsExceptionEnd[];

unsigned char hwethadr[6];
static void superio_reinit();

unsigned long long str_ra, str_sp;
#ifdef LS3A7A_STR
#define STR_STORE_BASE 0xafaaa000

uint64_t mem_read64(unsigned long addr)
{
       unsigned char bytes[8];
       int i;

       for (i = 0; i < 8; i++)
               bytes[i] = *((unsigned char *)(STR_STORE_BASE + addr + i));

       return *(uint64_t *) bytes;
}

void mem_write64(uint64_t data, unsigned long addr)
{
       int i;
       unsigned char *bytes = (unsigned char *) &data;

       for (i = 0; i < 8; i++)
                *((unsigned char *)(STR_STORE_BASE + addr + i)) = bytes[i];
}

void check_str()
{
       uint64_t str_ra,str_flag, str_ra1,str_flag1;
       long long str_sp ,str_sp1;
       unsigned int sp_h,sp_l;

       str_ra = mem_read64(0x40);
       str_sp = mem_read64(0x48);
       str_flag = mem_read64(0x50);
       sp_h = str_sp >> 32;
       sp_l = str_sp;
       printf("SP=%llx, RA=%llx\n", str_sp, str_ra);
       printf("str_flag=%llx\n", str_flag);
       if ((str_sp < 0x9800000000000000) || (str_ra < 0xffffffff80000000)
                       || (str_flag != 0x5a5a5a5a5a5a5a5a)) {
               *(uint64_t *)(STR_STORE_BASE + 0x50) = 0x0; //clean str flag
               printf("not s3 exit %llx\n", str_flag);
               return;
       }

       printf("Start status S3....\n");
       mem_write64(0x0, 0x40);
       mem_write64(0x0, 0x48);
       mem_write64(0x0, 0x50);

       /* misc:0x1008,0000 -- 0x100f,ffff */
       /* acpi offset 0x50000 of misc */
       /* 0x50 of acpi is cmos reg which storage s3 flag, now clear this flag */
    *((unsigned int *)(0xb00d0050)) = 0x0;

       ls_pcie_interrupt_fixup();

       printf("CPU TLBClear....\n");
       CPU_TLBClear();
       printf("CPU TLBInit....\n");
       CPU_TLBInit();
       printf("CPU FlushCache....\n");
       CPU_FlushCache();

       printf("jump to kernel....\n");
       __asm__ __volatile__(
                       ".set   noat                    \n"
                       ".set   mips64                  \n"
                       "move   $t0, %0                 \n"
                       "move   $t1, %1                 \n"
                       "dli    $t2, 0x00000000ffffffff \n"
                       "and    $t1,$t2                 \n"
                       "dsll   $t0,32                  \n"
                       "or     $sp, $t0,$t1            \n"
                       "jr     %2                      \n"
                       "nop                            \n"
                       ".set   at                      \n"
                       : /* No outputs */
                       :"r"(sp_h), "r"(sp_l),"r"(str_ra)
                       );
}
#endif

extern char waitforinit[];
void initmips(unsigned int raw_memsz)
{
	//core1-3 run waitdorinit function in ram
#ifdef		MULTI_CHIP
	asm volatile(
	".set push;\n"
	".set noreorder\n"
        ".set mips64;\n"
	"dli $2,0x900010003ff01000;\n"
	"dli $3, 4;\n"
        "1:sd %0,0x20($2);"
	"daddiu $2,0x100;\n"
	"addiu $3,-1;\n"
	"bnez $3, 1b;\n"
	"nop;\n"
	".set reorder;\n"
        ".set mips0;\n"
	".set pop;\n"
         ::"r"(&waitforinit):"$2","$3");
#endif
	asm volatile(
	".set push;\n"
	".set noreorder\n"
        ".set mips64;\n"
	"dli $2,0x900000003ff01100;\n"
	"dli $3, 3;\n"
        "1:sd %0,0x20($2);"
	"daddiu $2,0x100;\n"
	"addiu $3,-1;\n"
	"bnez $3, 1b;\n"
	"nop;\n"
	".set reorder;\n"
        ".set mips0;\n"
	".set pop;\n"
         ::"r"(&waitforinit):"$2","$3");

	//disable 3a spi instruction fetch
	__raw__writeq(0x900000003ff00080ULL, 0x1fc00082ULL);

    tgt_fpuenable();

    get_memorysize(raw_memsz);

    /*
     *  Probe clock frequencys so delays will work properly.
     */
    ls7a_pwm(5000, 10000);
    tgt_cpufreq();
    SBD_DISPLAY("DONE",0);
    /*
     *  Init PMON and debug
     */
    cpuinfotab[0] = &DBGREG;
    dbginit(NULL);

    /*
     *  Set up exception vectors.
     */
    SBD_DISPLAY("BEV1",0);
    bcopy(MipsException, (char *)TLB_MISS_EXC_VEC, MipsExceptionEnd - MipsException);
    bcopy(MipsException, (char *)GEN_EXC_VEC, MipsExceptionEnd - MipsException);

    CPU_FlushCache();

#ifndef ROM_EXCEPTION
    CPU_SetSR(0, SR_BOOT_EXC_VEC);
#endif
    SBD_DISPLAY("BEV0",0);

    printf("BEV in SR set to zero.\n");
#ifdef DTB
	verify_dtb();
#endif

    main();
}

/*
 *  Put all machine dependent initialization here. This call
 *  is done after console has been initialized so it's safe
 *  to output configuration and debug information with printf.
 */

extern void	vt82c686_init(void);
int psaux_init(void);
extern int video_hw_init (void);
extern int fb_init(unsigned long,unsigned long);
extern int dc_init(void);

extern unsigned long long uma_memory_base;
extern unsigned long long uma_memory_size;

extern unsigned short ScreenLineLength;
extern unsigned short ScreenDepth;
extern unsigned short ScreenHeight;

#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE
static int w83627_read(int dev,int addr)
{
	int data;
	/*enter*/
	outb(superio_base + 0x002e,0x87);
	outb(superio_base + 0x002e,0x87);
	/*select logic dev reg */
	outb(superio_base + 0x002e,0x7);
	outb(superio_base + 0x002f,dev);
	/*access reg */
	outb(superio_base + 0x002e,addr);
	data=inb(superio_base + 0x002f);
	/*exit*/
	outb(superio_base + 0x002e,0xaa);
	outb(superio_base + 0x002e,0xaa);
	return data;
}

static void w83627_write(int dev,int addr,int data)
{
	/*enter*/
	outb(superio_base + 0x002e,0x87);
	outb(superio_base + 0x002e,0x87);
	/*select logic dev reg */
	outb(superio_base + 0x002e,0x7);
	outb(superio_base + 0x002f,dev);
	/*access reg */
	outb(superio_base + 0x002e,addr);
	outb(superio_base + 0x002f,data);
	/*exit*/
	outb(superio_base + 0x002e,0xaa);
	outb(superio_base + 0x002e,0xaa);
}
#define HM_OFF		0x180
#define INDEX_OFF	0X5
#define DATA_OFF	0X6

#ifndef SUPERIO_UART1_IRQPORT
#define SUPERIO_UART1_IRQPORT 0x0403f8
#endif

#ifndef SUPERIO_UART2_IRQPORT
#define SUPERIO_UART2_IRQPORT 0x0302f8
#endif

static void superio_reinit()
{
	/*set global  multifuntion pin reg 0x2c low 2bit to 3 for 83627dhg to enable uart2*/
	w83627_write(0,0x2c,0xe3);
#ifdef SUPERIO_CLOCK_24M
	//bit6=0 , clk=24M //dbg-yg 
	w83627_write(0,0x24,0x81);
#else
	w83627_write(0,0x24,0xc1);
#endif
	w83627_write(5,0x30,1);
	w83627_write(5,0x60,0);
	w83627_write(5,0x61,0x60);
	w83627_write(5,0x62,0);
	w83627_write(5,0x63,0x64);
	w83627_write(5,0x70,1);
	w83627_write(5,0x72,0xc);
	w83627_write(5,0xf0,0x80);
	//w83627_UART1
	w83627_write(2,0x30,0x01);
	w83627_write(2,0x60,(SUPERIO_UART1_IRQPORT>>8)&0xff);
	w83627_write(2,0x61,SUPERIO_UART1_IRQPORT&0xff);
	w83627_write(2,0x70,(SUPERIO_UART1_IRQPORT>>16)&0xff);
	w83627_write(2,0xf0,0x00);

	//w83627_UART2
	w83627_write(3,0x30,0x01);
	w83627_write(3,0x60,(SUPERIO_UART2_IRQPORT>>8)&0xff);
	w83627_write(3,0x61,SUPERIO_UART2_IRQPORT&0xff);
	w83627_write(3,0x70,(SUPERIO_UART2_IRQPORT>>16)&0xff);
	w83627_write(3,0xf0,0x00);
	////w83627_PALLPort
	w83627_write(1,0x30,0x01);
	w83627_write(1,0x60,0x03);
	w83627_write(1,0x61,0x78);
	w83627_write(1,0x70,0x01);
	w83627_write(1,0x74,0x04);
	w83627_write(1,0xf0,0xF0); 

	/* enable HM  */
	w83627_write(0xb,0x30,0x1);
	/* add support for fan speed controler */
	w83627_write(0xb,0x60,HM_OFF >> 0x8); // set HM base address @0xb8000180,0x290 with some error
	w83627_write(0xb,0x61,HM_OFF & 0xff);
}
#endif

void tgt_devconfig()
{
	int ic, len;
    int count = 0;
    char bootup[] = "Booting...";
    char * s;
#if NMOD_VGACON > 0
    int rc=1;
#if NMOD_FRAMEBUFFER > 0 
    unsigned long fbaddress,ioaddress;
    extern struct pci_device *pcie_dev;
#endif
#endif
	_pci_devinit(1);	/* PCI device initialization */

#ifndef MULTI_CHIP
    usb_spi_init();
#endif

#if (NMOD_X86EMU_INT10 > 0)||(NMOD_X86EMU >0)
	if(pcie_dev != NULL){
		SBD_DISPLAY("VGAI", 0);
		rc = vga_bios_init();
		printf("rc=%d\n", rc);
	}
#endif

#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE
    /* get dc base addr from pci config space */
	superio_base = 0xb8000000;
	superio_reinit();
#elif defined(USE_LS3A_LPC)
	superio_base = 0xbff00000;
	superio_reinit();
#endif
#ifdef USE_SUPERIO_UART
	{int i;
	 int j;
	 int uart[] = { superio_base  + 0x3f8, superio_base + 0x2f8 };

		for(i=0,j=0;ConfigTable[i].devinfo;i++)
		{
			if(ConfigTable[i].devinfo==-1)
			{
				DevTable[i].sio=uart[j++];
				ns16550(OP_BAUD, &DevTable[i], NULL, CONS_BAUD);
			}
		}
	}
#endif
#if defined(VESAFB)
	SBD_DISPLAY("VESA", 0);
	if(rc > 0)
		vesafb_init();
#endif
#if NMOD_FRAMEBUFFER > 0
	vga_available = 0;
	if (rc > 0) {
		if(pcie_dev == NULL){
			printf("begin dc_init\n");
			fbaddress = dc_init();
			//this parameters for 800*600 VGA
			ScreenLineLength = 1600;
			ScreenDepth = 16;
			ScreenHeight = 600;
#if 0
			//this parameters for 1920*1080 VGA
			ScreenLineLength = 3840;
			ScreenDepth = 16;
			ScreenHeight = 1080;
#endif
		} else {
			fbaddress  = _pci_conf_read(pcie_dev->pa.pa_tag,0x10);
			fbaddress = fbaddress &0xffffff00; //laster 8 bit
			fbaddress |= 0x80000000;
		}
		printf("fbaddress = %08x\n", fbaddress);

		fb_init(fbaddress, ioaddress);//ioaddress is unuseful
		printf("fb_init done\n");
#ifndef MULTI_CHIP
		/* For dvo1 */
#ifdef USE_CH7034
		gpioi2c_config_ch7034();
#endif
		/* For dvo0 */
#ifdef USE_SII9022a
		gpioi2c_config_sii9022a();
#endif

#ifdef USE_ADV7511
		dvo_hdmi_init();
#endif
#endif
	} else {
		printf("vga bios init failed, rc=%d\n",rc);
	}
#endif

#if (NMOD_FRAMEBUFFER > 0) || (NMOD_VGACON > 0 )
	if (rc > 0)
		if(!getenv("novga"))
			vga_available = 1;
		else
			vga_available = 0;
	printf("vga available : %d\n", vga_available);
#endif

#ifdef CPU_GPIO_SET
	*(volatile int *)0xbfe00120 &= ~CPU_GPIO_SET;
	*(volatile int *)0xbfe0011c |= CPU_GPIO_SET;
#endif
	config_init();
	configure();
	gmac_mac_init();

#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE
	if(getenv("nokbd"))
		rc=1;
	else {
		rc=kbd_initialize();
	}
	printf("%s\n",kbd_error_msgs[rc]);
	if(!rc){
		kbd_available=1;
	}
#endif
#ifdef INTERFACE_3A780E 

	vga_available = 1;
    bios_available = 1; //support usb_kbd in bios
	// Ask user whether to set bios menu
    printf("Press <Del> to set BIOS,waiting for 3 seconds here..... \n");

	video_set_color(0xf);

	init_win_device();

	vga_available = 0;          //lwg close printf output

	{
       	struct FackTermDev *devp;
        int fd = 0;
        DevEntry* p;
        int sign = 0;

        //get input without wait ===========   add by oldtai
        fd = ((FILE*)stdin)->fd;
        devp = (struct FackTermDev *)(_file[fd].data);
        p = &DevTable[devp->dev];

		for (count = 0;count < 10;count++)
		{
			//get input without wait
			scandevs();
            while(!tgt_smplock());

            /* 'DEL' to BIOS Interface */
            if(p->rxq->count >= 3){
				if ((p->rxq->dat[p->rxq->first + p->rxq->count-3] == 0x1b)
                          		&& (p->rxq->dat[p->rxq->first + p->rxq->count-2] == '[')
					&& (p->rxq->dat[p->rxq->first + p->rxq->count-1] == 'G')) {
                        		sign = 1;
                        		p->rxq->count -= 3;
				}
				if (sign == 1) {
					tgt_smpunlock();
					break;
				}
			}
            tgt_smpunlock();

            /*If you want to Press <Del> to set BIOS open it(from 0 to 1)*/
#if 1
			delay1(300);
#endif
		}

		vga_available = 1;
		video_set_color(0xf);

		for (ic = 0; ic < 64; ic++)
		{
         		video_putchar1(2 + ic*8, REV_ROW_LINE, ' ');
                	video_putchar1(2 + ic*8, INF_ROW_LINE, ' ');
		}

		vga_available = 0;

        if (count >= 10)
        	goto run;
        else
		    goto bios;

bios:

		if(!(s = getenv("SHOW_DISPLAY")) || s[0] !='2')
		{
			video_set_color(0xf);
            video_set_color(0x8);
            tty_flush();
			vga_available = 1;
            do_cmd("main");
            if (!afxIsReturnToPmon)
            {
            	vga_available = 0;
            }
        }

run:
		vga_available = 1;
		bios_available = 0;//support usb_kbd in bios

		len = strlen(bootup);
		for (ic = 0; ic < len; ic++)
		{
			video_putchar1(2 + ic*8, INF_ROW_LINE,bootup[ic]);
		}
	}
#endif
	printf("devconfig done.\n");
	clear_pcie_inter_irq();
	ls_pcie_interrupt_fixup();
	hda_codec_set();
#ifdef SLT
	slt_test();
#endif
}
static int w83627_read(int dev,int addr)
{
	int data;
	/*enter*/
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x87);
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x87);
	/*select logic dev reg */
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x7);
	outb(BONITO_PCIIO_BASE_VA + 0x002f,dev);
	/*access reg */
	outb(BONITO_PCIIO_BASE_VA + 0x002e,addr);
	data=inb(BONITO_PCIIO_BASE_VA + 0x002f);
	/*exit*/
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0xaa);
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0xaa);
	return data;
}

static void w83627_write(int dev,int addr,int data)
{
	/*enter*/
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x87);
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x87);
	/*select logic dev reg */
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0x7);
	outb(BONITO_PCIIO_BASE_VA + 0x002f,dev);
	/*access reg */
	outb(BONITO_PCIIO_BASE_VA + 0x002e,addr);
	outb(BONITO_PCIIO_BASE_VA + 0x002f,data);
	/*exit*/
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0xaa);
	outb(BONITO_PCIIO_BASE_VA + 0x002e,0xaa);
}
static void superio_reinit()
{
	w83627_write(0,0x24,0xc1);
	w83627_write(5,0x30,1);
	w83627_write(5,0x60,0);
	w83627_write(5,0x61,0x60);
	w83627_write(5,0x62,0);
	w83627_write(5,0x63,0x64);
	w83627_write(5,0x70,1);
	w83627_write(5,0x72,0xc);
	w83627_write(5,0xf0,0x80);
}

void tgt_devinit()
{

	/*
	 *  Gather info about and configure caches.
	 */
	if(getenv("ocache_off")) {
		CpuOnboardCacheOn = 0;
	}
	else {
		CpuOnboardCacheOn = 1;
	}
	if(getenv("ecache_off")) {
		CpuExternalCacheOn = 0;
	}
	else {
		CpuExternalCacheOn = 1;
	}

	CPU_ConfigCache();

	_pci_businit(1);	/* PCI bus initialization */
}

void tgt_poweroff()
{
	readl(LS7A_ACPI_PM1_STS_REG) &= 0xffffffff;
	readl(LS7A_ACPI_PM1_CNT_REG) = 0x3c00;
}

static int ls7a_rtc_wakeup_reboot(int delay);

void tgt_reboot(void)
{
#ifdef LS7A_RTC_WAKEUP_REBOOT
	init_legacy_rtc();
	ls7a_rtc_wakeup_reboot(LS7A_RTC_WAKEUP_REBOOT);
#else
	readl(LS7A_ACPI_RST_CNT_REG) = 1;
#endif
}

#ifdef LS7A_RTC_WAKEUP_REBOOT
static int acpi_suspend()
{
#define GPE0_EN  (LS7A_ACPI_REG_BASE+0x2c)
#define GPE0_STS (LS7A_ACPI_REG_BASE+0x28)
#define PM1_STS  (LS7A_ACPI_REG_BASE+0xc)
#define PM1_CNT  (LS7A_ACPI_REG_BASE+0x14)
#define PM1_EN  (LS7A_ACPI_REG_BASE+0x10)
#define PMCON_RESUME  (LS7A_ACPI_REG_BASE+0x4)

readl(GPE0_EN) = 0xfd70;
readl(GPE0_STS) = 0x0000ffff;
readl(PM1_STS) = 0x0000ffff;
readl(PMCON_RESUME) |= 0x2880;
readl(PM1_EN) |= 0x400;
readl(PM1_CNT) = 0x00003400;
}

static int ls7a_rtc_alarmset(struct tm *tm)
{
	int rtc_reg = 0xb00d0100;
	int c = readl(rtc_reg+0x40);
	if ((c&0x2900)!=0x2900) readl(rtc_reg+0x40) = 0x2900;

	readl(rtc_reg+0x34) = ((tm->tm_year&0x3f)<<26)|((tm->tm_mon + 1)<<22)|(tm->tm_mday<<17) \
			      |(tm->tm_hour<<12)|(tm->tm_min<<6)|(tm->tm_sec<<0);
	return 0;
}

static int ls7a_rtc_wakeup_reboot(int delay)
{
	struct tm *tm;
	time_t t = tgt_gettime();
	t += delay;
	tm = gmtime(&t);
	ls7a_rtc_alarmset(tm);
	acpi_suspend();
	return 0;
}
#endif

/*
 *  This function makes inital HW setup for debugger and
 *  returns the apropriate setting for the status register.
 */
register_t tgt_enable(int machtype)
{
	/* XXX Do any HW specific setup */
	return(SR_COP_1_BIT|SR_FR_32|SR_EXL);
}


/*
 *  Target dependent version printout.
 *  Printout available target version information.
 */

void tgt_cmd_vers()
{
}

/*
 *  Display any target specific logo.
 */
void tgt_logo()
{
    printf("\n");
    printf("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\n");
    printf("[[  [[[[[[[[[       [[[[[       [[[[   [[[[[  [[[[[      [[[[[       [[[[[       [[[[   [[[[[  [[\n");
    printf("[[  [[[[[[[[   [[[[  [[[   [[[[  [[[    [[[[  [[[[  [[[[  [[[   [[[[  [[[   [[[[  [[[    [[[[  [[\n");
    printf("[[  [[[[[[[[  [[[[[[ [[[  [[[[[[ [[[  [  [[[  [[[  [[[[[[[[[[[[   [[[[[[[  [[[[[[ [[[  [  [[[  [[\n");
    printf("[[  [[[[[[[[  [[[[[[ [[[  [[[[[[ [[[  [[  [[  [[[  [[[    [[[[[[[    [[[[  [[[[[[ [[[  [[  [[  [[\n");
    printf("[[  [[[[[[[[  [[[[[[ [[[  [[[[[[ [[[  [[[  [  [[[  [[[[[  [[[[[[[[[[  [[[  [[[[[[ [[[  [[[  [  [[\n");
    printf("[[  [[[[[[[[   [[[[  [[[   [[[[  [[[  [[[[    [[[   [[[[  [[[   [[[  [[[[   [[[[  [[[  [[[[    [[\n");
    printf("[[       [[[[       [[[[[       [[[[  [[[[[   [[[[       [[[[[      [[[[[[       [[[[  [[[[[   [[\n");
    printf("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[2011 Loongson][[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\n");
 
}

static void ls7a_pwm(int x,int y)
{
    readl(LS7A_PWM0_CTRL) &= ~1;
    outl(LS7A_PWM0_LOW,x);
    outl(LS7A_PWM0_FULL,y);
    readl(LS7A_PWM0_CTRL) |= 1;

    readl(LS7A_PWM1_CTRL) &= ~1;
    outl(LS7A_PWM1_LOW,x);
    outl(LS7A_PWM1_FULL,y);
    readl(LS7A_PWM1_CTRL) |= 1;

    //as now, the 7A Fan control circuit(PWM2) has problem, keep it constant to avoid wearing fan.
    //readl(LS7A_PWM2_CTRL) &= ~1;
    //outl(LS7A_PWM2_LOW,x);
    //outl(LS7A_PWM2_FULL,y);
    //readl(LS7A_PWM2_CTRL) |= 1;

    readl(LS7A_PWM3_CTRL) &= ~1;
    outl(LS7A_PWM3_LOW,x);
    outl(LS7A_PWM3_FULL,y);
    readl(LS7A_PWM3_CTRL) |= 1;
}

static void init_legacy_rtc(void)
{
	int year, month, date, hour, min, sec, val;
	val = (1 << 13) | (1 << 11) | (1 << 8);
	
	outl(LS7A_RTC_CTRL_REG, val);
	outl(LS7A_TOY_TRIM_REG, 0);
	outl(LS7A_RTC_TRIM_REG, 0);

	val = inl(LS7A_TOY_READ0_REG);

	year = inl(LS7A_TOY_READ1_REG);
	month = (val >> 26) & 0x3f;
	date = (val >> 21) & 0x1f; 
	hour = (val >> 16) & 0x1f; 
	min = (val >> 10) & 0x3f; 
	sec = (val >> 4) & 0x3f; 
	if ((year < 0 || year > 138)
		|| (month < 1 || month > 12)
		|| (date < 1 || date > 31)
		|| (hour > 23) || (min > 59)
		|| (sec > 59)) {

		tgt_printf("RTC time invalid, reset to epoch.\n");
		/* 2000-01-01 00:00:00 */
		val = (1 << 26) | (1 << 21);
		outl(LS7A_TOY_WRITE1_REG, 0x64);
		outl(LS7A_TOY_WRITE0_REG, val);
	}
}
static void _probe_frequencies()
{
#ifdef HAVE_TOD
	int i, timeout, cur, sec, cnt, sec1;
#endif

	SBD_DISPLAY ("FREQ", CHKPNT_FREQ);

	md_pipefreq = 660000000;
	md_cpufreq = 60000000;

	clk_invalid = 1;
#ifdef HAVE_TOD
	init_legacy_rtc();

	SBD_DISPLAY ("FREI", CHKPNT_FREQ);

#ifdef USE_RTC_COUNTER
	/*
	 * Do the next twice for two reasons. First make sure we run from
	 * cache. Second make sure synched on second update. (Pun intended!)
	 */
	for (i = 2; i != 0; i--) {
		timeout = 10000000;
		sec = (inl(LS7A_TOY_READ0_REG) >> 4) & 0x3f;
		sec1 = (sec+1)%60;
		do {
			cur = ((inl(LS7A_TOY_READ0_REG) >> 4) & 0x3f);
		} while (cur != sec1);
		sec1 = (cur+1)%60;

		cnt = CPU_GetCOUNT();
		do {
			timeout--;
			sec = (inl(LS7A_TOY_READ0_REG) >> 4) & 0x3f;
		} while (timeout != 0 && (sec1 != sec));
		cnt = CPU_GetCOUNT() - cnt;
		if (timeout == 0) {
			tgt_printf("time out!\n");
			break;	/* Get out if clock is not running */
		}
	}

	/*
	 *  Calculate the external bus clock frequency.
	 */
	if (timeout != 0) {
		clk_invalid = 0;
		md_pipefreq = cnt / 10000;
		md_pipefreq *= 20000;
		/* we have no simple way to read multiplier value
		 */
		md_cpufreq = 66000000;
	}
	tgt_printf("cpu freq %u\n", md_pipefreq);

#else
/*
 //whd: use to check the read delay
		cnt = CPU_GetCOUNT();
	for(i = 0; i != 100; i++) {
		inl(LS7A_TOY_READ0_REG);
	}
		cnt = CPU_GetCOUNT() - cnt;

	tgt_printf("100 read RTC delay %u\n", cnt);

		cnt = CPU_GetCOUNT();
	for(i = 0; i != 100; i++) {
		inl(0xBA000000);
	}
		cnt = CPU_GetCOUNT() - cnt;

	tgt_printf("100 read HT header delay %u\n", cnt);

		cnt = CPU_GetCOUNT();
	for(i = 0; i != 100; i++) {
		inl(0xBA001000);
	}
		cnt = CPU_GetCOUNT() - cnt;

	tgt_printf("100 read RTC header delay %u\n", cnt);

		cnt = CPU_GetCOUNT();
	for(i = 0; i != 100; i++) {
		inl(0xB00A0004);
	}
		cnt = CPU_GetCOUNT() - cnt;

	tgt_printf("100 read PWM header delay %u\n", cnt);

		cnt = CPU_GetCOUNT();
	for(i = 0; i != 100; i++) {
		inl(0xB00010f0);
	}
		cnt = CPU_GetCOUNT() - cnt;

	tgt_printf("100 read HPET header delay %u\n", cnt);
*/

/* whd : USE HPET to calculate the frequency, 
 *       reduce the booting delay and improve the frequency accuracy. 
 *       when use the RTC counter of 7A, it cost 160us+ for one read, 
 *       but if we use the HPET counter, it only cost ~300ns for one read,
 *       so the HPET has good accuracy even use less time */

	outl(LS7A_HPET_CONF, 0x1);//Enable main clock

	/*
	 * Do the next twice to make sure we run from cache
	 */
	for (i = 2; i != 0; i--) {
		timeout = 10000000;

		sec = inl(LS7A_HPET_MAIN);//get time now
		cnt = CPU_GetCOUNT();
		cur = (inl(LS7A_HPET_PERIOD) / 1000000);
		sec = sec + (100000000 / cur);//go 100 ms
		do {
			timeout--;
			cur = (inl(LS7A_HPET_MAIN));
		} while (timeout != 0 && (cur < sec));

		cnt = CPU_GetCOUNT() - cnt;
		if (timeout == 0) {
			tgt_printf("time out!\n");
			break;	/* Get out if clock is not running */
		}
	}

	/*
	 *  Calculate the external bus clock frequency.
	 */
	if (timeout != 0) {
		clk_invalid = 0;
		md_pipefreq = cnt / 1000;

		if((cnt % 1000) >= 500)//to make rounding
			md_pipefreq = md_pipefreq + 1;

		md_pipefreq *= 20000;
		/* we have no simple way to read multiplier value
		 */
		md_cpufreq = 66000000;
	}
		cur = (inl(LS7A_HPET_PERIOD) / 1000000);
	tgt_printf("cpu freq %u, cnt %u\n", md_pipefreq, cnt);

	outl(LS7A_HPET_CONF, 0x0);//Disable main clock
#endif
#endif /* HAVE_TOD */
}
/*
 *   Returns the CPU pipelie clock frequency
 */
int tgt_pipefreq()
{
	if(md_pipefreq == 0) {
		_probe_frequencies();
	}
	return(md_pipefreq);
}

/*
 *   Returns the external clock frequency, usually the bus clock
 */
int tgt_cpufreq()
{
	if(md_cpufreq == 0) {
		_probe_frequencies();
	}
	return(md_cpufreq);
}

/*
ls2k RTC_TOY_READ1/0x1fe07830 read result will or RST_CNT/0x1fe07030 read result.
ls7a RTC_TOY_READ1/0x100d0130 read result will or RST_CNT/0x100d0030 read result.
so we need write RST_CNT/0x1fe07030 to 0 before read RTC_TOY_READ1/0x1fe07830.
we feed dog if wdt_en, because another core may feed dog when we set RST_CNT/0x1fe07030 to 0.
*/
#define RST_WDTEN 2
#ifdef LOONGSON_2K
#define RTS_CNT_OFFSET 0x800
#else
#define RTS_CNT_OFFSET 0x100
#endif
time_t tgt_gettime()
{
	struct tm tm;
	time_t t;
	unsigned int val;

#ifdef HAVE_TOD
	if (!clk_invalid) {
		unsigned int  rst_ctrl = inl(LS7A_TOY_READ1_REG - RTS_CNT_OFFSET);
		if (rst_ctrl & RST_WDTEN)
			outl(LS7A_TOY_READ1_REG - RTS_CNT_OFFSET, 0);
		tm.tm_year = inl(LS7A_TOY_READ1_REG);
		if (rst_ctrl & RST_WDTEN) {
			outl(LS7A_TOY_READ1_REG - RTS_CNT_OFFSET, rst_ctrl);
			outl(LS7A_TOY_READ1_REG - RTS_CNT_OFFSET + 4, 1);
		}
		val = inl(LS7A_TOY_READ0_REG);
		tm.tm_sec = (val >> 4) & 0x3f;
		tm.tm_min = (val >> 10) & 0x3f;
		tm.tm_hour = (val >> 16) & 0x1f;
		tm.tm_mday = (val >> 21) & 0x1f;
		tm.tm_mon = ((val >> 26) & 0x3f) - 1;
		tm.tm_isdst = tm.tm_gmtoff = 0;
		t = gmmktime(&tm);
	} else
#endif
	{
		t = 957960000;	/* Wed May 10 14:00:00 2000 :-) */
	}
	return (t);
}

/*
 *  Set the current time if a TOD clock is present
 */
void tgt_settime(time_t t)
{
	struct tm *tm;
	unsigned int val;

#ifdef HAVE_TOD
	if (!clk_invalid) {
		tm = gmtime(&t);
		val = ((tm->tm_mon + 1) << 26) | (tm->tm_mday << 21) |
			(tm->tm_hour << 16) | (tm->tm_min << 10) |
			(tm->tm_sec << 4);
		outl(LS7A_TOY_WRITE0_REG, val);
		outl(LS7A_TOY_WRITE1_REG, tm->tm_year);
	}
#endif
}

/*
 *  Print out any target specific memory information
 */
void tgt_memprint()
{
	printf("Primary Instruction cache size %dkb (%d line, %d way)\n",
			CpuPrimaryInstCacheSize / 1024, CpuPrimaryInstCacheLSize, CpuNWayCache);
	printf("Primary Data cache size %dkb (%d line, %d way)\n",
			CpuPrimaryDataCacheSize / 1024, CpuPrimaryDataCacheLSize, CpuNWayCache);
	if(CpuSecondaryCacheSize != 0) {
		printf("Secondary cache size %dkb\n", CpuSecondaryCacheSize / 1024);
	}
	if(CpuTertiaryCacheSize != 0) {
		printf("Tertiary cache size %dkb\n", CpuTertiaryCacheSize / 1024);
	}
}

void tgt_machprint()
{
	printf("Copyright 2000-2002, Opsycon AB, Sweden.\n");
	printf("Copyright 2005, ICT CAS.\n");
	printf("CPU %s @", md_cpuname());
} 

/*
 *  Return a suitable address for the client stack.
 *  Usually top of RAM memory.
 */

register_t tgt_clienttos()
{
	extern char start[];
	return(register_t)(int)PHYS_TO_CACHED(start - 64);
}

#ifdef HAVE_FLASH
/*
 *  Flash programming support code.
 */

/*
 *  Table of flash devices on target. See pflash_tgt.h.
 */

struct fl_map tgt_fl_map_boot16[]={
	TARGET_FLASH_DEVICES_16
};


struct fl_map *tgt_flashmap()
{
	return tgt_fl_map_boot16;
}   
void tgt_flashwrite_disable()
{
}

int tgt_flashwrite_enable()
{
	return(1);
}

void tgt_flashinfo(void *p, size_t *t)
{
	struct fl_map *map;

	map = fl_find_map(p);
	if(map) {
		*t = map->fl_map_size;
	}
	else {
		*t = 0;
	}
}

void tgt_flashprogram(void *p, int size, void *s, int endian)
{
	printf("Programming flash %x:%x into %x\n", s, size, p);
	if(fl_erase_device(p, size, TRUE)) {
		printf("Erase failed!\n");
		return;
	}
	if(fl_program_device(p, s, size, TRUE)) {
		printf("Programming failed!\n");
	}
	fl_verify_device(p, s, size, TRUE);
}
#endif /* PFLASH */

/*
 *  Network stuff.
 */
void tgt_netinit()
{
}

int tgt_ethaddr(char *p)
{
	bcopy((void *)&hwethadr, p, 6);
	return(0);
}

void tgt_netreset()
{
}

/*************************************************************************/
/*
 *	Target dependent Non volatile memory support code
 *	=================================================
 *
 *
 *  On this target a part of the boot flash memory is used to store
 *  environment. See EV64260.h for mapping details. (offset and size).
 */

/*
 *  Read in environment from NV-ram and set.
 */
void tgt_mapenv(int (*func) __P((char *, char *)))
{
	char *ep;
	char env[512];
	char *nvram;
	int i;

	/*
	 *  Check integrity of the NVRAM env area. If not in order
	 *  initialize it to empty.
	 */
	printf("in envinit\n");
	nvram = (char *)(tgt_flashmap())->fl_map_base;
	printf("nvram=%08x\n",(unsigned int)nvram);
	if(fl_devident(nvram, NULL) == 0 || cksum(nvram + NVRAM_OFFS, NVRAM_SIZE, 0) != 0) {
			printf("NVRAM is invalid!\n");
			nvram_invalid = 1;
		}
		else {
			nvram += NVRAM_OFFS;
			ep = nvram+2;;

			while(*ep != 0) {
				char *val = 0, *p = env;
				i = 0;
				while((*p++ = *ep++) && (ep <= nvram + NVRAM_SIZE - 1) && i++ < 255) {
					if((*(p - 1) == '=') && (val == NULL)) {
						*(p - 1) = '\0';
						val = p;
					}
				}
				if(ep <= nvram + NVRAM_SIZE - 1 && i < 255) {
					(*func)(env, val);
				}
				else {
					nvram_invalid = 2;
					break;
				}
			}
		}

		printf("NVRAM@%x\n",(u_int32_t)nvram);

		/*
		 *  Ethernet address for Galileo ethernet is stored in the last
		 *  six bytes of nvram storage. Set environment to it.
		 */
		bcopy(&nvram[ETHER_OFFS], hwethadr, 6);
		sprintf(env, "%02x:%02x:%02x:%02x:%02x:%02x", hwethadr[0], hwethadr[1],
				hwethadr[2], hwethadr[3], hwethadr[4], hwethadr[5]);
		(*func)("ethaddr", env);

#ifdef no_thank_you
		(*func)("vxWorks", env);
#endif


		sprintf(env, "%d", memorysize / (1024 * 1024));
		(*func)("memsize", env);

		sprintf(env, "%d", memorysize_high / (1024 * 1024));
		(*func)("highmemsize", env);
#ifdef MULTI_CHIP
		sprintf(env, "%d", memorysize_high_n1 / (1024 * 1024));
		(*func)("memorysize_high_n1", env);

		sprintf(env, "%d", memorysize_high_n2 / (1024 * 1024));
		(*func)("memorysize_high_n2", env);

		sprintf(env, "%d", memorysize_high_n3 / (1024 * 1024));
		(*func)("memorysize_high_n3", env);
#endif
		sprintf(env, "%d", md_pipefreq);
		(*func)("cpuclock", env);

		sprintf(env, "%d", md_cpufreq);
		(*func)("busclock", env);

		sprintf(env, "%d",VRAM_SIZE);
		(*func)("vramsize", env);

        sprintf(env, "%d",0);

        (*func)("sharevram", env);


		(*func)("systype", SYSTYPE);

}

int
tgt_unsetenv(char *name)
{
	char *ep, *np, *sp;
	char *nvram;
	char *nvrambuf;
	char *nvramsecbuf;
	int status;

	if(nvram_invalid) {
		return(0);
    }

			/* Use first defined flash device (we probably have only one) */
	nvram = (char *)(tgt_flashmap())->fl_map_base;

	/* Map. Deal with an entire sector even if we only use part of it */
	nvram += NVRAM_OFFS & ~(NVRAM_SECSIZE - 1);
	nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf == 0) {
		printf("Warning! Unable to malloc nvrambuffer!\n");
		return(-1);
	}
	memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
	nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));

	ep = nvrambuf + 2;

	status = 0;
	while((*ep != '\0') && (ep <= nvrambuf + NVRAM_SIZE)) {
		np = name;
		sp = ep;

		while((*ep == *np) && (*ep != '=') && (*np != '\0')) {
			ep++;
			np++;
		}
		if((*np == '\0') && ((*ep == '\0') || (*ep == '='))) {
			while(*ep++);
			while(ep <= nvrambuf + NVRAM_SIZE) {
				*sp++ = *ep++;
			}
			if(nvrambuf[2] == '\0') {
				nvrambuf[3] = '\0';
			}
			cksum(nvrambuf, NVRAM_SIZE, 1);
			if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
				status = -1;
				break;
			}

			if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
				status = -1;
				break;
			}
			status = 1;
			break;
		}
		else if(*ep != '\0') {
			while(*ep++ != '\0');
		}
	}

	free(nvramsecbuf);
	return(status);
}

int	tgt_setenv(char *name, char *value)
{
	char *ep;
	int envlen;
	char *nvrambuf;
	char *nvramsecbuf;
	char *nvram;

	/* Non permanent vars. */
	if(strcmp(EXPERT, name) == 0) {
		return(1);
	}

	/* Calculate total env mem size requiered */
	envlen = strlen(name);
	if(envlen == 0) {
		return(0);
	}
	if(value != NULL) {
		envlen += strlen(value);
	}
	envlen += 2;    /* '=' + null byte */
	if(envlen > 255) {
		return(0);      /* Are you crazy!? */
	}

	/* Use first defined flash device (we probably have only one) */
	nvram = (char *)(tgt_flashmap())->fl_map_base;

	/* Deal with an entire sector even if we only use part of it */
	nvram += NVRAM_OFFS & ~(NVRAM_SECSIZE - 1);

	/* If NVRAM is found to be uninitialized, reinit it. */
	if(nvram_invalid) {
		nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
		if(nvramsecbuf == 0) {
			printf("Warning! Unable to malloc nvrambuffer!\n");
			return(-1);
		}
		memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
		nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
		memset(nvrambuf, -1, NVRAM_SIZE);
		nvrambuf[2] = '\0';
		nvrambuf[3] = '\0';
		cksum((void *)nvrambuf, NVRAM_SIZE, 1);
		printf("Warning! NVRAM checksum fail. Reset!\n");
		if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
			printf("Error! Nvram erase failed!\n");
			free(nvramsecbuf);
			return(-1);
		}
		if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
			printf("Error! Nvram init failed!\n");
			free(nvramsecbuf);
			return(-1);
		}
		nvram_invalid = 0;
		free(nvramsecbuf);
	}

	/* Remove any current setting */
	tgt_unsetenv(name);

	/* Find end of evironment strings */
	nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf == 0) {
		printf("Warning! Unable to malloc nvrambuffer!\n");
		return(-1);
	}
	memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
	nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
	/* Etheraddr is special case to save space */
	if (strcmp("ethaddr", name) == 0) {
		char *s = value;
		int i;
		int32_t v;
		for(i = 0; i < 6; i++) {
			gethex(&v, s, 2);
			hwethadr[i] = v;
			s += 3;         /* Don't get to fancy here :-) */
		} 
	} else {
		ep = nvrambuf+2;
		if(*ep != '\0') {
			do {
				while(*ep++ != '\0');
			} while(*ep++ != '\0');
			ep--;
		}
		if(((int)ep + NVRAM_SIZE - (int)ep) < (envlen + 1)) {
			free(nvramsecbuf);
			return(0);      /* Bummer! */
		}

		/*
		 *  Special case heaptop must always be first since it
		 *  can change how memory allocation works.
		 */
		if(strcmp("heaptop", name) == 0) {

			bcopy(nvrambuf+2, nvrambuf+2 + envlen,
					ep - nvrambuf+1);

			ep = nvrambuf+2;
			while(*name != '\0') {
				*ep++ = *name++;
			}
			if(value != NULL) {
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else {
				*ep++ = '\0';
			}
		}
		else {
			while(*name != '\0') {
				*ep++ = *name++;
			}
			if(value != NULL) {
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else {
				*ep++ = '\0';
			}
			*ep++ = '\0';   /* End of env strings */
		}
	}
	cksum(nvrambuf, NVRAM_SIZE, 1);

	bcopy(hwethadr, &nvramsecbuf[ETHER_OFFS], 6);
	if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
		printf("Error! Nvram erase failed!\n");
		free(nvramsecbuf);
		return(0);
	}
	if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
		printf("Error! Nvram program failed!\n");
		free(nvramsecbuf);
		return(0);
	}
	free(nvramsecbuf);
	return(1);
}

/*
 *  Calculate checksum. If 'set' checksum is calculated and set.
 */
static int cksum(void *p, size_t s, int set)
{
	u_int16_t sum = 0;
	u_int8_t *sp = p;
	int sz = s / 2;

	if(set) {
		*sp = 0;	/* Clear checksum */
		*(sp+1) = 0;	/* Clear checksum */
	}
	while(sz--) {
		sum += (*sp++) << 8;
		sum += *sp++;
	}
	if(set) {
		sum = -sum;
		*(u_int8_t *)p = sum >> 8;
		*((u_int8_t *)p+1) = sum;
	}
	return(sum);
}


/*
 *  Simple display function to display a 4 char string or code.
 *  Called during startup to display progress on any feasible
 *  display before any serial port have been initialized.
 */
void tgt_display(char *msg, int x)
{
	/* Have simple serial port driver */
	tgt_putchar(msg[0]);
	tgt_putchar(msg[1]);
	tgt_putchar(msg[2]);
	tgt_putchar(msg[3]);
	tgt_putchar('\r');
	tgt_putchar('\n');
}

void clrhndlrs()
{
}

int
tgt_getmachtype()
{
	return(md_cputype());
}

/*
 *  Create stubs if network is not compiled in
 */
#ifdef INET
void tgt_netpoll()
{
	splx(splhigh());
}
#endif /*INET*/

#define SPINSZ		0x800000
#define DEFTESTS	7
#define MOD_SZ		20
#define BAILOUT		if (bail) goto skip_test;
#define BAILR		if (bail) return;
/* memspeed operations */
#define MS_BCOPY	1
#define MS_COPY		2
#define MS_WRITE	3
#define MS_READ		4


char *tran_month(char *c, char *i)
{
    switch (*c++){
    case  'J':
        if(*c++ == 'a')     /* Jan */
            i = "01";
        else if(*c++ == 'n')    /* June */
            i = "06";
        else                /* July */
            i = "07";
        break;
    case  'F':              /* Feb */
        i = "02";
        break;
    case  'M':
        c++;
        if(*c++ == 'r')     /* Mar */
            i = "03";
        else                /* May */
            i = "05";
        break;
    case  'A':
        if(*c++ == 'p')     /* Apr */
            i = "04";
        else                /* Aug */
            i = "08";
        break;
        case  'S':              /* Sept */
        i = "09";
        break;
    case  'O':              /* Oct */
        i = "10";
        break;
    case  'N':              /* Nov */
        i = "11";
        break;
    case  'D':              /* Dec */
        i = "12";
        break;
    default :
        i = NULL;
    }

    return i;
}

int get_update(char *p)
{
    char *t,*mp,m[3];

    t  = strstr(vers, ":");
    strncpy(p, t+26, 4);     /* year */
    p[4] = '-';
    mp = tran_month(t+6, m);    /* month */
    strncpy(p+5,mp,2);
    p[7]='-';
    strncpy(p+8, t+10, 2);   /* day */
    p[10] = '\0';

    return 0;
}

void clear_pcie_inter_irq(void)
{
        unsigned int dev;
        unsigned int val;
        unsigned int addr;
        int i;
        for(i = 9;i < 21;i++) {
                dev = _pci_make_tag(0, i, 0);
                val = _pci_conf_read(dev, 0x00);
                if( val != 0xffffffff){
                        addr = ((_pci_conf_read(dev, 0x10) & (~0xf)) | 0x80000000);
                       	val = inl(addr + 0x18);
                        if(val){
                                outl(addr + 0x1c, val);
                        }
                }
        }
}

int ls7a_get_irq(unsigned char dev, int fn)
{
	int irq;
	switch(dev)
	{
		default:
		case 2:
		/*APB 2*/
		irq = 0;
		break;

		case 3:
		/*GMAC0 3 0*/
		/*GMAC1 3 1*/
		irq = (fn == 0) ? 12 : 14;
		break;

		case 4:
		/* ohci:4 0 */
		/* ehci:4 1 */
		irq = (fn == 0) ? 49 : 48;
		break;

		case 5:
		/* ohci:5 0 */
		/* ehci:5 1 */
		irq = (fn == 0) ? 51 : 50;
		break;

		case 6:
		/* DC: 6 1 28 */
		/* GPU:6 0 29 */
		irq = (fn == 0) ? 29 : 28;
		break;

		case 7:
		/*HDA: 7 0 58 */
		irq = 58;
		break;

		case 8:
		/* sata */
		if (fn == 0)
			irq = 16;
		if (fn == 1)
			irq = 17;
		if (fn == 2)
			irq = 18;
		break;

		case 9:
		/* pcie_f0 port0 */
		irq = 32;
		break;

		case 10:
		/* pcie_f0 port1 */
		irq = 33;
		break;

		case 11:
		/* pcie_f0 port2 */
		irq = 34;
		break;

		case 12:
		/* pcie_f0 port3 */
		irq = 35;
		break;

		case 13:
		/* pcie_f1 port0 */
		irq = 36;
		break;

		case 14:
		/* pcie_f1 port1 */
		irq = 37;
		break;

		case 15:
		/* pcie_g0 port0 */
		irq = 40;
		break;

		case 16:
		/* pcie_g0 port1 */
		irq = 41;
		break;

		case 17:
		/* pcie_g1 port0 */
		irq = 42;
		break;

		case 18:
		/* pcie_g1 port1 */
		irq = 43;
		break;

		case 19:
		/* pcie_h port0 */
		irq = 38;
		break;

		case 20:
		/* pcie_h port1 */
		irq = 39;
		break;
	}
	return irq + 64;
}

void ls_pcie_interrupt_fixup(void)
{

	extern struct pci_device *_pci_head;
	extern int pci_roots;

	unsigned int tag;
	unsigned char i, irq;
	struct pci_device *pd,*pdd,*pddd;

	for (i = 0, pd = _pci_head; i < pci_roots; i++, pd = pd->next) {
		for (pdd = pd->bridge.child; pdd != NULL; pdd = pdd->next) {
			//printf("- bus %d device %d function %d\n", pdd->pa.pa_bus, pdd->pa.pa_device, pdd->pa.pa_function);
			tag = _pci_make_tag(pdd->pa.pa_bus, pdd->pa.pa_device, pdd->pa.pa_function);
			irq = ls7a_get_irq(pdd->pa.pa_device,pdd->pa.pa_function);
			_pci_conf_write8(tag, 0x3c, irq);
			//printf("irq -> %d\n", 0xff & _pci_conf_read(tag, 0x3c));
			for (pddd = pdd->bridge.child; pddd != NULL; pddd = pddd->next) {
				//printf("- bus %d device %d function %d\n", pddd->pa.pa_bus, pddd->pa.pa_device, pddd->pa.pa_function);
				tag = _pci_make_tag(pddd->pa.pa_bus, pddd->pa.pa_device, pddd->pa.pa_function);
				_pci_conf_write8(tag, 0x3c, irq);
				//printf("irq -> %d\n", 0xff & _pci_conf_read(tag, 0x3c));
			}
		}
	}
}

extern struct efi_memory_map_loongson g_map;

#include "../../../pmon/common/smbios/smbios.h"
#include "../../../pmon/cmds/bootparam.h"
struct efi_memory_map_loongson * init_memory_map()
{
	struct efi_memory_map_loongson *emap = &g_map;
	int i = 0;
	unsigned long long size = memorysize_high;

#define EMAP_ENTRY(entry, node, type, start, size) \
	emap->map[(entry)].node_id = (node), \
	emap->map[(entry)].mem_type = (type), \
	emap->map[(entry)].mem_start = (start), \
	emap->map[(entry)].mem_size = (size), \
	(entry)++

#if 1
	EMAP_ENTRY(i, 0, SYSTEM_RAM_LOW, 0x00200000, 0x0ee);
	/* for entry with mem_size < 1M, we set bit31 to 1 to indicate
	 * that the unit in mem_size is Byte not MBype */
	EMAP_ENTRY(i, 0, SMBIOS_TABLE, (SMBIOS_PHYSICAL_ADDRESS & 0x0fffffff),
			(SMBIOS_SIZE_LIMIT | 0x80000000));
	/* 0x20000000 size 512M */
	EMAP_ENTRY(i, 0, VUMA_VIDEO_RAM, 0x20000000, 0x200);
	/* SYSTEM_RAM_HIGH high 512M  */
	EMAP_ENTRY(i, 0, UMA_VIDEO_RAM, 0x90000000ULL + ((unsigned long long)(size - 0x20000000)), 0x200);

	EMAP_ENTRY(i, 0, SYSTEM_RAM_HIGH, 0x90000000, (size - 0x20000000) >> 20);
#endif

#ifdef MULTI_CHIP
	if(memorysize_high_n1) {
 		EMAP_ENTRY(i, 1, SYSTEM_RAM_LOW, 0x00000000000L, 0x100);
 		EMAP_ENTRY(i, 1, SYSTEM_RAM_HIGH, 0x00000000000L + 0x90000000, memorysize_high_n1 >> 20);
	}
#endif


	emap->vers = 1;
	emap->nr_map = i;
	return emap;
#undef	EMAP_ENTRY
}

#define HW_CONFIG 0xbfe00180
#define HW_SAMPLE 0xbfe00190
#define HT_MEM_PLL 0xbfe001c0

int mem_is_hw_bypassed()
{
	int32_t hw_sample = readl(HW_SAMPLE + 0x4);
	return ((hw_sample >> 5) & 0x1f) == 0x1f;
}

int mem_is_sw_setup()
{
	int32_t hw_sample = readl(HW_SAMPLE + 0x4);
	return ((hw_sample >> 5) & 0x1f) == 0x0f; 

}

int mem_hw_freq_mul()
{
	int32_t hw_mul = (readl(HW_SAMPLE + 0x4) >> 5) & 0xf;

	return hw_mul + 30;
}

int mem_hw_freq_div()
{
	int32_t hw_div = (readl(HW_SAMPLE + 0x4) >> 9) & 0x1;

	return hw_div + 3;
}

int mem_sw_freq_mul()
{
	int32_t sw_mul = (readl(HT_MEM_PLL) >> 14) & 0x3ff;

	return sw_mul;
}

int mem_sw_freq_div()
{
	int32_t sw_div = (readl(HT_MEM_PLL) >> 24) & 0x3f;

	return sw_div;
}

void print_mem_freq(void) 
{
	int mem_ref_clock = 33; /* int Mhz */

	if (mem_is_hw_bypassed()) { 
		printf("hw bypassed! mem@ %dMhz\n", mem_ref_clock);
		return;
	}
	if (!mem_is_sw_setup()) 
		printf("hw selected! mem@ %dMhz\n", (mem_hw_freq_mul() * mem_ref_clock)/mem_hw_freq_div());
	else
		printf("sw selected! mem@ %dMhz\n", (mem_sw_freq_mul() * mem_ref_clock)/mem_sw_freq_div());
	
}

extern struct interface_info g_board;
struct board_devices *board_devices_info()
{

	struct board_devices *bd = &g_board;

#ifdef  MULTI_CHIP
	strcpy(bd->name,"Loongson-3A3000-7A-Dev-2way-ATX_EVB");
#else
	strcpy(bd->name,"Loongson-3A3000-7A-Dev-1way-ATX_EVB");
#endif
	bd->num_resources = 10;

	return bd;
}

void  print_cpu_info()
{
	int cycles1, cycles2;
	int bogo;
	int loops = 1 << 18;
	int freq = tgt_pipefreq() / 1000000;

	printf("Copyright 2000-2002, Opsycon AB, Sweden.\n");
	printf("Copyright 2005, ICT CAS.\n");
	printf("CPU %s ", md_cpuname());

	cycles1 = (int)read_c0_count();
	__loop_delay(loops);
	cycles2 = (int)read_c0_count();

	bogo = freq * loops / (cycles2 - cycles1);

	printf("BogoMIPS: %d\n", bogo);
}

#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE
union commondata{
		unsigned char data1;
		unsigned short data2;
		unsigned int data4;
		unsigned int data8[2];
		unsigned char c[8];
};
extern unsigned int syscall_addrtype;
extern int (*syscall1)(int type,long long addr,union commondata *mydata);
extern int (*syscall2)(int type,long long addr,union commondata *mydata);


static int logicdev=0;
static int PnpRead_w83627(int type,long long addr,union commondata *mydata)
{
switch(type)
{
case 1:
mydata->data1=w83627_read(logicdev,addr);
break;
default: return -1;break;
}
return 0;
}

static int PnpWrite_w83627(int type,long long addr,union commondata *mydata)
{
switch(type)
{
case 1:
w83627_write(logicdev,addr,mydata->data1);
break;
default: return -1;break;
}
return 0;
}



static int pnps(int argc,char **argv)
{
logicdev=strtoul(argv[1],0,0);
syscall1=(void*)PnpRead_w83627;
syscall2=(void*)PnpWrite_w83627;
syscall_addrtype=0;
return 0;
}
#endif

unsigned long long strtoull(const char *nptr,char **endptr,int base);
int cmd_cpuexec(int argc, char **argv)
{
	unsigned long long pc;
	int cpu;
	cpu = (argc > 1)?strtoul(argv[1], 0, 0): 0;
	pc = (argc > 2)?strtoull(argv[2], 0, 0): 0;

	if (cpu < 4)
		__raw__writeq(0x900000003ff01020ULL + cpu*0x100, pc);
	else if (cpu < 8)
		__raw__writeq(0x900010003ff01020ULL + (cpu -4 )*0x100, pc);
	return 0;
}

static const Cmd Cmds[] =
{
	{"MyCmds"},
#if defined(LS7A_LPC_DISABLE) && !LS7A_LPC_DISABLE
	{"pnps",	"", 0, "select pnp ops for d1,m1 ", pnps, 0, 99, CMD_REPEAT},
#endif
	{"cpuexec",	"cpu pc", 0, "boot cpu from pc", cmd_cpuexec, 0, 99, CMD_REPEAT},
	{0, 0}
};

static void init_cmd __P((void)) __attribute__ ((constructor));

static void
init_cmd()
{
	cmdlist_expand(Cmds, 1);
}

