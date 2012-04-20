#! /system/bin/sh

WIFION=`getprop init.svc.wpa_supplicant`

case "$WIFION" in
  "running") echo " ********************************************************"
             echo " * Turn Wi-Fi OFF and launch the script for calibration *"
             echo " ********************************************************"
             exit;;
          *) echo " ******************************"
             echo " * Starting Wi-Fi calibration *"
             echo " ******************************";;
esac

TARGET_FW_DIR=/system/etc/firmware/ti-connectivity
TARGET_NVS_FILE=$TARGET_FW_DIR/wl1271-nvs.bin
TARGET_INI_FILE=/system/etc/wifi/TQS_D_1.7.ini
WL12xx_MODULE=/system/lib/modules/wl12xx_sdio.ko

# Remount system partition as rw
mount -o remount rw /system

# Remove old NVS file
rm /system/etc/firmware/ti-connectivity/wl1271-nvs.bin

# Actual calibration...
# calibrator plt autocalibrate <dev> <module path> <ini file1> <nvs file> <mac addr>
# Leaving mac address field empty for random mac
calibrator plt autocalibrate wlan0 $WL12xx_MODULE $TARGET_INI_FILE $TARGET_NVS_FILE

echo " ******************************"
echo " * Finished Wi-Fi calibration *"
echo " ******************************"
