ROOT := $(abspath ..)
OO_PS4_TOOLCHAIN ?= $(ROOT)/.toolchains/OpenOrbis/PS4Toolchain
GOLDHEN_SDK ?= $(ROOT)/GoldHEN_Plugins_SDK
PAYLOAD_SDK ?= $(ROOT)/.toolchains/ps4-payload-sdk
export OO_PS4_TOOLCHAIN
CC := clang-18
LD := ld.lld-18
FSELF := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself
# GoldHEN can enter plugin callbacks with an 8-byte-aligned stack. Realign every
# compiled function before using SSE stack slots or calling runtime functions.
CFLAGS := --target=x86_64-pc-freebsd12-elf -std=c11 -fPIC -funwind-tables -mstackrealign -O1 -g -Wall -Wextra -Werror -include sched.h -isysroot $(OO_PS4_TOOLCHAIN) -I$(GOLDHEN_SDK)/include -I$(OO_PS4_TOOLCHAIN)/include
LDFLAGS := -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x -e _init --eh-frame-hdr -z max-page-size=0x4000 -z common-page-size=0x4000 -L$(OO_PS4_TOOLCHAIN)/lib
COMMON := build/crtprx.o build/GoldHEN.o build/Syscall.o build/probe_common.o
LIBS := -lSceLibcInternal -lkernel
.PHONY: all
all: dist/hen_rest_start.bin dist/hen_rest_hook.prx dist/hen_rest_keeper.prx
build dist:
	mkdir -p $@
build/crtprx.o: $(GOLDHEN_SDK)/crt/crtprx.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/GoldHEN.o: $(GOLDHEN_SDK)/source/GoldHEN.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/Syscall.o: $(GOLDHEN_SDK)/source/Syscall.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: source/%.c source/probe_common.h source/process_records.h source/hook_record.h source/startup_records.h source/mono_api.h Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/hen_rest_hook.elf: $(COMMON) build/rest_redirect.o build/mono_api.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
build/hen_rest_keeper.elf: $(COMMON) build/keeper.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
dist/%.prx: build/%.elf | dist
	$(FSELF) -in=$< -out=build/$*.oelf --lib=$@ --paid 0x3800000000000011

# Build the payload SDK in a private directory. Upstream stays pinned and clean.
PAYLOAD_CFLAGS := -I$(PAYLOAD_SDK)/libPS4/include -Os -std=c11 -ffunction-sections -fdata-sections -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -masm=intel -march=btver2 -mtune=btver2 -m64 -mabi=sysv -mcmodel=small -fpie -fPIC -fvisibility=hidden -mstackrealign
PAYLOAD_SOURCES := $(wildcard $(PAYLOAD_SDK)/libPS4/source/*.c)
PAYLOAD_ASM := $(wildcard $(PAYLOAD_SDK)/libPS4/source/*.s)
PAYLOAD_OBJS := $(patsubst $(PAYLOAD_SDK)/libPS4/source/%.c,build/payload/%.o,$(PAYLOAD_SOURCES)) $(patsubst $(PAYLOAD_SDK)/libPS4/source/%.s,build/payload/%.o,$(PAYLOAD_ASM))
build/payload:
	mkdir -p $@
build/payload/%.o: $(PAYLOAD_SDK)/libPS4/source/%.c Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/payload/%.o: $(PAYLOAD_SDK)/libPS4/source/%.s Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/payload/libPS4.a: $(PAYLOAD_OBJS)
	ar rcs $@ $^
build/payload/start_payload.o: source/start_payload.c source/process_records.h source/startup_records.h Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -Werror -c $< -o $@
build/payload/crt0.o: $(PAYLOAD_SDK)/libPS4/crt0.s Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/hen_rest_start.elf: build/payload/start_payload.o build/payload/libPS4.a build/payload/crt0.o payload.ld
	gcc -nostdlib -nostartfiles -pie -Wl,--no-dynamic-linker -Wl,-Bsymbolic -Wl,--build-id=none -Wl,--gc-sections -Wl,-T,payload.ld -Wl,-z,max-page-size=0x4000 -o $@ build/payload/crt0.o build/payload/start_payload.o build/payload/libPS4.a
dist/hen_rest_start.bin: build/hen_rest_start.elf | dist
	objcopy -O binary --set-section-flags .bss=alloc,load,contents $< $@
