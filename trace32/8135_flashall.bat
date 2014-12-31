fastboot flash partition 8135_loader\MBR
fastboot flash boot0 8135_loader\preloader.img
fastboot flash UBOOT 8135_loader\lk.bin
fastboot flash boot 8135_loader\boot.img
fastboot flash TEE1 8135_loader\tz.img
fastboot flash TEE2 8135_loader\tz.img
