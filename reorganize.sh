cd ~/obsidia-os && bash <<'REORGANIZE'
set -euo pipefail

echo "=== Obsidia source-tree reorganization ==="

# ------------------------------------------------------------
# Sanity check
# ------------------------------------------------------------

if [[ ! -f Makefile || ! -d kernel || ! -d userland ]]; then
    echo "ERROR: This does not look like the expected Obsidia repo."
    exit 1
fi

# ------------------------------------------------------------
# Backup the parts we're about to restructure
# ------------------------------------------------------------

STAMP="$(date +%Y%m%d-%H%M%S)"
BACKUP="/tmp/obsidia-pre-reorg-${STAMP}.tar.gz"

echo "Creating backup:"
echo "  $BACKUP"

tar -czf "$BACKUP" \
    Makefile \
    userland \
    initrd \
    build_oar.py \
    2>/dev/null

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

move_file() {
    local src="$1"
    local dst="$2"

    if [[ ! -e "$src" ]]; then
        return
    fi

    if [[ -e "$dst" ]]; then
        echo "ERROR: Refusing to overwrite existing $dst"
        exit 1
    fi

    mkdir -p "$(dirname "$dst")"
    mv "$src" "$dst"

    echo "moved: $src -> $dst"
}

# ------------------------------------------------------------
# Create the new source layout
# ------------------------------------------------------------

mkdir -p \
    user/runtime \
    user/include/obsidia \
    user/lib \
    user/system/init \
    user/services/displayd \
    user/services/inputd \
    user/services/serviced \
    user/apps/hello \
    user/apps/shell \
    user/apps/desktop \
    user/apps/terminal \
    user/tests/legacy \
    rootfs \
    tools \
    third_party \
    build

# Keep currently-empty future source dirs visible to git.
touch \
    user/include/obsidia/.gitkeep \
    user/lib/.gitkeep \
    user/services/displayd/.gitkeep \
    user/services/inputd/.gitkeep \
    user/services/serviced/.gitkeep \
    user/apps/desktop/.gitkeep \
    user/apps/terminal/.gitkeep

# ------------------------------------------------------------
# Move userspace runtime
# ------------------------------------------------------------

move_file userland/crt0.asm   user/runtime/crt0.asm
move_file userland/stdint.h   user/runtime/stdint.h
move_file userland/syscall.h  user/runtime/syscall.h
move_file userland/user.ld    user/runtime/linker.ld

# ------------------------------------------------------------
# Move actual OS programs
# ------------------------------------------------------------

move_file userland/init.c       user/system/init/main.c
move_file userland/shell_user.c user/apps/shell/main.c
move_file userland/hello_elf.c  user/apps/hello/main.c

# ------------------------------------------------------------
# Move platform/kernel tests
# ------------------------------------------------------------

move_file userland/fpu_user.c     user/tests/fpu.c
move_file userland/fs_user.c      user/tests/fs.c
move_file userland/invalid_user.c user/tests/invalid.c
move_file userland/ipc_client.c   user/tests/ipc_client.c
move_file userland/ipc_sender.c   user/tests/ipc_sender.c
move_file userland/shm_client.c   user/tests/shm_client.c
move_file userland/surface_user.c user/tests/surface.c
move_file userland/vm_user.c      user/tests/vm.c
move_file userland/fault_user.asm user/tests/fault_user.asm

# Old/raw compatibility tests.
move_file userland/hello_user.asm   user/tests/legacy/hello_user.asm
move_file userland/hello_user_c.c   user/tests/legacy/hello_user_c.c
move_file userland/input_user.asm   user/tests/legacy/input_user.asm

# ------------------------------------------------------------
# Move static root-filesystem contents
# ------------------------------------------------------------

move_file initrd/foundation.txt rootfs/foundation.txt
move_file initrd/hello.txt      rootfs/hello.txt

# ------------------------------------------------------------
# Move build tooling
# ------------------------------------------------------------

move_file build_oar.py tools/build_oar.py

