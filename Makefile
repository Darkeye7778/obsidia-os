.DEFAULT_GOAL := all

.RECIPEPREFIX := >

CC      := gcc
LD      := ld
NASM    := nasm
AR      := ar
NATIVE_EXEC_EXT := obsx

BUILD_DIR    := build
KERNEL_BUILD := $(BUILD_DIR)/kernel
USER_BUILD   := $(BUILD_DIR)/user
ROOTFS_BUILD := $(BUILD_DIR)/rootfs
ISO_ROOT     := $(BUILD_DIR)/iso
TEST_ROOTFS_BUILD := $(BUILD_DIR)/test-rootfs
TEST_ISO_ROOT := $(BUILD_DIR)/test-iso

KERNEL := $(BUILD_DIR)/kernel.elf
INITRD := $(BUILD_DIR)/initrd.oar
ISO    := $(BUILD_DIR)/obsidia.iso
TEST_INITRD := $(BUILD_DIR)/test-initrd.oar
TEST_ISO := $(BUILD_DIR)/obsidia-test.iso
DISK   := $(BUILD_DIR)/obsidia_disk.img

LIMINE_DIR := third_party/limine
LIMINE_BIN := $(LIMINE_DIR)/limine

CFLAGS := \
    -ffreestanding \
    -m64 \
    -mcmodel=kernel \
    -mno-red-zone \
    -mgeneral-regs-only \
    -fno-pic \
    -Ikernel

LDFLAGS := \
    -nostdlib \
    -z max-page-size=0x1000 \
    -T linker.ld

# ============================================================
# Kernel
# ============================================================

KERNEL_C_SRCS := $(shell find kernel -type f -name '*.c' | sort)
KERNEL_ASM_SRCS := $(shell find kernel -type f -name '*.asm' | sort)

KERNEL_C_OBJS := \
    $(patsubst kernel/%.c,$(KERNEL_BUILD)/%.o,$(KERNEL_C_SRCS))

KERNEL_ASM_OBJS := \
    $(patsubst kernel/%.asm,$(KERNEL_BUILD)/%.o,$(KERNEL_ASM_SRCS))

KERNEL_OBJS := $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

$(KERNEL_BUILD)/%.o: kernel/%.c
>@mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BUILD)/%.o: kernel/%.asm
>@mkdir -p $(dir $@)
>$(NASM) -f elf64 $< -o $@

$(KERNEL): $(KERNEL_OBJS) linker.ld
>@mkdir -p $(dir $@)
>$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

# ============================================================
# Userspace runtime
# ============================================================

USER_CC := gcc
USER_LD := ld

USER_CFLAGS := \
    -ffreestanding \
    -m64 \
    -mcmodel=large \
    -mno-red-zone \
    -fno-pic \
    -fno-pie \
    -nostdlib \
    -nostdinc \
    -Iuser/runtime \
    -Iuser/include \
    -Wall \
    -Wextra \
    -O2

USER_LDFLAGS := \
    -T user/runtime/linker.ld \
    -nostdlib

USER_CRT0 := $(USER_BUILD)/runtime/crt0.o
USER_LIB := $(USER_BUILD)/lib/libobsidia.a
USER_LIB_SRCS := $(shell find user/lib -type f -name '*.c' | sort)
USER_LIB_OBJS := $(patsubst user/lib/%.c,$(USER_BUILD)/lib/%.o,$(USER_LIB_SRCS))

$(USER_CRT0): user/runtime/crt0.asm
>@mkdir -p $(dir $@)
>$(NASM) -f elf64 $< -o $@

$(USER_BUILD)/lib/%.o: user/lib/%.c
>@mkdir -p $(dir $@)
>$(USER_CC) $(USER_CFLAGS) -c $< -o $@

$(USER_LIB): $(USER_LIB_OBJS)
>@mkdir -p $(dir $@)
>$(AR) rcs $@ $^

# Helper for freestanding ELF userspace programs.
define USER_ELF
$(USER_BUILD)/$(1).elf: $(2) $(USER_CRT0) $(USER_LIB) user/runtime/linker.ld
>@mkdir -p $$(dir $$@)
>$(USER_CC) $(USER_CFLAGS) -c $(2) -o $(USER_BUILD)/$(1).main.o
>$(USER_LD) $(USER_LDFLAGS) $(USER_CRT0) $(USER_BUILD)/$(1).main.o $(USER_LIB) -o $$@
>@rm -f $(USER_BUILD)/$(1).main.o
endef

