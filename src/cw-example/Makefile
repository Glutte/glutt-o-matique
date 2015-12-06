###
# GNU ARM Embedded Toolchain
CC=arm-none-eabi-gcc
LD=arm-none-eabi-ld
AR=arm-none-eabi-ar
AS=arm-none-eabi-as
CP=arm-none-eabi-objcopy
OD=arm-none-eabi-objdump
SIZE=arm-none-eabi-size

###
# Directory Structure
BINDIR=bin
SRCDIR=.

###
# Find source files
ASOURCES=$(shell find -L $(SRCDIR) -name '*.s')
CSOURCES+=$(shell find -L $(SRCDIR) -name '*.c')
# Find header directories
INC=$(shell find -L . -name '*.h' -exec dirname {} \; | uniq)
INCLUDES=$(INC:%=-I%)
# Create object list
OBJECTS=$(ASOURCES:%.s=%.o)
OBJECTS+=$(CSOURCES:%.c=%.o)
# Define output files ELF & IHEX
BINELF=outp.elf
BINHEX=outp.hex

###
# MCU FLAGS
MCFLAGS=-mcpu=cortex-m4 -mthumb -mlittle-endian \
-mfpu=fpv4-sp-d16 -mfloat-abi=softfp -mthumb-interwork
# COMPILE FLAGS
DEFS=-DUSE_STDPERIPH_DRIVER -DSTM32F4XX -DARM_MATH_CM4 -D__FPU_PRESENT=1
CFLAGS  =-Wall -ggdb -std=c99 -c $(MCFLAGS) $(DEFS) $(INCLUDES)
# LINKER FLAGS
LDSCRIPT= bsp/stm32_flash.ld
LDFLAGS =-T $(LDSCRIPT) --specs=nosys.specs $(MCFLAGS)

###
# Optimizations
OPT?='O1 O2 O3 O4 O6 O7' # O5 disabled by default, because it breaks code

ifneq ($(findstring release-memopt,$(MAKECMDGOALS)),)
ifneq ($(filter O1,$(OPT)),)
CXXFLAGS+=-fno-exceptions # Uncomment to disable exception handling
DEFS+=-DNO_EXCEPTIONS # The source code has to comply with this rule
endif

ifneq ($(filter O2,$(OPT)),)
CFLAGS+=-Os # Optimize for size https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
CXXFLAGS+=-Os
LDFLAGS+=-Os # Optimize for size https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
endif

ifneq ($(filter O3,$(OPT)),)
CFLAGS+=-ffunction-sections -fdata-sections # Place each function or data item into its own section in the output file
CXXFLAGS+=-ffunction-sections -fdata-sections # -||-
LDFLAGS+=-Wl,-gc-sections # Remove isolated unused sections
endif

ifneq ($(filter O4,$(OPT)),)
CFLAGS+=-fno-builtin # Disable C++ exception handling
CXXFLAGS+=-fno-builtin # Disable C++ exception handling
endif

ifneq ($(filter O5,$(OPT)),)
CFLAGS+=-flto # Enable link time optimization
CXXFLAGS+=-flto # Enable link time optimization
LDFLAGS+=-flto # Enable link time optimization
endif

ifneq ($(filter O6,$(OPT)),)
CXXFLAGS+=-fno-rtti # Disable type introspection
endif

ifneq ($(findstring O7,$(OPT)),)
LDFLAGS+=--specs=nano.specs # Use size optimized newlib
endif
endif

###
# Build Rules
.PHONY: all release release-memopt debug clean

all: release

release: $(BINDIR)/$(BINHEX)

release-memopt: release

debug: CFLAGS+=-g
debug: LDFLAGS+=-g
debug: release

$(BINDIR)/$(BINHEX): $(BINDIR)/$(BINELF)
	$(CP) -O ihex $< $@

$(BINDIR)/$(BINELF): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	$(SIZE) $(BINDIR)/$(BINELF)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(BINDIR)/$(BINELF) $(BINDIR)/$(BINHEX)

# Connect to openocd's gdb server on port 3333
deploy:
ifeq ($(wildcard /opt/openocd/bin/openocd),)
	/usr/bin/openocd -f /usr/share/openocd/scripts/board/stm32f4discovery.cfg -c "program bin/"$(BINELF)" verify reset" -c "init" -c "reset halt"
else
	/opt/openocd/bin/openocd -f /opt/openocd/share/openocd/scripts/board/stm32f4discovery.cfg -c "program bin/"$(BINELF)" verify reset" -c "init" -c "reset halt"
endif
