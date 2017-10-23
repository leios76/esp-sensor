#!/bin/sh

ARDUINO_IDE=/home/leios/다운로드/arduino-1.8.4/arduino
VERSION=$(git describe --tags)
PROJECT=esp-sensor

rm -Rf build && mkdir -p build && ${ARDUINO_IDE} --verify --board esp8266com:esp8266:d1_mini:FlashSize=4M1M --pref build.path=./build ${PROJECT}.ino && mv build/${PROJECT}.ino.bin ./${PROJECT}_${VERSION}_4M.bin

rm -Rf build && mkdir -p build && ${ARDUINO_IDE} --verify --board esp8266com:esp8266:esp-wroom-02:FlashSize=2M --pref build.path=./build ${PROJECT}.ino && mv build/${PROJECT}.ino.bin ./${PROJECT}_${VERSION}_2M.bin

rm -Rf build && mkdir -p build && ${ARDUINO_IDE} --verify --board esp8266com:esp8266:d1_mini_lite:FlashSize=1M256 --pref build.path=./build ${PROJECT}.ino && mv build/${PROJECT}.ino.bin ./${PROJECT}_${VERSION}_1M.bin

scp -P 2222 ${PROJECT}_${VERSION}_*.bin root@nor.kr:/mnt/coreos1/iot/var/www/iot/esp/firmware/