# ------------------------------------------------------------
# Move Limine out of the project root
# ------------------------------------------------------------

if [[ -d limine ]]; then
    if [[ -e third_party/limine ]]; then
        echo "ERROR: Both limine/ and third_party/limine exist."
        exit 1
    fi

    mv limine third_party/limine
    echo "moved: limine -> third_party/limine"
fi

# ------------------------------------------------------------
# Remove OLD generated outputs.
#
# Source has already been moved. Everything below should be
# reproducible by the new Makefile.
# ------------------------------------------------------------

find userland -maxdepth 1 -type f \
    \( -name '*.o' -o -name '*.elf' -o -name '*.bin' \) \
    -delete 2>/dev/null || true

# Do NOT delete userland if some unexpected source file remains.
if [[ -d userland ]]; then
    if [[ -z "$(find userland -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
        rmdir userland
    else
        echo
        echo "NOTE: userland/ contains files I did not recognize, so I left it intact:"
        find userland -maxdepth 1 -type f -printf '  %p\n'
        echo
    fi
fi

# Generated initrd contents are replaced by build/rootfs.
rm -rf initrd

# Old ISO staging tree was generated.
rm -rf iso

# Root-level generated build products.
rm -f ./*.o
rm -f kernel.elf
rm -f obsidia.iso
rm -f obsidia_disk.img
rm -f initrd.o
rm -f initrd.oar
rm -f initrd.tar

# Start from a clean build directory.
rm -rf build
mkdir -p build

# ------------------------------------------------------------
# Replace Makefile with clean source/build separation.
#
# kernel/      -> kernel source
# user/        -> userspace/OS source
# rootfs/      -> static OS filesystem contents
# build/       -> ALL generated output
# tools/       -> build tooling
# third_party/ -> external dependencies
# ------------------------------------------------------------

cat > Makefile <<'MAKEFILE'
.RECIPEPREFIX := >

CC      := gcc
LD      := ld
NASM    := nasm

BUILD_DIR    := build
KERNEL_BUILD := $(BUILD_DIR)/kernel
USER_BUILD   := $(BUILD_DIR)/user
ROOTFS_BUILD := $(BUILD_DIR)/rootfs
ISO_ROOT     := $(BUILD_DIR)/iso

KERNEL := $(BUILD_DIR)/kernel.elf
INITRD := $(BUILD_DIR)/initrd.oar
ISO    := $(BUILD_DIR)/obsidia.iso
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

$(USER_CRT0): user/runtime/crt0.asm
>@mkdir -p $(dir $@)
>$(NASM) -f elf64 $< -o $@

# Helper for freestanding ELF userspace programs.
define USER_ELF
$(USER_BUILD)/$(1).elf: $(2) $(USER_CRT0) user/runtime/linker.ld
>@mkdir -p $$(dir $$@)
>$(USER_CC) $(USER_CFLAGS) -c $(2) -o $(USER_BUILD)/$(1).o
>$(USER_LD) $(USER_LDFLAGS) $(USER_CRT0) $(USER_BUILD)/$(1).o -o $$@
>@rm -f $(USER_BUILD)/$(1).o
endef

# ============================================================
# Actual Obsidia userspace
# ============================================================

$(eval $(call USER_ELF,system/init,user/system/init/main.c))
$(eval $(call USER_ELF,apps/shell,user/apps/shell/main.c))
$(eval $(call USER_ELF,apps/hello,user/apps/hello/main.c))

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
    $(USER_BUILD)/apps/shell.elf \
    $(USER_BUILD)/apps/hello.elf \
    $(USER_BUILD)/tests/fpu.elf \
    $(USER_BUILD)/tests/fs.elf \
    $(USER_BUILD)/tests/invalid.elf \
    $(USER_BUILD)/tests/ipc_client.elf \
    $(USER_BUILD)/tests/ipc_sender.elf \
    $(USER_BUILD)/tests/shm_client.elf \
    $(USER_BUILD)/tests/surface.elf \
    $(USER_BUILD)/tests/vm.elf \
    $(USER_BUILD)/tests/fault_user.bin \
    $(USER_BUILD)/tests/hello_user.bin \
    $(USER_BUILD)/tests/input_user.bin

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
>cp $(USER_BUILD)/tests/fpu.elf         $(ROOTFS_BUILD)/fpu.elf
>cp $(USER_BUILD)/tests/fs.elf          $(ROOTFS_BUILD)/fs.elf
>cp $(USER_BUILD)/tests/invalid.elf     $(ROOTFS_BUILD)/invalid.elf
>cp $(USER_BUILD)/tests/ipc_client.elf  $(ROOTFS_BUILD)/ipc_client.elf
>cp $(USER_BUILD)/tests/ipc_sender.elf  $(ROOTFS_BUILD)/ipc_sender.elf
>cp $(USER_BUILD)/tests/shm_client.elf  $(ROOTFS_BUILD)/shm_client.elf
>cp $(USER_BUILD)/tests/surface.elf     $(ROOTFS_BUILD)/surface.elf
>cp $(USER_BUILD)/tests/vm.elf          $(ROOTFS_BUILD)/vm.elf
>cp $(USER_BUILD)/tests/fault_user.bin  $(ROOTFS_BUILD)/fault_user.bin
>cp $(USER_BUILD)/tests/hello_user.bin  $(ROOTFS_BUILD)/hello_user.bin
>cp $(USER_BUILD)/tests/input_user.bin  $(ROOTFS_BUILD)/input_user.bin

$(INITRD): rootfs tools/build_oar.py
>@mkdir -p $(dir $@)
>python3 tools/build_oar.py $(ROOTFS_BUILD) $(INITRD)

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

.PHONY: all build iso run clean distclean

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

clean:
>rm -rf $(BUILD_DIR)

distclean: clean
>rm -rf $(LIMINE_DIR)
MAKEFILE

# ------------------------------------------------------------
# Git ignore
# ------------------------------------------------------------

if ! grep -q '^# Obsidia reorganized build outputs$' .gitignore 2>/dev/null; then
    cat >> .gitignore <<'GITIGNORE'

# Obsidia reorganized build outputs
/build/
/third_party/limine/

# Legacy root-level generated files
/*.o
/kernel.elf
/obsidia.iso
/obsidia_disk.img
/initrd.oar
/initrd.tar
/iso/
GITIGNORE
fi

# ------------------------------------------------------------
# Show the new source layout
# ------------------------------------------------------------

echo
echo "=== New Obsidia source structure ==="
echo

find user rootfs tools \
    -maxdepth 4 \
    -type f \
    | sort \
    | sed 's/^/  /'

echo
echo "Backup remains available at:"
echo "  $BACKUP"
echo

# ------------------------------------------------------------
# Verify Makefile syntax + full clean build
# ------------------------------------------------------------

echo "=== Building reorganized tree ==="
make clean
make -j4

echo
echo "=== Checking git diff ==="
git diff --check

echo
echo "============================================================"
echo "REORGANIZATION COMPLETE"
echo
echo "Source:"
echo "  kernel/                  kernel"
echo "  user/runtime/            userspace runtime + syscall ABI"
echo "  user/system/             core Obsidia userspace"
echo "  user/services/           future long-running services"
echo "  user/apps/               applications"
echo "  user/tests/              kernel/platform regression tests"
echo "  rootfs/                  static filesystem contents"
echo "  tools/                   build tooling"
echo "  third_party/             external dependencies"
echo
echo "Generated:"
echo "  build/kernel/"
echo "  build/user/"
echo "  build/rootfs/"
echo "  build/kernel.elf"
echo "  build/initrd.oar"
echo "  build/obsidia.iso"
echo "  build/obsidia_disk.img"
echo
echo "Run with:"
echo "  make run"
echo
echo "Inspect changes with:"
echo "  git status"
echo "============================================================"
REORGANIZE
