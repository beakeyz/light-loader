#!/bin/bash

# utility for building the x86_64 toolchain (cross compiler really)

# set -e
trap 'handle_err' ERR

## settings lol

# for now this is the only one :clown: (thus also unused)
arch=x86
url=https://ftp.gnu.org/gnu
gcc_url=https://ftp.nluug.nl/languages/gcc/releases/gcc-10.1.0/


## end

# vars

# general exports and crap
export CC_PATH="$PWD/tools/cc/"
export TARGET="x86_64-pc-lightos"
# possible:
#           x86_64-elf          (64 bit mode apperantly)
#           i686-elf            (32 bit mode (?))
#           x86_64-pc-lightos   (patched 64 bit mode)
#           i686-pc-lightos     (?)

patch_path="$PWD/tools/"

# binutils
bin_utils_version="2.33.1"
bin_utils_dir="binutils-$bin_utils_version"
binutils_file="binutils-$bin_utils_version.tar.xz"
export BINUTILS_SRC="$CC_PATH/src/$bin_utils_dir"

# gcc
gcc_version="10.1.0"
gcc_dir="gcc-$gcc_version"
gcc_file="gcc-$gcc_version.tar.xz"
export GCC_SRC="$CC_PATH/src/$gcc_dir"


handle_err () {
    panic "Error occurred, exititing..."
}

# hihi wonky color codes
log () {
    echo -e "\e[36m[\e[0m \e[1;36mLog\e[0m \e[36m]\e[0m: $1"
}

warn () {
    echo -e "\e[31m[\e[0m \e[1;31mWarn\e[0m \e[31m]\e[0m: $1"
}

panic () {
    echo -e "[Panic] \e[31m $1 \e[0m"
    exit 0
}

# TODO: download deps automaticly
setup_src () {
    log "Downloading $2 src . . ."
    wget -c "$1"
    log "Extracting $2"
    tar xf "$2"
}

# params:
# 0: path to src
# 1: patchfile
_patch () {
    log "Patching $1"
    cd $1
    patch -p1 < "$patch_path/$2"
    cd ..
}

_install_suite () {
  log "Installing..."
  sudo cp $CC_PATH/bin/x86_64-pc-lightos-gcc ~/.local/bin
  sudo cp $CC_PATH/bin/x86_64-pc-lightos-ld ~/.local/bin 
  sudo cp $CC_PATH/bin/x86_64-pc-lightos-nm ~/.local/bin
  sudo cp $CC_PATH/bin/x86_64-pc-lightos-objdump ~/.local/bin 
  sudo cp $CC_PATH/bin/x86_64-pc-lightos-as ~/.local/bin 
  sudo cp $CC_PATH/libexec/gcc/x86_64-pc-lightos/$gcc_version/cc1 ~/.local/bin
  log "Installed the suite to ~/.local/bin !"
}

### start of the script ###

log "Welcome to the light-loader cc build =D"
log "TODO: instead of building a new cc for every project, can't we just install a suite on the system? -_-"

if [[ $PATH != *"$CC_PATH/bin"* ]]; then
    export PATH="$CC_PATH/bin:$PATH"
else
    warn "binaries are already in PATH variable"
fi

if [[ -d $CC_PATH ]]
then
    panic "It seems like the crosscompiler suite has already been installed. To reinstall, remove the 'cross_compiler' directory from the project"
fi

log "Creating cc build dir..."
mkdir -p "$CC_PATH/build/gcc" "$CC_PATH/build/binutils"
mkdir -p "$CC_PATH/src"

cd "$CC_PATH/src"

# step 0: download and extract sources
# binutils
setup_src "$url/binutils/$binutils_file" "$binutils_file"

# gcc
setup_src "$gcc_url/$gcc_file" "$gcc_file"

# step 1: apply patches
read -p "[?] Do you want to patch this cc build? [YES/no]: " do_patch

if [[ "$do_patch" == "yes" ]]; then 
  _patch "$BINUTILS_SRC" "binutils.patch"
  _patch "$GCC_SRC" "gcc.patch"
else
  if [[ -z $do_patch ]]; then
    _patch "$BINUTILS_SRC" "binutils.patch"
    _patch "$GCC_SRC" "gcc.patch"
  fi
fi

# gcc prerequisites
log "Fixing gcc prerequisites"
cd "$gcc_dir"
./contrib/download_prerequisites
cd ..

# step 2: build the binaries
# binutils
cd ..
cd build

log "Configuring binutils"
cd binutils
"$BINUTILS_SRC/configure" --target="$TARGET" 	\
        --prefix="$CC_PATH" 	\
        --disable-nls 		\
        --disable-werror     \
        --enable-shared       \

log "Started making binutils"
make -j$(nproc)
make install -j$(nproc)
cd ..

log "Done! Starting gcc . . ."
# gcc
log "Configuring gcc"
cd gcc
"$GCC_SRC/configure" --target="$TARGET" \
        --prefix="$CC_PATH" 		\
        --disable-nls			\
        --enable-languages=c	\
        --enable-shared \

log "Making gcc . . ."
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc -j$(nproc)
make install-target-libgcc -j$(nproc)

log "Warning: some of the buildsystem requires you to install the compiler, so installing is strongly encouraged!"
read -p "[?] Do you want to install this compiler on your system? [YES/no]: " do_patch

if [[ "$do_patch" == "yes" ]]; then 
  _install_suite
else
  if [[ -z $do_patch ]]; then
    _install_suite
  fi
fi

log "Done =D"
cd ..

# step 3: configure and install
