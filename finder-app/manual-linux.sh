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

    cp ${FINDER_APP_DIR}/dtc-multiple-definition.patch ./
    

    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper  
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig      
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir  rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp -L $FINDER_APP_DIR/reqlibs/ld-linux-aarch64.so.1  ./lib64/
cp -L $FINDER_APP_DIR/reqlibs/ld-linux-aarch64.so.1 ./lib/
cp -L $FINDER_APP_DIR/reqlibs/libm.so.6 ./lib64/
cp -L $FINDER_APP_DIR/reqlibs/libresolv.so.2 ./lib64/
cp -L $FINDER_APP_DIR/reqlibs/libc.so.6 ./lib64/

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1


# TODO: Clean and build the writer utility  
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

cp --parents writer conf/username.txt finder.sh finder-test.sh autorun-qemu.sh ${OUTDIR}/rootfs/home/
  
# TODO: Chown the root directory
cd "$OUTDIR/rootfs"
sudo chown -R root:root *
# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ../initramfs.cpio
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image $OUTDIR/ 