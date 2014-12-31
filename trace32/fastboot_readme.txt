
fastboot is Android tool

fbtool is a tool develop by mediatek. 
fbtool/src folder contains source code. You can build by yourself.
fbtool/windows have a prebuilt windows version
fbtool/fbtool-8135-da and fbtool/fbtool-8173-da contains binaries that needed when write empty flash

Usage:
Use fastboot write partition (for examle, boot.img)

1. How to enter fastboot mode (choose a or b)
   a. If empty flash
       Host run command: 
       		fbtool fbtool-da-8135/fbtool-da-pl.bin fbtool-da-8135/fbtool-da-lk.bin
       Power on platform and plugin USB cable
       Then mt8173 start showing log, and enter fastboot mode
            Log shows fastboot_init()
                        fastboot: processing commands: fastboot_mode=2
	Sometimes fbtool will hang, this is due to Host USB driver, not tool.
    If you see mt8173 already enter fastboot mode, you can force stop fbtool (Ctrl + C)

  b. If flash not empty and can run full lk
       Host run command: 
       		fastboot getvar all
       Plug-in USB cable and reset mt8173 platform => then mt8173 enter fastboot mode

2. Write image
       All partitions: run script (windows version, you can change to linux script by yourself)
           # 8173_flashall.bat
       Specific partition:
           # fastboot flash boot 8173_loader/boot.img