# Build through ELF as a host-tool intermediate, then extract PT_LOAD data into
# the independent OBSX runtime format. The installed artifact contains no ELF.
define USER_OBSX
$(USER_BUILD)/$(1).$(NATIVE_EXEC_EXT).elf: $(2) $(USER_CRT0) $(USER_LIB) user/runtime/linker.ld
>@mkdir -p $$(dir $$@)
>$(USER_CC) $(USER_CFLAGS) -c $(2) -o $(USER_BUILD)/$(1).obsx.o
>$(USER_LD) $(USER_LDFLAGS) $(USER_CRT0) $(USER_BUILD)/$(1).obsx.o $(USER_LIB) -o $$@
>@rm -f $(USER_BUILD)/$(1).obsx.o
$(USER_BUILD)/$(1).$(NATIVE_EXEC_EXT): $(USER_BUILD)/$(1).$(NATIVE_EXEC_EXT).elf tools/mkobs.py
>python3 tools/mkobs.py $$< $$@
endef

# ============================================================
# Actual Obsidia userspace
# ============================================================

$(eval $(call USER_ELF,system/init,user/system/init/main.c))
$(eval $(call USER_ELF,system/test-init,user/system/test-init/main.c))
$(eval $(call USER_ELF,apps/shell,user/apps/shell/main.c))
$(eval $(call USER_ELF,apps/hello,user/apps/hello/main.c))
$(eval $(call USER_OBSX,apps/native-smoke,user/apps/native-smoke/main.c))
$(eval $(call USER_OBSX,apps/desktop,user/apps/desktop/main.c))
$(eval $(call USER_OBSX,services/displayd,user/services/displayd/main.c))
$(eval $(call USER_OBSX,system/serviced,user/system/serviced/main.c))

# ============================================================
# Kernel/platform regression tests
# ============================================================

$(eval $(call USER_ELF,tests/fpu,user/tests/fpu.c))
$(eval $(call USER_ELF,tests/fs,user/tests/fs.c))
$(eval $(call USER_ELF,tests/invalid,user/tests/invalid.c))
$(eval $(call USER_ELF,tests/ipc_client,user/tests/ipc_client.c))
$(eval $(call USER_ELF,tests/ipc_sender,user/tests/ipc_sender.c))
$(eval $(call USER_ELF,tests/shm_client,user/tests/shm_client.c))
$(eval $(call USER_ELF,tests/surface,user/tests/surface.c))
$(eval $(call USER_ELF,tests/vm,user/tests/vm.c))
$(eval $(call USER_ELF,tests/fault,user/tests/fault.c))

$(USER_BUILD)/fixtures/win-smoke.exe: tools/mkpe_fixture.py
>@mkdir -p $(dir $@)
>python3 tools/mkpe_fixture.py $@

$(USER_BUILD)/fixtures/imports.exe: tools/mkpe_fixture.py
>@mkdir -p $(dir $@)
>python3 tools/mkpe_fixture.py --imports $@

$(USER_BUILD)/fixtures/malformed.exe: tools/mkpe_fixture.py
>@mkdir -p $(dir $@)
>python3 tools/mkpe_fixture.py --malformed $@

$(USER_BUILD)/fixtures/win32.exe: tools/mkpe_fixture.py
>@mkdir -p $(dir $@)
>python3 tools/mkpe_fixture.py --pe32 $@

# Raw compatibility/test binaries.

$(USER_BUILD)/tests/fault_user.bin: user/tests/fault_user.asm
>@mkdir -p $(dir $@)
>$(NASM) -f bin $< -o $@

$(USER_BUILD)/tests/hello_user.bin: user/tests/legacy/hello_user.asm
>@mkdir -p $(dir $@)
>$(NASM) -f bin $< -o $@

$(USER_BUILD)/tests/input_user.bin: user/tests/legacy/input_user.asm
>@mkdir -p $(dir $@)
>$(NASM) -f bin $< -o $@

USER_PROGRAMS := \
    $(USER_BUILD)/system/init.elf \
    $(USER_BUILD)/system/test-init.elf \
    $(USER_BUILD)/apps/shell.elf \
    $(USER_BUILD)/apps/hello.elf \
    $(USER_BUILD)/apps/native-smoke.obsx \
    $(USER_BUILD)/apps/desktop.obsx \
    $(USER_BUILD)/services/displayd.obsx \
    $(USER_BUILD)/system/serviced.obsx \
    $(USER_BUILD)/tests/fpu.elf \
    $(USER_BUILD)/tests/fs.elf \
    $(USER_BUILD)/tests/invalid.elf \
    $(USER_BUILD)/tests/ipc_client.elf \
    $(USER_BUILD)/tests/ipc_sender.elf \
    $(USER_BUILD)/tests/shm_client.elf \
    $(USER_BUILD)/tests/surface.elf \
    $(USER_BUILD)/tests/vm.elf \
    $(USER_BUILD)/tests/fault.elf \
    $(USER_BUILD)/fixtures/win-smoke.exe \
    $(USER_BUILD)/fixtures/imports.exe \
    $(USER_BUILD)/fixtures/malformed.exe \
    $(USER_BUILD)/fixtures/win32.exe

