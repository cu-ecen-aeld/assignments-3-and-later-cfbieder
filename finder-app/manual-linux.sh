#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Changes done by Vishal Raj

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
#KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git #---Adding my part here
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
PROJECT_PATH=/AESD/Assignment-1,2,3/assignment-1-vishalraj3112

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

#---Adding my part here
if ! [ -d "${OUTDIR}" ]; then #directory could not be created, return error
	echo "Error: $OUTDIR Director could not be created"
	exit 1
fi
#---Adding my part here

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
    # Step 1: Clean tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # Step 2: Configure
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # Step 3: Build kernal image
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # Step 4: Build modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # Step 5: Build device tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir  "
#TODO: Copy the Image in outdir
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories

#---Adding my part here
mkdir ${OUTDIR}/rootfs

if ! [ -d "${OUTDIR}/rootfs" ]
then
	echo "Error: ${OUTDIR}/rootfs could not be created"
	exit 1
fi

cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib proc sbin sys tmp usr var lib64
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log
#cd ${OUTDIR}/rootfs
#sudo chown -R root:root *

#---Adding my part here

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

# TODO: Make and insatll busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs # My part

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
#cd ${OUTDIR}/rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp $SYSROOT/lib/ld-linux-aarch64.so.1 lib
cp $SYSROOT/lib64/libresolv.so.2 lib64
cp $SYSROOT/lib64/libm.so.6 lib64
cp $SYSROOT/lib64/libc.so.6 lib64


echo "Starting make devices step"
# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

echo "Starting writer app build step"
# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE} all

echo "Starting files copy step"
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
#mkdir ${OUTDIR}/rootfs/home
cp  ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
cp  ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -r ${FINDER_APP_DIR}/conf/ ${OUTDIR}/rootfs/home
cp  ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs
cp -f ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home

echo "Starting chown root directory step"
# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

echo "Starting create initramfs.cpio.gz step"
# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip -f initramfs.cpio
