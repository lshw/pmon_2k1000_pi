# 1 "/home/pdb/loongson/loongsonpai/loongson/pmon-loongson3-20200728/zloader.ls2k/../Targets/LS2K/conf/LS2K.dts"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "/home/pdb/loongson/loongsonpai/loongson/pmon-loongson3-20200728/zloader.ls2k/../Targets/LS2K/conf/LS2K.dts"




/dts-v1/;
/ {
 model = "loongson,LS2k1000-EVP";
 compatible = "loongson,ls2k";
 #address-cells = <2>;
 #size-cells = <2>;


 memory {
  name = "memory";
  device_type = "memory";



  reg = <0 0x00200000 0 0x0ee00000

   1 0x10000000 1 0xb0000000>;
 };

 aliases {
  ethernet0 = &gmac0;
  ethernet1 = &gmac1;
 };



 soc {
  compatible = "ls,nbus", "simple-bus";
  #address-cells = <1>;
  #size-cells = <1>;
  ranges = <0x10000000 0 0x10000000 0x10000000
     0x40000000 0 0x40000000 0x40000000
     0x20000000 0 0x20000000 0x20000000
              0x0d000000 0 0x0d000000 0x02000000>;

  dma-coherent;
  nr_cpus_loongson = <2>;
  cores_per_package = <2>;

  icu: interrupt-controller@1fe11400 {
   compatible = "loongson,2k-icu";
   interrupt-controller;
   #interrupt-cells = <1>;
   reg = <0x1fe11400 0x40>;
  };

  msi: pci-msi-controller@1fe114a0 {
   compatible = "loongson,2k-pci-msi";
   msi-controller;
   interrupt-parent = <&icu>;
   msi-mask = <0x000000c0 0x40000c00>;
   reg = <0x1fe114a0 0x60>;
  };






  hda@0x400d0000 {
   compatible = "loongson,ls-audio";
   reg = <0x400d0000 0xffff>;
   interrupt-parent = <&icu>;
   interrupts = <12>;
  };


  dc@0x400c0000 {
   compatible = "loongson,ls-fb";
   reg = <0x400c0000 0x10000



       0x20000000 0x8000000>;

   interrupt-parent = <&icu>;
   interrupts = <36>;
  };


  gpu@0x40080000 {
   compatible = "loongson,galcore";
   reg = <0x40080000 0x40000
    0x2a000000 0x15000000>;
   interrupt-parent = <&icu>;
   interrupts = <37>;
  };


  ohci@0x40070000 {
   compatible = "loongson,ls2k-ohci", "usb-ohci", "generic-ohci";
   reg = <0x40070000 0x10000>;
   interrupt-parent = <&icu>;
   interrupts = <59>;
   dma-mask = <0x0 0xffffffff>;
  };

  ehci@0x40060000 {
   compatible = "loongson,ls2k-ehci", "usb-ehci";
   reg = <0x40060000 0x10000>;
   interrupt-parent = <&icu>;
   interrupts = <58>;
   dma-mask = <0xffffffff 0xffffffff>;
  };

  otg@0x40000000 {
   compatible = "loongson,dwc-otg", "dwc-otg";
   reg = <0x40000000 0x40000>;
   interrupt-parent = <&icu>;
   interrupts = <57>;
   dma-mask = <0x0 0xffffffff>;
  };

  ahci@0x400e0000{
   compatible = "snps,spear-ahci";
   reg = <0x400e0000 0x10000>;
   interrupt-parent = <&icu>;
   interrupts = <27>;
   dma-mask = <0x0 0xffffffff>;
  };

  gmac0: ethernet@0x40040000 {
   compatible = "snps,dwmac-3.70a", "ls,ls-gmac";
   reg = <0x40040000 0x10000>;
   interrupt-parent = <&icu>;
   interrupts = <20 21>;
   interrupt-names = "macirq", "eth_wake_irq";
   mac-address = [ 64 48 48 48 48 60 ];
   phy-mode = "rgmii";
   bus_id = <0x0>;
   phy_addr = <0xffffffff>;
   dma-mask = <0xffffffff 0xffffffff>;
  };

  gmac1: ethernet@0x40050000 {
   compatible = "snps,dwmac-3.70a", "ls,ls-gmac";
   reg = <0x40050000 0x10000>;
   interrupt-parent = <&icu>;
   interrupts = <22 23>;
   interrupt-names = "macirq", "eth_wake_irq";
   mac-address = [ 64 48 48 48 48 61 ];
   phy-mode = "rgmii";
   bus_id = <0x1>;
   phy_addr = <0xffffffff>;
   dma-mask = <0xffffffff 0xffffffff>;
  };

  pcie0_port0@40100000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <40>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0x1 0x3>;
     ranges = <0x02000000 0x0 0x40100000 0x40100000 0x0 0x8000000
      0x01000000 0x0 0x18001000 0x18001000 0x0 0x0ff000>;
  };

  pcie0_port1@50000000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <41>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0x4 0x6>;
     ranges = <0x02000000 0x0 0x50000000 0x50000000 0x0 0x4000000
      0x01000000 0x0 0x18100000 0x18100000 0x0 0x100000>;
  };

  pcie0_port2@54000000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <42>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0x8 0xa>;
     ranges = <0x02000000 0x0 0x54000000 0x54000000 0x0 0x4000000
      0x01000000 0x0 0x18200000 0x18200000 0x0 0x100000>;
  };

  pcie0_port3@58000000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <43>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0xc 0xe>;
     ranges = <0x02000000 0x0 0x58000000 0x58000000 0x0 0x8000000
      0x01000000 0x0 0x18300000 0x18300000 0x0 0x100000>;
  };

  pcie1_port0@60000000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <44>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0x10 0x12>;
     ranges = <0x02000000 0x0 0x60000000 0x60000000 0x0 0x18000000
      0x01000000 0x0 0x18400000 0x18400000 0x0 0x100000>;
  };

  pcie1_port1@78000000 {
   compatible = "loongson,ls-pcie";
   interrupt-parent = <&icu>;
   interrupts = <45>;
   #address-cells = <3>;
   #size-cells = <2>;
   bus-range = <0x14 0x16>;
     ranges = <0x02000000 0x0 0x78000000 0x78000000 0x0 0x8000000
      0x01000000 0x0 0x18500000 0x18500000 0x0 0x100000>;
  };

  serial0x@0x1fe00000{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00000 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <8>;
  };

  serial1x@0x1fe00100{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00100 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <8>;
  };

  serial2x@0x1fe00200{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00200 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <8>;
  };

  serial3x@0x1fe00300{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00300 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <8>;
  };

  serial4x@0x1fe00400{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00400 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <9>;
  };

  serial5x@0x1fe00500{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00500 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <9>;
  };

  serial6x@0x1fe00600{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00600 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <9>;
  };

  serial7x@0x1fe00700{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00700 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <9>;
  };

  serial8x@0x1fe00800{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00800 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <10>;
  };

  serial9x@0x1fe00900{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00900 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <10>;
  };

  serialax@0x1fe00a00{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00a00 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <10>;
  };

  serialbx@0x1fe00b00{
   device_type = "serial";
   compatible = "ns16550";
   reg = <0x1fe00b00 0x100>;
   clock-frequency = <125000000>;
   interrupt-parent = <&icu>;
   interrupts = <10>;
  };

  pioA:gpio@0x1fe10500{
   compatible = "ls,ls2k-gpio", "ls,ls-gpio";
   reg = <0x1fe10500 0x38>;
   ngpios = <64>;
   gpio-controller;
   #gpio-cells = <2>;
  };

  i2c0: i2c@1fe01000{
   compatible = "loongson,ls-i2c";
   reg = <0x1fe01000 0x8>;
   interrupt-parent = <&icu>;
   interrupts = <30>;
   #address-cells = <1>;
   #size-cells = <0>;
   eeprom@57{
    compatible = "atmel,24c16";
    reg = <0x57>;
    pagesize = <16>;
   };
  };
  i2c1: i2c@1fe01800{
   #address-cells = <1>;
   #size-cells = <0>;

   compatible = "loongson,ls-i2c";
   reg = <0x1fe01800 0x8>;
   interrupt-parent = <&icu>;
   interrupts = <31>;
   rtc@68{
    compatible = "dallas,ds1338";
    reg = <0x68>;
   };
   codec@1a{
    compatible = "codec_uda1342";
    reg = <0x1a>;
   };
   eeprom@50{
    compatible = "eeprom-edid";
    reg = <0x50>;
   };

  };

  i2c_dvo0:i2c@2{
   compatible = "i2c-gpio";
   gpios = <&pioA 1 0
     &pioA 0 0
    >;


   i2c-gpio,delay-us = <5>;
   #address-cells = <1>;
   #size-cells = <0>;






  };

  i2c_dvo1:i2c@3{
   compatible = "i2c-gpio";
   gpios = <&pioA 32 0
     &pioA 33 0
    >;


   i2c-gpio,delay-us = <5>;






   codec@1a{
    compatible = "realtek,rt5651";
    reg = <0x1a>;
   };
  };

  spi0: spi@1fff0220{
   compatible = "loongson,ls-spi";
   #address-cells = <1>;
   #size-cells = <0>;
   reg = <0x1fff0220 0x10>;
   spidev@3{
    compatible = "rohm,dh2228fv";
    spi-max-frequency = <100000000>;
    reg = <3>;
   };
   spidev@0{
    compatible = "m25p80";
    spi-max-frequency = <40000000>;
    reg = <0>;
   };
   spidev@1{
    compatible = "mt29f";
    spi-max-frequency = <20000000>;
    reg = <1>;

   number-of-parts = <0x2>;

   partition@0 {
    label = "kernel_partition";
    reg = <0x0000000 0x01e00000>;
   };

   partition@0x01400000 {
    label = "os_partition";
    reg = <0x01e00000 0x0>;
   };
   };
  };

  rtc0: rtc@1fe07800{
   compatible = "loongson,ls-rtc";
   reg = <0x1fe07800 0x100>;
   interrupt-parent = <&icu>;
   interrupts = <60>;
  };





  can0: can@1fe00c00{
   compatible = "nxp,sja1000";
   reg = <0x1fe00c00 0xff>;
   interrupt-parent = <&icu>;
   interrupts = <24>;
   nxp,tx-output-mode = <0>;
   nxp,tx-output-config = <0x16>;
   nxp,external-clock-frequency = <125000000>;
  };
  can1: can@1fe00d00{
   compatible = "nxp,sja1000";
   reg = <0x1fe00d00 0xff>;
   interrupt-parent = <&icu>;
   interrupts = <25>;
   nxp,tx-output-mode = <0>;
   nxp,tx-output-config = <0x16>;
   nxp,external-clock-frequency = <125000000>;
  };

  pwm0: pwm@1fe02000{
   compatible = "loongson,ls2k-pwm";
   reg = <0x1fe02000 0x10>;
   interrupt-parent = <&icu>;
   interrupts = <32>;
  };
  pwm1: pwm@1fe02010{
   compatible = "loongson,ls2k-pwm";
   reg = <0x1fe02010 0x10>;
   interrupt-parent = <&icu>;
   interrupts = <33>;
  };
  pwm2: pwm@1fe02020{
   compatible = "loongson,ls2k-pwm";
   reg = <0x1fe02020 0x10>;
   interrupt-parent = <&icu>;
   interrupts = <34>;
  };
  pwm3: pwm@1fe02030{
   compatible = "loongson,ls2k-pwm";
   reg = <0x1fe02030 0x10>;
   interrupt-parent = <&icu>;
   interrupts = <35>;
  };






  apbdma: apbdma@1fe10438{
   compatible = "loongson,ls-apbdma";
   reg = <0x1fe10438 0x8>;
   #config-nr = <2>;
  };




  dma0: dma@1fe10c00 {
   compatible = "loongson,ls-apbdma-0";
   reg = <0x1fe10c00 0x8>;
   apbdma-sel = <&apbdma 0x0 0x0>;
   #dma-cells = <1>;
   dma-channels = <1>;
   dma-requests = <1>;
  };
  dma1: dma@1fe10c10 {
   compatible = "loongson,ls-apbdma-1";
   reg = <0x1fe10c10 0x8>;
   apbdma-sel = <&apbdma 0x5 0x1>;
   #dma-cells = <1>;
   dma-channels = <1>;
   dma-requests = <1>;
  };
  dma2: dma@1fe10c20 {
   compatible = "loongson,ls-apbdma-2";
   reg = <0x1fe10c20 0x8>;
   apbdma-sel = <&apbdma 0x6 0x2>;
   #dma-cells = <1>;
   dma-channels = <1>;
   dma-requests = <1>;
  };
  dma3: dma@1fe10c30 {
   compatible = "loongson,ls-apbdma-3";
   reg = <0x1fe10c30 0x8>;
   apbdma-sel = <&apbdma 0x7 0x3>;
   #dma-cells = <1>;
   dma-channels = <1>;
   dma-requests = <1>;
  };
