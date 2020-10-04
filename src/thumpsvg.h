#ifndef SVG_DLL_H
#define SVG_DLL_H

#ifdef SVG_DLL
#define SVGDLL_API              EXTERN_C __declspec(dllexport) HRESULT WINAPI
#else
#define SVGDLL_API              EXTERN_C __declspec(dllimport) HRESULT WINAPI
#endif

SVGDLL_API RegisterViewerClass(HINSTANCE* phinst, ATOM* patom);

#endif
