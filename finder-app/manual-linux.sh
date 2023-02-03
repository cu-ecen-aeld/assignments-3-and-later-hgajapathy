#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
NPROC=$(nproc)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

# Create a directory outdir if it doesn’t exist.  Fail if the
# directory could not be created.
if [ ! -d ${OUTDIR} ]; then
    mkdir -p ${OUTDIR}
    if [ $? -ne 0 ]; then
        echo "Could not create directory '${OUTDIR}'"
        exit 1
    fi
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # Removes all intermediate files.
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # Create default configuration.
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # Compile kernel.
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # Build modules, we will install the modules to rootfs later.
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # Build device tree.
    make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

# echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var opt
mkdir usr/{bin,lib,sbin}
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
export SYSROOT=`${CROSS_COMPILE}gcc --print-sysroot`
cd ${OUTDIR}/rootfs
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib
cp ${SYSROOT}/lib64/ld-2.31.so lib64
cp ${SYSROOT}/lib64/libm.so.6 lib64
cp ${SYSROOT}/lib64/libm-2.31.so lib64
cp ${SYSROOT}/lib64/libresolv.so.2 lib64
cp ${SYSROOT}/lib64/libresolv-2.31.so lib64
cp ${SYSROOT}/lib64/libc.so.6 lib64
cp ${SYSROOT}/lib64/libc-2.31.so lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} clean
make -j${NPROC} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder-test.sh finder.sh writer autorun-qemu.sh ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/home/conf
cp -rf conf/* ${OUTDIR}/rootfs/home/conf

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip initramfs.cpio