LEGACY_RAW_PROGRAMS := $(USER_BUILD)/tests/fault_user.bin $(USER_BUILD)/tests/hello_user.bin $(USER_BUILD)/tests/input_user.bin

# ============================================================
# Root filesystem / initrd
#
# We deliberately preserve the CURRENT flat names here so the
# existing kernel/userspace code continues to boot unchanged.
#
# Later, when Obsidia's filesystem layout becomes real, these
# can become /system, /apps, /tests, etc.
# ============================================================

.PHONY: rootfs

rootfs: $(USER_PROGRAMS)
>rm -rf $(ROOTFS_BUILD)
>mkdir -p $(ROOTFS_BUILD)
>cp -a rootfs/. $(ROOTFS_BUILD)/
>cp $(USER_BUILD)/system/init.elf       $(ROOTFS_BUILD)/init.elf
>cp $(USER_BUILD)/apps/shell.elf        $(ROOTFS_BUILD)/shell.elf
>cp $(USER_BUILD)/apps/hello.elf        $(ROOTFS_BUILD)/hello.elf
>cp $(USER_BUILD)/apps/native-smoke.obsx $(ROOTFS_BUILD)/native-smoke.obsx
>cp $(USER_BUILD)/apps/desktop.obsx     $(ROOTFS_BUILD)/desktop.obsx
>cp $(USER_BUILD)/services/displayd.obsx $(ROOTFS_BUILD)/displayd.obsx
>cp $(USER_BUILD)/system/serviced.obsx   $(ROOTFS_BUILD)/serviced.obsx
>cp $(USER_BUILD)/tests/fpu.elf         $(ROOTFS_BUILD)/fpu.elf
>cp $(USER_BUILD)/tests/fs.elf          $(ROOTFS_BUILD)/fs.elf
>cp $(USER_BUILD)/tests/invalid.elf     $(ROOTFS_BUILD)/invalid.elf
>cp $(USER_BUILD)/tests/ipc_client.elf  $(ROOTFS_BUILD)/ipc_client.elf
>cp $(USER_BUILD)/tests/ipc_sender.elf  $(ROOTFS_BUILD)/ipc_sender.elf
>cp $(USER_BUILD)/tests/shm_client.elf  $(ROOTFS_BUILD)/shm_client.elf
>cp $(USER_BUILD)/tests/surface.elf     $(ROOTFS_BUILD)/surface.elf
>cp $(USER_BUILD)/tests/vm.elf          $(ROOTFS_BUILD)/vm.elf
>cp $(USER_BUILD)/tests/fault.elf       $(ROOTFS_BUILD)/fault.elf
>cp $(USER_BUILD)/fixtures/win-smoke.exe $(ROOTFS_BUILD)/win-smoke.exe
>cp $(USER_BUILD)/fixtures/imports.exe  $(ROOTFS_BUILD)/imports.exe
>cp $(USER_BUILD)/fixtures/malformed.exe $(ROOTFS_BUILD)/malformed.exe
>cp $(USER_BUILD)/fixtures/win32.exe     $(ROOTFS_BUILD)/win32.exe

$(INITRD): rootfs tools/build_oar.py
>@mkdir -p $(dir $@)
>python3 tools/build_oar.py $(ROOTFS_BUILD) $(INITRD)

.PHONY: test-rootfs legacy-raw
test-rootfs: rootfs $(USER_BUILD)/system/test-init.elf
>rm -rf $(TEST_ROOTFS_BUILD)
>mkdir -p $(TEST_ROOTFS_BUILD)
>cp -a $(ROOTFS_BUILD)/. $(TEST_ROOTFS_BUILD)/
>cp $(USER_BUILD)/system/test-init.elf $(TEST_ROOTFS_BUILD)/init.elf

$(TEST_INITRD): test-rootfs tools/build_oar.py
>python3 tools/build_oar.py $(TEST_ROOTFS_BUILD) $(TEST_INITRD)

legacy-raw: $(LEGACY_RAW_PROGRAMS)
>@echo "Legacy raw fixtures built only; the production executable dispatcher rejects raw images."

# ============================================================
# Limine
# ============================================================

$(LIMINE_DIR):
>@mkdir -p third_party
>git clone https://github.com/limine-bootloader/limine.git \
>    --branch v7.x-binary \
>    --depth=1 \
>    $(LIMINE_DIR)

