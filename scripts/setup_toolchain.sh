#!/bin/bash
# AtomOS Development Environment Setup Script
# Installs all required dependencies and cross-compilers

set -e  # Exit on any error

PROJECT_NAME="AtomOS"
CROSS_PREFIX="/opt/cross-compiler"
BINUTILS_VERSION="2.42"
GCC_VERSION="13.2.0"

# --- DEĞİŞİKLİK 1: Renk kodları kaldırıldı ---
# --- DEĞİŞİKLİK 2: Log fonksiyonları standart hataya (stderr) yönlendirildi ---
log_info() {
    echo "[INFO] $1" >&2
}

log_error() {
    echo "[ERROR] $1" >&2
}

log_warn() {
    echo "[WARN] $1" >&2
}

log_build() {
    echo "[BUILD] $1" >&2
}

# Detect Linux distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        log_error "Cannot detect Linux distribution"
        exit 1
    fi
    log_info "Detected: $PRETTY_NAME"
}

# Install system dependencies
install_dependencies() {
    log_info "Installing system dependencies..."
    case "$DISTRO" in
        "ubuntu"|"debian")
            sudo apt update
            sudo apt install -y \
                build-essential nasm qemu-system-x86 grub-pc-bin grub-efi-amd64-bin \
                grub-efi-ia32-bin xorriso ovmf wget curl git make texinfo bison flex \
                libgmp3-dev libmpc-dev libmpfr-dev libisl-dev libzstd-dev
            ;;
        "fedora"|"rhel"|"centos")
            sudo dnf groupinstall -y "Development Tools"
            sudo dnf install -y \
                nasm qemu-system-x86 grub2-pc grub2-efi-x64 grub2-efi-ia32 xorriso \
                edk2-ovmf wget curl git make texinfo bison flex gmp-devel \
                libmpc-devel mpfr-devel isl-devel
            ;;
        "arch"|"manjaro")
            sudo pacman -Syu --noconfirm
            sudo pacman -S --noconfirm --needed \
                base-devel nasm qemu-system-x86 grub efibootmgr xorriso ovmf \
                wget curl git make texinfo bison flex gmp libmpc mpfr isl
            ;;
        *)
            log_error "Unsupported distribution: $DISTRO"
            exit 1
            ;;
    esac
    log_info "System dependencies installed successfully"
}

# Check if cross-compiler already exists
check_existing_toolchain() {
    local i686_gcc="$CROSS_PREFIX/bin/i686-elf-gcc"
    local x86_64_gcc="$CROSS_PREFIX/bin/x86_64-elf-gcc"
    if [ -f "$i686_gcc" ] && [ -f "$x86_64_gcc" ]; then
        log_info "Cross-compilers already exist at $CROSS_PREFIX"
        if "$i686_gcc" --version >/dev/null 2&>1 && "$x86_64_gcc" --version >/dev/null 2>&1; then
            log_info "Existing cross-compilers are functional"
            read -p "Do you want to rebuild them? (y/N) " response
            if [[ ! "$response" =~ ^[Yy]$ ]]; then
                log_info "Skipping cross-compiler build"
                return 0
            fi
        fi
    fi
    return 1
}

# Download package with robust fallback mechanism
download_package() {
    local package_name=$1
    local version=$2
    local cache_file=$3

    if [ -f "$cache_file" ]; then
        log_info "Using cached $package_name: $cache_file"
        return 0
    fi

    local urls=()
    if [ "$package_name" = "binutils" ]; then
        urls=(
            "https://ftp.gnu.org/gnu/binutils/binutils-$version.tar.xz"
            "https://mirrors.kernel.org/gnu/binutils/binutils-$version.tar.xz"
            "https://sourceware.org/pub/binutils/releases/binutils-$version.tar.xz"
            "https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.xz"
        )
    elif [ "$package_name" = "gcc" ]; then
        urls=(
            "https://ftp.gnu.org/gnu/gcc/gcc-$version/gcc-$version.tar.xz"
            "https://mirrors.kernel.org/gnu/gcc/gcc-$version/gcc-$version.tar.xz"
            "https://ftp.gnu.org/gnu/gcc/gcc-13.1.0/gcc-13.1.0.tar.xz"
        )
    fi

    log_build "Auto-trying multiple sources for $package_name $version..."
    for url in "${urls[@]}"; do
        log_info "Trying: $url"
        if wget --no-check-certificate --timeout=30 -O "$cache_file.tmp" "$url" 2>/dev/null; then
            mv "$cache_file.tmp" "$cache_file"
            log_info "Successfully downloaded from: $url"
            return 0
        else
            log_warn "Failed: $url"
            rm -f "$cache_file.tmp"
        fi
    done

    log_error "Failed to download $package_name from all sources"
    return 1
}

