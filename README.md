# Introduction
Microsoft is too lazy to provide SVG thumbnails on Windows Explorer.

## Prerequisites
- Windows 7 or later

## Usage
1. Open command prompt as administrator
1. Run `regsvr32 C:\full\path\to\thumpsvg.dll`

## Build
1. Install msys2
1. Install mingw, make, cmake and librsvg
1. Run `cmake -G "MSYS Makefiles" /path/to/src -DCMAKE_BUILD_TYPE="Release"`
1. Run `mingw32-make`

## Screenshots
<img alt="Screenshot" src="../assets/screenshot.png?raw=true" width="320">

## Todos
- [ ] Make an installer
- [ ] Add a viewer/converter
- [ ] Implement IPreviewHandler
- [ ] Statically link dependencies

## See also
- [svgextension](https://github.com/maphew/svg-explorer-extension/) - an SVG thumbnail provider that uses Qt instead of librsvg

## License
- GPL?
