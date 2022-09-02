#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aesd-autograder
#KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

echo "FInder Directory is: "$FINDER_APP_DIR
echo "Outdir Directory is: "$OUTDIR

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

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
			make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
			make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
			make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
			make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
			make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs



fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
		make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
		make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

#sudo

# TODO: Add library dependencies to rootfs
cd ${OUTDIR}/rootfs
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"


# TO DELETE IF NOT NEEDED
export SROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)
cp -L $SROOT/lib/ld-linux-aarch64.* lib
cp -L $SROOT/lib64/libm.so.* lib64
cp -L $SROOT/lib64/libresolv.so.* lib64
cp -L $SROOT/lib64/libc.so.* lib64



# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1
#mount -t proc proc /proc
#mount -t sysfs sysfs /sys

# TODO: Clean and build the writer utility
echo "Building writer utility"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copying files"
cp  ./finder-test.sh ${OUTDIR}/rootfs/home
cp  ./finder.sh ${OUTDIR}/rootfs/home
cp  ./writer.sh ${OUTDIR}/rootfs/home
cp  ./writer.c ${OUTDIR}/rootfs/home
cp  ./writer ${OUTDIR}/rootfs/home
cp  ./Makefile ${OUTDIR}/rootfs/home
cp -r ./conf/ ${OUTDIR}/rootfs/home
cp  ./autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio
mkimage -A arm -O linux -T ramdisk -d initramfs.cpio.gz uRamdisk
#cp {$OUTDIR}/linux-stable/arch/arm64/boot/Image .
