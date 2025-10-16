#!/bin/bash
# Simple GDB launcher selecting architecture & kernel ELF
set -e

ARCH="$1"    # i386 | x86_64
ELF="$2"     # path to ELF

# =====================
# SKIP LIST (edit here)
# Boşlukla ayrılmış fonksiyon isimleri. Örn: "memcpy memset printk"
SKIP_FUNCTIONS="memcpy inb outb inw outw inl outl vprintf uart_open uart_close uart_write_char uart_write_string uart_print uart_printf"
# Ortamdan geçmek isterseniz şu değişkenlerden biri ile override edebilirsiniz:
#   SKIP_FUNCTIONS, SKIP_FUNCS veya GDB_SKIP_FUNCS
# =====================

# Ortam override çöz
SKIP_FUNCTIONS="${SKIP_FUNCTIONS:-${SKIP_FUNCS:-${GDB_SKIP_FUNCS:-}}}"

if [ -z "$ARCH" ] || [ -z "$ELF" ]; then
    echo "Usage: $0 <arch> <elf>" >&2
    echo "  arch: i386 | x86_64" >&2
    echo "  elf : path to kernel ELF" >&2
    echo "Not: Skip edilecek fonksiyonları bu dosyanın başındaki SKIP_FUNCTIONS değişkenine yazın" >&2
    exit 1
fi

pick_gdb() {
    if command -v gdb-multiarch >/dev/null 2>&1; then
        echo gdb-multiarch
    elif command -v ${ARCH}-elf-gdb >/dev/null 2>&1; then
        echo ${ARCH}-elf-gdb
    else
        echo gdb
    fi
}

GDB_BIN="$(pick_gdb)"
case "$ARCH" in
    i386) GDB_ARCH="i386" ;;
    x86_64) GDB_ARCH="i386:x86-64" ;;
    *) echo "Unknown arch: $ARCH" >&2; exit 2 ;;
esac

chmod +x "$0" 2>/dev/null || true

# Build GDB -ex sequence
GDB_ARGS=(
    -ex "set architecture $GDB_ARCH"
    -ex "target remote localhost:1234"
    -ex "symbol-file $ELF"
    -ex "break _start"
    -ex "layout split"
)

# Add skip rules if provided (space-separated)
if [ -n "$SKIP_FUNCTIONS" ]; then
    for fn in $SKIP_FUNCTIONS; do
        GDB_ARGS+=( -ex "skip function $fn" )
    done
fi

echo "[GDB] Using $GDB_BIN (arch=$GDB_ARCH, elf=$ELF)"
if [ -n "$SKIP_FUNCTIONS" ]; then
    echo "[GDB] Skipping functions: $SKIP_FUNCTIONS"
fi

exec "$GDB_BIN" "${GDB_ARGS[@]}"
