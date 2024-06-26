#	$Id: Makefile.Bonito2fdev,v 1.1.1.1 2006/09/14 01:59:09 root Exp $
#
# Makefile for PMON2000 EV64240
#
# This makefile is constructed from a machine description:
#	config machineid
# Most changes should be made in the machine description
#	/sys/arch/pmonppc/conf/``machineid''
# after which you should do
#	config machineid
# Machine generic makefile changes should be made in
#	/sys/arch/pmonppc/conf/Makefile.pmonppc
# after which config should be rerun for all machines of that type.
#
# N.B.: NO DEPENDENCIES ON FOLLOWING FLAGS ARE VISIBLE TO MAKEFILE
#	IF YOU CHANGE THE DEFINITION OF ANY OF THESE RECOMPILE EVERYTHING
#
.SUFFIXES:	.S .c .o

CROSS_COMPILE 	?=mipsel-linux-

#
# Include the make variables (CC, etc...)
#

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld  -m elf32ltsmip -G 0 -static -n -nostdlib
CC		?= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
RANLIB		= $(CROSS_COMPILE)ranlib
SIZE		= $(CROSS_COMPILE)size

ifeq "$(shell echo ${CC}|grep  'mips-elf-')" ""
 ifeq "$(shell echo ${CC}|grep  '\-mabi=64')" ""
LD		= $(CROSS_COMPILE)ld  -m elf32ltsmip -G 0 -static -n -nostdlib
 else
LD		= $(CROSS_COMPILE)ld  -m elf64ltsmip -G 0 -static -n -nostdlib
 endif
else
ENDIAN_FLAG= -EL
LD		= $(CROSS_COMPILE)ld  -m elf32ebmip -G 0 -static -n -nostdlib -EL
endif

OPT?=	-O2
IDENT:=${IDENT} $(shell echo  ${IDENT}|sed -n 's/.*-DX\([0-9]\+\)x\([0-9]\+\).*/ -DFB_XSIZE=\1 -DFB_YSIZE=\2 /p')
IDENT:=${IDENT} $(shell echo  ${IDENT}|sed -n 's/.*-DCONFIG_VIDEO_\([0-9]\+\)BPP.*/ -DFB_COLOR_BITS=\1 /p')

all: pmon

# source tree is located via $S relative to the compilation directory
ifndef S
S:=$(shell cd ../../../..; pwd)
endif

# Defines

START?=start.o
MACHINE=mips
MACHINE_ARCH=mips
COMPILEDIR=${shell pwd}
OBJDIR=${COMPILEDIR}
PMONDIR=${S}


INCLUDES=	-I. -I${S}/include -I./machine -I${S} \
		-I${S}/sys/arch/${MACHINE}/include -I${S}/sys \
		-I${TARGET} -I${COMPILEDIR} -I${PATH1} -nostdinc -fno-strict-aliasing -fno-pic

ifneq "$(findstring $S/x86emu/src,$(CFILES))" ""
		INCLUDES += -I${S}/x86emu/src/x86emu/ -I${S}/x86emu/src/x86emu/include
else
		INCLUDES += -I${S}/x86emu/int10/x86emu/include -I${S}/x86emu/int10/x86emu/src/x86emu/x86emu 
endif

CPPFLAGS := ${CPPFLAGS} ${ENDIAN_FLAG}	${INCLUDES} ${IDENT} -D_KERNEL -D__OpenBSD__ -DPMON -D__PMON__\
		  -mmemcpy -mno-abicalls -fno-builtin#-march=r4600 
#CWARNFLAGS=	-Werror -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wno-uninitialized -Wno-format -Wno-main
CWARNFLAGS=	-Wall -Wstrict-prototypes \
		-Wno-uninitialized -Wno-format -Wno-main
CFLAGS=		${DEBUG} ${CWARNFLAGS} ${OPT} -G 0
AFLAGS=		${DEBUG} -D_LOCORE -G 0
LFLAGS=	${ENDIAN_FLAG} -N -G 0 -T../../conf/ld.script -e start
STRIPFLAGS=	-g -S --strip-debug

HOSTCC?=	${CC}
HOSTED_CPPFLAGS=${CPPFLAGS:S/^-nostdinc$//}
HOSTED_CFLAGS=	${CFLAGS}

include ${S}/lib/libc/Makefile.inc
LIBC=${CLIB}
include ${S}/lib/libm/Makefile.inc
LIBM=${MLIB}
include ${S}/lib/libz/Makefile.inc
LIBZ=${ZLIB}

# compile rules: rules are named ${TYPE}_${SUFFIX}${CONFIG_DEP}
# where TYPE is NORMAL, DRIVER, or PROFILE}; SUFFIX is the file suffix,
# capitalized (e.g. C for a .c file), and CONFIG_DEP is _C if the file
# is marked as config-dependent.

USRLAND_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
USRLAND_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

NORMAL_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
NORMAL_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

DRIVER_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c $<
DRIVER_C_C=	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} ${PARAM} -c $<

NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<
NORMAL_S_C=	${AS}  ${COPTS} ${PARAM} $< -o $@




