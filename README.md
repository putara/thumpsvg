# Introduction
Microsoft is still too lazy to natively provide SVG thumbnails on Windows Explorer.

## Prerequisites
- Windows 10 64-bit
- Visual C++ runtime [for x64](https://aka.ms/vs/16/release/vc_redist.x64.exe)
- To build the source code, Visual Studio 2019, Clang, CMake, Rustup and Cargo are also required

## Usage
1. Disable or uninstall any other SVG thumbnail provider
1. Open command prompt as administrator
1. Run `regsvr32 C:\full\path\to\thumpsvg.dll`
1. Associate .SVG and .SVGZ to `svgview.exe`
1. Enjoy SVG thumbnails on Windows Explorer, Inkscape, and/or your favourite file manager app

## Build
1. Install Visual Studio 2019 with Clang and CMake
2. Install Rustup and Cargo with Visual Studio as toolchain
3. Open command prompt and run follows
```
git clone https://github.com/putara/thumpsvg.git
cd thumpsvg
git submodule init
git submodule update
cd tools
makelib
```
4. Build the DLL and the viewer on x64 Native Tools Command Prompt for VS 2019
```
cd /d C:\full\path\to\thumpsvg\src
nmake
```
5. Copy DLL and EXE in the bin\64 folder to somewhere

## Screenshots
<img alt="Screenshot" src="../assets/screenshot.png?raw=true" width="320">

## Todos
- [ ] Installer
- [X] Implement [IPreviewHandler](https://docs.microsoft.com/en-nz/windows/desktop/shell/preview-handlers)
- [ ] Implement [IQueryInfo](https://docs.microsoft.com/en-us/windows/desktop/api/shlobj_core/nn-shlobj_core-iqueryinfo)
- [X] Implement [IPropertyStore](https://docs.microsoft.com/en-us/windows/win32/api/propsys/nn-propsys-ipropertystore)

## See also
- [PowerToys](https://github.com/microsoft/PowerToys) - PowerToys comes with Microsoft's own SVG thumbnail provider that loads IE and .NET framework :scream:
- [svgextension](https://github.com/maphew/svg-explorer-extension/) - an SVG thumbnail provider that uses QtSvg
- [resvg](https://github.com/RazrFalcon/resvg/) - an SVG renderer and an SVG thumbnail provider that uses Qt or Skia

## License
This is free and unencumbered software released into the public domain.
For more information, please refer to [unlicense.org](https://unlicense.org/).
