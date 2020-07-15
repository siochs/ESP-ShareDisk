#!/bin/bash

if [ ! -f "src/secrets.hpp" ]
then
    echo "Create src/secrets.hpp and retry."
    echo "See also src/secrets.hpp.template"
    exit -1
fi

python3 -m venv venv
. ./venv/bin/activate
pip3 install -U platformio
platformio run --target clean
platformio run
echo .
echo "Done. Your firmware is located here:"
find .pio/build/ -name *firmware*
echo .
echo "Hit ENTER if you wish to upload the firmware"
read
ls /dev
read -p "Which device is your serial port? /dev/" device

while true
do 
    read -p "Erase Flash? (y/n)? " yn
    case $yn in
        "y") 
            platformio run --target erase --upload-port "/dev/$device"
            break
            ;;
        "n") 
            break
            ;;
        * ) echo "Answer y or n";;
    esac
done
platformio run --target uploadfs --upload-port "/dev/$device"
platformio run --target upload --target monitor --upload-port "/dev/$device"