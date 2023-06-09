# AtomS3 TC70 Control

This project demonstrates TP-Link TC70 and AtomS3 synchronization.

<div><video controls src="./onvif_sync.mp4" muted="false"></video></div>

## Usage

1. Supply power to AtomS3 and wait for convergence of the madgwick filter.
2. Set AtomS3 designated posture (See the movie)
3. Press on the button to start synchronization.

## Behavior

AtomS3 communicates with TC70 through ONVIF protocol and controls Pan/Tilt. AtomS3 uses embedded IMU and madgwick filter to detect its own posture.

FYI: This project wouldn't work with generic ONVIF cameras because it doesn't handle XML namespaces properly.

## Notes

* Please set up TC70 with tapo app.
* Please modify Wifi and TC70 information in main.cpp.

## Supported Hardware

* [TP-Link TC70](https://www.tp-link.com/en/home-networking/cloud-camera/tc70/)
* [M5Stack AtomS3](https://docs.m5stack.com/en/core/AtomS3)

## Build Environment

* PlatformIO

## Dependencies

* m5stack/M5AtomS3 @ ^0.0.3
* fastled/FastLED @ ^3.6.0
* arduino-libraries/Madgwick@^1.2.0
* [tinyxml2](https://github.com/leethomason/tinyxml2.git)
