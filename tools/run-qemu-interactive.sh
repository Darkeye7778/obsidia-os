#!/usr/bin/env bash
set -euo pipefail

mode=${1:-auto}
iso=${2:-build/obsidia.iso}
disk=${3:-build/obsidia_disk.img}
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
iso=$(realpath -- "$repo_root/$iso")
disk=$(realpath -- "$repo_root/$disk")

case "$mode" in
    auto|linux|windows) ;;
    *) echo "usage: $0 {auto|linux|windows} [iso] [disk]" >&2; exit 2 ;;
esac

is_wsl=0
if [[ -n ${WSL_DISTRO_NAME:-} ]] || grep -qi microsoft /proc/sys/kernel/osrelease 2>/dev/null; then
    is_wsl=1
fi

find_windows_qemu() {
    local configured=${OBSIDIA_QEMU_WINDOWS:-${QEMU_WINDOWS:-}}
    local candidate=
    if [[ -n $configured ]]; then
        if [[ $configured == [A-Za-z]:\\* ]]; then wslpath -u "$configured"; else printf '%s\n' "$configured"; fi
        return
    fi
    if command -v cmd.exe >/dev/null 2>&1; then
        candidate=$(cd /mnt/c/Windows && cmd.exe /d /c where qemu-system-x86_64.exe 2>/dev/null | tr -d '\r' | head -n 1 || true)
        if [[ -n $candidate ]]; then wslpath -u "$candidate"; return; fi
    fi
    for candidate in "/mnt/c/Program Files/qemu/qemu-system-x86_64.exe" "/mnt/c/Program Files (x86)/qemu/qemu-system-x86_64.exe"; do
        if [[ -x $candidate ]]; then printf '%s\n' "$candidate"; return; fi
    done
}

run_windows() {
    local qemu_exe iso_windows disk_windows display
    qemu_exe=$(find_windows_qemu)
    if [[ -z $qemu_exe || ! -x $qemu_exe ]]; then
        echo "Windows-native QEMU was not found." >&2
        echo "Install QEMU for Windows, add it to PATH, or set OBSIDIA_QEMU_WINDOWS." >&2
        return 1
    fi
    iso_windows=$(wslpath -w "$iso")
    disk_windows=$(wslpath -w "$disk")
    display=${OBSIDIA_QEMU_WINDOWS_DISPLAY:-sdl,gl=off}
    echo "Interactive backend: Windows-native $qemu_exe ($display), PS/2 mouse"
    exec "$qemu_exe" -machine pc -display "$display" -cdrom "$iso_windows" -serial stdio \
        -drive "file=$disk_windows,format=raw,if=ide" -m 256
}

run_linux() {
    local qemu_exe=${OBSIDIA_QEMU_LINUX:-qemu-system-x86_64}
    local display=${OBSIDIA_QEMU_LINUX_DISPLAY:-gtk,gl=off}
    if (( is_wsl )); then
        echo "Interactive backend: Linux $qemu_exe via WSLg XWayland ($display), PS/2 mouse"
        exec env GDK_BACKEND=${OBSIDIA_GDK_BACKEND:-x11} "$qemu_exe" -machine pc -display "$display" \
            -cdrom "$iso" -serial stdio -drive "file=$disk,format=raw,if=ide" -m 256
    fi
    echo "Interactive backend: Linux $qemu_exe ($display), PS/2 mouse"
    exec "$qemu_exe" -machine pc -display "$display" -cdrom "$iso" -serial stdio \
        -drive "file=$disk,format=raw,if=ide" -m 256
}

if [[ $mode == windows ]]; then
    (( is_wsl )) || { echo "run-windows must be invoked from WSL." >&2; exit 1; }
    run_windows
elif [[ $mode == linux ]]; then
    run_linux
elif (( is_wsl )) && [[ -n $(find_windows_qemu) ]]; then
    run_windows
else
    if (( is_wsl )); then
        echo "Windows QEMU not installed; using the explicit XWayland fallback."
    fi
    run_linux
fi
