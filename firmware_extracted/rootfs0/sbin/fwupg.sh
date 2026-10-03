#!/bin/sh

USAGE="Usage: fwupg <temp_partition> <rootfs_backup_partition> <kernel_backup_partition> <rootfs_filename> <kernel_filename>"

if [ $# -lt 5 ]; then
	echo ${USAGE}
	exit 1;
fi

TEMP_PARTITION=$1
ROOTFS_BACKUP=$2
KERNEL_BACKUP=$3
ROOTFS_FILE=$4
KERNEL_FILE=$5

echo ${TEMP_PARTITION}
echo ${ROOTFS_BACKUP}
echo ${KERNEL_BACKUP}
echo ${ROOTFS_FILE}
echo ${KERNEL_FILE}


if [ ! -b ${TEMP_PARTITION} ]; then
	echo "${TEMP_PARTITION} does not exist."
	echo ${USAGE}
	exit 1;
fi

if [ ! -b ${ROOTFS_BACKUP} ]; then
	echo "${ROOTFS_BACKUP} does not exist."
	echo ${USAGE}
	exit 1;
fi

if [ ! -b ${KERNEL_BACKUP} ]; then
	echo "${KERNEL_BACKUP} does not exist."
	echo ${USAGE}
	exit 1;
fi

MODULE_PATH="/lib/modules/`uname -r`/kernel/drivers/usb/gadget"
GADGET_DRIVER="bcm28xx_udc2"
FILE_STORAGE_DRIVER="g_file_storage"
FILE_STORAGE_ARGS="file=${TEMP_PARTITION} removable=y stall=n vendor=0x0a5c product=0x2820 buflen=65536"

if [ ! -f ${MODULE_PATH}/${GADGET_DRIVER} ]; then
   echo "Can't found USB driver!"
   exit 1;        
fi        

MOUNT_POINT="/tmp/media2820"
FIRMWARE_DIR="2820_FWUPG"

if [ ! -d ${MOUNT_POINT} ]; then
	mkdir ${MOUNT_POINT}
fi

echo "Cleaning mount point..."
mount ${TEMP_PARTITION} ${MOUNT_POINT}
rm -rf ${MOUNT_POINT}/*
mkdir ${MOUNT_POINT}/${FIRMWARE_DIR}
umount ${MOUNT_POINT}

insmod ${MODULE_PATH}/${GADGET_DRIVER}.ko
insmod ${MODULE_PATH}/${FILE_STORAGE_DRIVER}.ko ${FILE_STORAGE_ARGS}

echo "Please connect device to PC using USB cable and copy ${KERNEL_FILE} and ${ROOTFS_FILE} to ${FIRMWARE_DIR}"

read key

echo "Proceeding with firmware upgrade."
mount ${TEMP_PARTITION} ${MOUNT_POINT}
dd if=${MOUNT_POINT}/${FIRMWARE_DIR}/${KERNEL_FILE} of=${KERNEL_BACKUP}
dd if=${MOUNT_POINT}/${FIRMWARE_DIR}/${ROOTFS_FILE} of=${ROOTFS_BACKUP}
umount ${MOUNT_POINT}

rmmod ${FILE_STORAGE_DRIVER}
rmmod ${GADGET_DRIVER}

echo "Done..."
echo "Kindly reboot and modify nvconfig parameters for changes to take effect."