# Download source code
download_sources() {
    local cache_dir="$CROSS_PREFIX/src-cache"
    local build_dir="/tmp/AtomOS-toolchain-build-$$"

    log_info "Creating cache directory: $cache_dir"
    sudo mkdir -p "$cache_dir"
    sudo chown "$(whoami)":"$(id -gn)" "$cache_dir"

    log_info "Creating temporary build directory: $build_dir"
    mkdir -p "$build_dir"

    local binutils_file="$cache_dir/binutils-$BINUTILS_VERSION.tar.xz"
    if ! download_package "binutils" "$BINUTILS_VERSION" "$binutils_file"; then
        exit 1
    fi

    local gcc_file="$cache_dir/gcc-$GCC_VERSION.tar.xz"
    if ! download_package "gcc" "$GCC_VERSION" "$gcc_file"; then
        exit 1
    fi
    
    # Bu fonksiyonun tek çıktısı build_dir olmalı
    echo "$build_dir"
}

# Build cross-compiler for a specific target
build_cross_compiler() {
    local target=$1
    local build_dir=$2
    
    log_build "Building cross-compiler for $target..."
    
    # Derleme işlemleri için doğru dizine geç
    cd "$build_dir"

    log_info "Extracting binutils..."
    tar -xf "$cache_dir/binutils-$BINUTILS_VERSION.tar.xz"
    
    log_info "Extracting GCC..."
    tar -xf "$cache_dir/gcc-$GCC_VERSION.tar.xz"

    # Build binutils
    log_build "Building binutils for $target..."
    mkdir -p "build-binutils-$target"
    cd "build-binutils-$target"
    ../binutils-$BINUTILS_VERSION/configure --target="$target" --prefix="$CROSS_PREFIX" --with-sysroot --disable-nls --disable-werror
    make -j"$(nproc)"
    sudo make install
    cd ..

    # Build GCC
    log_build "Building GCC for $target..."
    mkdir -p "build-gcc-$target"
    cd "build-gcc-$target"
    ../gcc-$GCC_VERSION/configure --target="$target" --prefix="$CROSS_PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
    make -j"$(nproc)" all-gcc
    make -j"$(nproc)" all-target-libgcc
    sudo make install-gcc
    sudo make install-target-libgcc
    cd ..
}

# Build both cross-compilers
build_toolchain() {
    if check_existing_toolchain; then return 0; fi

    log_info "Building cross-compilation toolchain..."
    log_warn "This may take 30-60 minutes depending on your system"
    
    sudo mkdir -p "$CROSS_PREFIX"
    sudo chown "$(whoami)":"$(id -gn)" "$CROSS_PREFIX"
    
    local cache_dir="$CROSS_PREFIX/src-cache"
    sudo mkdir -p "$cache_dir"
    
    local build_dir
    build_dir=$(download_sources)
    
    # Önbellek dizinini ve derleme dizinini kopyala
    local final_build_dir=$build_dir
    cp "$cache_dir/binutils-$BINUTILS_VERSION.tar.xz" "$final_build_dir/"
    cp "$cache_dir/gcc-$GCC_VERSION.tar.xz" "$final_build_dir/"
    
    build_cross_compiler "i686-elf" "$final_build_dir" "$cache_dir"
    build_cross_compiler "x86_64-elf" "$final_build_dir" "$cache_dir"
    
    log_info "Cleaning up temporary build directory..."
    rm -rf "$final_build_dir"
}

# Update PATH in shell rc files
update_path() {
    local shell_rc=""
    case "$SHELL" in */bash) shell_rc="$HOME/.bashrc" ;; */zsh) shell_rc="$HOME/.zshrc" ;; esac
    
    if [ -z "$shell_rc" ]; then
        log_warn "Could not detect shell rc file. Please add $CROSS_PREFIX/bin to your PATH manually."
        return
    fi

    if grep -q "AtomOS Cross-Compiler" "$shell_rc"; then
        log_info "PATH already contains cross-compiler directory."
    else
        log_info "Adding cross-compiler to PATH in $shell_rc"
        echo -e "\n# AtomOS Cross-Compiler\nexport PATH=\"$CROSS_PREFIX/bin:\$PATH\"" >> "$shell_rc"
        log_info "PATH updated. Please restart your shell or run 'source $shell_rc'"
    fi
    export PATH="$CROSS_PREFIX/bin:$PATH"
}

# Verify installation
verify_installation() {
    log_info "Verifying installation..."
    export PATH="$CROSS_PREFIX/bin:$PATH"
    
    if command -v i686-elf-gcc &>/dev/null && command -v x86_64-elf-gcc &>/dev/null; then
        log_info "All cross-compilers found in PATH."
        log_info "i686-elf-gcc: $(i686-elf-gcc --version | head -n1)"
        log_info "x86_64-elf-gcc: $(x86_64-elf-gcc --version | head -n1)"
        log_info "Setup complete! You can now build AtomOS."
    else
        log_error "Cross-compilers not found in PATH. Please check the installation."
        return 1
    fi
}

main() {
    detect_distro
    install_dependencies
    build_toolchain
    update_path
    verify_installation
}

main "$@"