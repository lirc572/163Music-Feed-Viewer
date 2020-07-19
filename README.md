# 163Music-Feed-Viewer

![demo](./demo.jpg "Demo")

Runs on LilyGo T5 V2.3 2.13 inch EPD dev board.

Developed using PlatformIO IDE.

The information on the EPD is updated once every 10 minutes. The battery voltage is shown on the bottom right corner.

## Software Setup

- Install VSCode and PlatformIO extension
- Clone this repo
- Create a `definitions.h` file under `./src` which defines `WIFI_SSID`, `WIFI_PASSWORD` and `NEUID` (see example below)
- Done :beer::beer::beer:

Example of `definitions.h`:

```C
// src/definitions.h
#define WIFI_SSID wireless@163
#define WIFI_PASSWORD password@163
#define NEUID 32953014
```
## 3D Printed Case

- The STL files used are [NewCase.STL](https://github.com/lirc572/163Music-Feed-Viewer/blob/master/3D_models/EXPORT/NewCase.STL) and [Cover.STL](https://github.com/lirc572/163Music-Feed-Viewer/blob/master/3D_models/EXPORT/Cover.STL).
- The models are not optimized for FDM printing so it is better to use an SLA printer.
- The one shown in the picture above was printed with resin. 
