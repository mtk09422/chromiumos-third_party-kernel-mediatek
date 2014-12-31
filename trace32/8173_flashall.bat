fastboot flash partition 8173_loader\MBR
fastboot flash boot0 8173_loader\preloader.img
fastboot flash UBOOT 8173_loader\lk.bin
fastboot flash boot 8173_loader\boot.img
fastboot flash TEE1 8173_loader\trustzone.bin
fastboot flash TEE2 8173_loader\trustzone.bin