# 553 "/home/pdb/loongson/loongsonpai/loongson/pmon-loongson3-20200728/zloader.ls2k/../Targets/LS2K/conf/LS2K.dts"
  sdio@0x1fe0c000 {
   #address-cells = <1>;
   #size-cells = <1>;

   compatible = "loongson,ls2k_sdio";
   reg = <0x1fe0c000 0x1000>;
   interrupt-parent = <&icu>;
   interrupts = <39>;
   interrupt-names = "ls2k_mci_irq";

   dmas = <&dma1 1>;
   dma-names = "sdio_rw";
   dma-mask = <0xffffffff 0xffffffff>;
  };
  i2s: i2s@0x1fe0d000 {
   compatible = "loongson,ls-i2s";
   reg = <0x1fe0d000 0x10>;
  };
  audio: audio@0x1fe0d000 {
   compatible = "loongson,ls-pcm-audio";
   reg = <0x1fe0d000 0x10>;
   interrupt-parent = <&icu>;
   interrupts = <54 55>;
   interrupt-names = "i2s_irq0", "i2s_irq1";

   dmas = <&dma2 1
           &dma3 1>;
   dma-names = "i2s_play", "i2s_record";
   dma-mask = <0xffffffff 0xffffffff>;
  };
  sound {
   compatible = "loongson,ls-sound";
   loongson,i2s-controller = <&i2s>;
   loongson,audio-codec = <&audio>;
   codec-names = "rt5651","rt5651 Duplex", "rt5651-aif1", "rt5651.3-001a";
  };
  nand@0x1fe06040{
   #address-cells = <1>;
   #size-cells = <1>;
   compatible = "loongson,ls-nand";
   reg = <0x1fe06040 0x0
    0x1fe06000 0x20>;
   interrupt-parent = <&icu>;
   interrupts = <52>;
   interrupt-names = "nand_irq";

   dmas = <&dma0 1>;
   dma-names = "nand_rw";
   dma-mask = <0xffffffff 0xffffffff>;

   number-of-parts = <0x2>;

   partition@0 {
    label = "kernel_partition";
    reg = <0x0000000 0x01e00000>;
   };

   partition@0x01400000 {
    label = "os_partition";
    reg = <0x01e00000 0x0>;
   };
  };
  hwmon@0x1fe07000{
   #address-cells = <1>;
   #size-cells = <1>;

   compatible = "loongson,ls-hwmon";
   reg = <0x1fe07000 0x1>;
   max-id = <0>;
   id = <0>;
  };
 };

 suspend_to_ram {
  suspend_addr = <0x1fc00500>;
 };

 chosen {

  bootargs = "console=ttyS0,115200 root=/dev/sda1                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      ";
 };
};