$(LIMINE_BIN): $(LIMINE_DIR)/limine.c
>$(MAKE) -C $(LIMINE_DIR) limine

# ============================================================
# ISO
# ============================================================

$(ISO): $(LIMINE_BIN) $(KERNEL) $(INITRD)
>rm -rf $(ISO_ROOT)
>mkdir -p $(ISO_ROOT)/boot/limine
>cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
>cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine/
>cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
>printf '%s\n' \
>    'TIMEOUT=0' \
>    '' \
>    ':Obsidia OS' \
>    '    PROTOCOL=limine' \
>    '    KERNEL_PATH=boot:///boot/kernel.elf' \
>    '    MODULE_PATH=boot:///boot/initrd.oar' \
>    '    MODULE_STRING=initrd.oar' \
>    '    RESOLUTION=1280x720' \
>    > $(ISO_ROOT)/boot/limine/limine.cfg
>cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
>cp $(INITRD) $(ISO_ROOT)/boot/initrd.oar
>xorriso -as mkisofs \
>    -b boot/limine/limine-bios-cd.bin \
>    -no-emul-boot \
>    -boot-load-size 4 \
>    -boot-info-table \
>    --efi-boot boot/limine/limine-uefi-cd.bin \
>    -efi-boot-part \
>    --efi-boot-image \
>    -o $(ISO) \
>    $(ISO_ROOT)
>$(LIMINE_BIN) bios-install $(ISO)

$(TEST_ISO): $(LIMINE_BIN) $(KERNEL) $(TEST_INITRD)
>rm -rf $(TEST_ISO_ROOT)
>mkdir -p $(TEST_ISO_ROOT)/boot/limine
>cp $(LIMINE_DIR)/limine-bios-cd.bin $(TEST_ISO_ROOT)/boot/limine/
>cp $(LIMINE_DIR)/limine-bios.sys $(TEST_ISO_ROOT)/boot/limine/
>cp $(LIMINE_DIR)/limine-uefi-cd.bin $(TEST_ISO_ROOT)/boot/limine/
>printf '%s\n' \
>    'TIMEOUT=0' \
>    '' \
>    ':Obsidia OS regression mode' \
>    '    PROTOCOL=limine' \
>    '    KERNEL_PATH=boot:///boot/kernel.elf' \
>    '    MODULE_PATH=boot:///boot/test-initrd.oar' \
>    '    MODULE_STRING=test-initrd.oar' \
>    '    RESOLUTION=1280x720' \
>    > $(TEST_ISO_ROOT)/boot/limine/limine.cfg
>cp $(KERNEL) $(TEST_ISO_ROOT)/boot/kernel.elf
>cp $(TEST_INITRD) $(TEST_ISO_ROOT)/boot/test-initrd.oar
>xorriso -as mkisofs \
>    -b boot/limine/limine-bios-cd.bin \
>    -no-emul-boot \
>    -boot-load-size 4 \
>    -boot-info-table \
>    --efi-boot boot/limine/limine-uefi-cd.bin \
>    -efi-boot-part \
>    --efi-boot-image \
>    -o $(TEST_ISO) \
>    $(TEST_ISO_ROOT)
>$(LIMINE_BIN) bios-install $(TEST_ISO)

# ============================================================
# Test disk
# ============================================================

$(DISK):
>@mkdir -p $(dir $@)
>dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
>@echo "Test disk image created: $@"

# Compatibility target for old commands such as:
#     make obsidia_disk.img
.PHONY: obsidia_disk.img
obsidia_disk.img: $(DISK)

# ============================================================
# Top-level targets
# ============================================================

.PHONY: all build iso run test-build test-run clean distclean

all: build

build: $(ISO)
>@echo
>@echo "Build complete:"
>@echo "  kernel: $(KERNEL)"
>@echo "  initrd: $(INITRD)"
>@echo "  ISO:    $(ISO)"

iso: $(ISO)

run: $(ISO) $(DISK)
>qemu-system-x86_64 \
>    -cdrom $(ISO) \
>    -serial stdio \
>    -drive file=$(DISK),format=raw,if=ide \
>    -m 256

test-build: $(TEST_ISO)
>@echo "Regression ISO: $(TEST_ISO)"

test-run: $(TEST_ISO) $(DISK)
>qemu-system-x86_64 \
>    -cdrom $(TEST_ISO) \
>    -serial stdio \
>    -drive file=$(DISK),format=raw,if=ide \
>    -m 256

clean:
>rm -rf $(BUILD_DIR)

distclean: clean
>rm -rf $(LIMINE_DIR)