# load lines for config "xxx" will be emitted as:
# xxx: ${SYSTEM_DEP}
#	${SYSTEM_LD_HEAD}
#	${SYSTEM_LD}
#	${SYSTEM_LD_TAIL}
#SYSTEM_OBJ=	${START} crtbegin.o param.o ioconf.o ri.o ${OBJS} ${LIBC} ${LIBM} ${LIBZ} \

SYSTEM_OBJ=	${START} crtbegin.o param.o ioconf.o ri.o ${OBJS} ${LIBC} ${LIBM} ${LIBZ} \
		crtend.o
SYSTEM_DEP=	Makefile ${SYSTEM_OBJ}
SYSTEM_LD_HEAD=	rm -f $@
SYSTEM_LD=	@echo ${LD} ${LFLAGS} -o $@ ${LIBDIR} '${SYSTEM_OBJ}' vers.o; \
		${LD} ${LFLAGS} -o $@ ${LIBDIR} ${SYSTEM_OBJ} vers.o -L../../../../examples/math/ -lgcc
SYSTEM_LD_TAIL=	@${SIZE} $@; chmod 755 $@ ; \
		${OBJCOPY} -O binary $@ $@.bin

DEBUG?=
ifneq ("${DEBUG}", "")
LFLAGS+=	-X
SYSTEM_LD_TAIL+=; \
		echo cp $@ $@.gdb; rm -f $@.gdb; cp $@ $@.gdb; \
		echo ${STRIP} ${STRIPFLAGS} $@; ${STRIP} ${STRIPFLAGS} $@
else
LFLAGS+=	-S
endif


param.c: $S/sys/kern/param.c
	rm -f param.c
	cp $S/sys/kern/param.c .

param.o: param.c Makefile
	${NORMAL_C_C}

ioconf.o: ioconf.c
	${NORMAL_C}
ri.o:ri.c Makefile
	${NORMAL_C_C}
ri.c: $S/pmon/arch/mips/ri.c
	rm -f ri.c
	cp $S/pmon/arch/mips/ri.c .
crtbegin.c: $S/pmon/arch/mips/crtbegin.c
	rm -f crtbegin.c
	cp $S/pmon/arch/mips/crtbegin.c .

crtbegin.o: crtbegin.c Makefile
	${NORMAL_C_C}

crtend.c: $S/pmon/arch/mips/crtend.c
	rm -f crtend.c
	cp $S/pmon/arch/mips/crtend.c .

crtend.o: crtend.c Makefile
	${NORMAL_C_C}

newvers: ${SYSTEM_DEP} ${SYSTEM_SWAP_DEP}
	sh $S/conf/newvers.sh
	${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} -c vers.c

clean::
	rm -f eddep *netbsd netbsd.gdb tags *.[io] [a-z]*.s \
		[Ee]rrs linterrs makelinks genassym genassym.o 

lint:
	@lint -hbxncez -DGENERIC -Dvolatile= ${CPPFLAGS} -UKGDB \
		${CFILES} \
		ioconf.c param.c | \
		grep -v 'static function .* unused'

tags:
	@echo "see $S/kern/Makefile for tags"

links:
	egrep '#if' ${CFILES} | sed -f $S/conf/defines | \
	  sed -e 's/:.*//' -e 's/\.c/.o/' | sort -u > dontlink
	echo ${CFILES} | tr -s ' ' '\12' | sed 's/\.c/.o/' | \
	  sort -u | comm -23 - dontlink | \
	  sed 's,../.*/\(.*.o\),rm -f \1; ln -s ../GENERIC/\1 \1,' > makelinks
	sh makelinks && rm -f dontlink

SRCS=	${TARGET}/${SUBTARGET}/start.S \
	param.c ioconf.c ri.c ${CFILES} ${SFILES}
depend:: .depend
.depend: ${SRCS} param.c
	${MKDEP} ${AFLAGS} ${shell echo ${CPPFLAGS}|sed -e 's/ -f[^ ]*//g' } ${TARGET}/${SUBTARGET}/start.S
	${MKDEP} -a ${CFLAGS} ${shell echo ${CPPFLAGS}|sed -e 's/ -f[^ ]*//g' } param.c ioconf.c ${CFILES}
ifneq (${SFILES}, "")
	${MKDEP} -a ${AFLAGS} ${shell echo ${CPPFLAGS}|sed -e 's/ -f[^ ]*//g' } ${SFILES}
endif

# depend on root or device configuration
autoconf.o conf.o: Makefile

# depend on network or filesystem configuration
uipc_proto.o vfs_conf.o: Makefile

${START}: ${TARGET}/${SUBTARGET}/${START:.o=.S} Makefile
	${NORMAL_S}

dtb:
	${CC} ${AFLAGS} ${CPPFLAGS}  -E  -D__ASSEMBLY__ -D__DTS__ -Ulinux -x assembler-with-cpp -o $(DTB_O) $(DTB_I)

zpmon: startz.o
	rm start.o && cp -f startz.o start.o
	make pmon
	make -C ../zboot zpmon
startz.o: ${TARGET}/${SUBTARGET}/startz.S Makefile
	${NORMAL_S}

