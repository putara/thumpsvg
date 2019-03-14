# Introduction
Microsoft is too lazy to provide SVG thumbnails on Windows Explorer.

## Prerequisites
- Windows 7 or later

## Usage
1. Open command prompt as administrator
1. Run `regsvr32 C:\full\path\to\thumpsvg.dll`

## Build
1. Install msys2
1. Install mingw, make and cmake
1. Install rustup, cargo with *i686-pc-windows-gnu* or *x86_64-pc-windows-gnu* as toolchain
1. Open MSYS2 MinGW 32/64-bit
1. Run `cmake -G "MSYS Makefiles" /path/to/src -DCMAKE_BUILD_TYPE="Release"`
1. Run `mingw32-make`

## Screenshots
<img alt="Screenshot" src="../assets/screenshot.png?raw=true" width="320">

## Todos
- [ ] Installer
- [ ] Better SVG viewer
- [ ] Export as bmp, png, pdf etc.
- [ ] Implement [IPreviewHandler](https://docs.microsoft.com/en-nz/windows/desktop/shell/preview-handlers)
- [ ] Implement [IQueryInfo](https://docs.microsoft.com/en-us/windows/desktop/api/shlobj_core/nn-shlobj_core-iqueryinfo)
- [ ] Implement [IPropertySetStorage](https://docs.microsoft.com/en-us/windows/desktop/api/propidl/nn-propidl-ipropertysetstorage)
- [ ] Statically link dependencies

## See also
- [svgextension](https://github.com/maphew/svg-explorer-extension/) - an SVG thumbnail provider that uses QtSvg
- [resvg](https://github.com/RazrFalcon/resvg/) - an SVG renderer and an SVG thumbnail provider that uses Qt instead of cairo

## License
Released under the [WTFPL](http://www.wtfpl.net/about/).
