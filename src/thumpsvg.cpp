#define _WIN32_WINNT    0x601
#define _WIN32_IE       0x700
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <advpub.h>
#include <shlwapi.h>

#include <math.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <rsvg.h>

#include <new>

#include "dll.h"

#ifndef __IThumbnailProvider_FWD_DEFINED__
#define __IThumbnailProvider_FWD_DEFINED__
typedef interface IThumbnailProvider IThumbnailProvider;
#endif

#ifndef __IThumbnailProvider_INTERFACE_DEFINED__
#define __IThumbnailProvider_INTERFACE_DEFINED__

typedef enum WTS_ALPHATYPE {
    WTSAT_UNKNOWN = 0,
    WTSAT_RGB = 1,
    WTSAT_ARGB = 2
} WTS_ALPHATYPE;

EXTERN_C const IID IID_IThumbnailProvider;

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IThumbnailProvider, 0xe357fccd, 0xa995, 0x4576, 0xb0, 0x1f, 0x23, 0x46, 0x30, 0x15, 0x4e, 0x96)
#else
const IID IID_IThumbnailProvider = { 0xe357fccd, 0xa995, 0x4576, { 0xb0, 0x1f, 0x23, 0x46, 0x30, 0x15, 0x4e, 0x96 } };
#endif

#undef INTERFACE
#define INTERFACE IThumbnailProvider

DECLARE_INTERFACE_IID_(IThumbnailProvider, IUnknown, "e357fccd-a995-4576-b01f-234630154e96")
{
    STDMETHOD(GetThumbnail)(THIS_ UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) PURE;
};

#endif


EXTERN_C const CLSID CLSID_ThumbProviderSVG;

class DECLSPEC_NOVTABLE DllRefCount
{
private:
    static volatile LONG s_cRef;

public:
    static ULONG AddRef()
    {
        return ::InterlockedIncrement(&s_cRef);
    }
    static ULONG Release()
    {
        return ::InterlockedDecrement(&s_cRef);
    }
    static ULONG GetLockCount()
    {
        return s_cRef;
    }
};

volatile LONG DllRefCount::s_cRef = 0;

class DECLSPEC_NOVTABLE RefCount : public DllRefCount
{
private:
    volatile LONG cRef;

public:
    RefCount()
        : cRef(1)
    {
        DllRefCount::AddRef();
    }
    ~RefCount()
    {
        this->cRef = INT_MAX / 2;
        DllRefCount::Release();
    }

    ULONG AddRef()
    {
        return ::InterlockedIncrement(&this->cRef);
    }
    ULONG Release()
    {
        return ::InterlockedDecrement(&this->cRef);
    }
};


void* mmopen(const wchar_t* path, size_t* len)
{
    HANDLE hf, hfm;
    void* ptr = NULL;
    LARGE_INTEGER cb;
    cb.QuadPart = 0;
    hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        if (GetFileSizeEx(hf, &cb) && cb.HighPart == 0) {
            hfm = CreateFileMappingW(hf, NULL, PAGE_READONLY, 0, 0, NULL);
            CloseHandle(hf);
            if (hfm != NULL) {
                ptr = MapViewOfFile(hfm, FILE_MAP_READ, 0, 0, 0);
            }
            CloseHandle(hfm);
        }
        CloseHandle(hf);
    }
    *len = cb.LowPart;
    return ptr;
}

void mmclose(void* ptr)
{
    UnmapViewOfFile(ptr);
}

RsvgHandle* loadsvgmem(const void* ptr, size_t cb)
{
    GError* error = NULL;
    return rsvg_handle_new_from_data((const guint8*)ptr, cb, &error);
}

RsvgHandle* loadsvg(const wchar_t* path)
{
    size_t cb;
    void* ptr = mmopen(path, &cb);
    if (ptr == NULL) {
        return NULL;
    }
    RsvgHandle* rsvg = loadsvgmem(ptr, cb);
    mmclose(ptr);
    return rsvg;
}

void closesvg(RsvgHandle* rsvg)
{
    if (rsvg != NULL) {
        GError* error = NULL;
        rsvg_handle_close(rsvg, &error);
        g_object_unref(rsvg);
    }
}

void fill(HDC hdc, int width, int height, COLORREF bgcolour)
{
    COLORREF oldcolour = SetBkColor(hdc, bgcolour);
    RECT rect = { 0, 0, width, height };
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
    SetBkColor(hdc, oldcolour);
}

void rendersvg(RsvgHandle* rsvg, HDC hdc, int width, int height, COLORREF bgcolour)
{
    if (rsvg == NULL) {
        fill(hdc, width, height, bgcolour);
        return;
    }
    RsvgDimensionData dimensions = {};
    rsvg_handle_get_dimensions(rsvg, &dimensions);
    int org_width = dimensions.width;
    int org_height = dimensions.height;
    if ((org_width | org_height) == 0) {
        fill(hdc, width, height, bgcolour);
        return;
    }
    cairo_surface_t* surface = cairo_win32_surface_create_with_ddb(hdc, CAIRO_FORMAT_RGB24, width, height);
    if (surface == NULL) {
        fill(hdc, width, height, bgcolour);
        return;
    }
    cairo_t* cr = cairo_create(surface);
    if (cr != NULL) {
        cairo_set_source_rgb(cr,
            GetRValue(bgcolour) / 255.0,
            GetGValue(bgcolour) / 255.0,
            GetBValue(bgcolour) / 255.0);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
        double x_scale = 1, y_scale = 1;
        if (org_width > width) {
            x_scale = (double)width  / org_width;
        }
        if (org_height > height) {
            y_scale = (double)height / org_height;
        }
        double scale = x_scale < y_scale ? x_scale : y_scale;
        int new_width  = (int)floor(scale * org_width);
        int new_height = (int)floor(scale * org_height);
        cairo_matrix_t mat = {};
        mat.yy = mat.xx = scale;
        mat.x0 = (width - new_width) / 2;
        mat.y0 = (height - new_height) / 2;
        cairo_transform(cr, &mat);
        rsvg_handle_render_cairo(rsvg, cr);
        cairo_destroy(cr);
    }
    cairo_surface_flush(surface);
    BitBlt(hdc, 0, 0, width, height, cairo_win32_surface_get_dc(surface), 0, 0, SRCCOPY);
    cairo_surface_destroy(surface);
}


class ThumbProviderSVG : public IInitializeWithFile, public IInitializeWithStream, public IThumbnailProvider
{
private:
    RefCount ref;
    RsvgHandle* rsvg;

    void close()
    {
        if (this->rsvg != NULL) {
            closesvg(this->rsvg);
            this->rsvg = NULL;
        }
    }

public:
    ThumbProviderSVG()
        : rsvg()
    {
    }
    ~ThumbProviderSVG()
    {
        this->close();
    }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv)
    {
        static const QITAB c_atab[] =
        {
            QITABENT(ThumbProviderSVG, IThumbnailProvider),
            QITABENT(ThumbProviderSVG, IInitializeWithFile),
            QITABENT(ThumbProviderSVG, IInitializeWithStream),
            { 0 },
        };
        return ::QISearch(this, c_atab, riid, ppv);
    }
    STDMETHOD_(ULONG, AddRef)()
    {
        return this->ref.AddRef();
    }
    STDMETHOD_(ULONG, Release)()
    {
        const ULONG ret = this->ref.Release();
        if (ret == 0) {
            delete this;
        }
        return ret;
    }

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode)
    {
        this->close();
        this->rsvg = loadsvg(pszFilePath);
        return S_OK;
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pstm, DWORD grfMode)
    {
        this->close();
        STATSTG stat;
        HRESULT hr = pstm->Stat(&stat, STATFLAG_NONAME);
        if (SUCCEEDED(hr) && stat.cbSize.QuadPart < (32U << 20)) {
            void* ptr = ::calloc(1, stat.cbSize.LowPart);
            if (ptr != NULL) {
                pstm->Read(ptr, stat.cbSize.LowPart, NULL);
                this->rsvg = loadsvgmem(ptr, stat.cbSize.LowPart);
                ::free(ptr);
            }
        }
        return hr;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
    {
        if (pdwAlpha != NULL) {
            // TODO: support WTSAT_ARGB
            *pdwAlpha = WTSAT_RGB;
        }
        return InternalGetThumbnail(cx, phbmp);
    }

    HRESULT InternalGetThumbnail(UINT cx, HBITMAP* phbmp)
    {
        if (phbmp == NULL) {
            return E_POINTER;
        }
        *phbmp = NULL;
        if (this->rsvg == NULL) {
            return E_FAIL;
        }
        if (cx - 1 >= INT_MAX) {
            return E_INVALIDARG;
        }

        int size = static_cast<int>(cx);
        BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), size, -size, 1, 32 } };
        HBITMAP hbmp = ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
        if (hbmp == NULL) {
            return E_OUTOFMEMORY;
        }

        if (this->rsvg != NULL) {
            HDC hdc = CreateCompatibleDC(NULL);
            if (hdc != NULL) {
                HBITMAP hbmpold = SelectBitmap(hdc, hbmp);
                rendersvg(this->rsvg, hdc, size, size, 0xffffff);
                SelectBitmap(hdc, hbmpold);
                DeleteDC(hdc);
            }
        }

        *phbmp = hbmp;
        return S_OK;
    }
};


class ClassFactory : public IClassFactory
{
private:
    RefCount ref;

public:
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv)
    {
        static const QITAB c_atab[] =
        {
            QITABENT(ClassFactory, IClassFactory),
            { 0 },
        };
        return ::QISearch(this, c_atab, riid, ppv);
    }
    STDMETHOD_(ULONG, AddRef)()
    {
        return this->ref.AddRef();
    }
    STDMETHOD_(ULONG, Release)()
    {
        const ULONG ret = this->ref.Release();
        if (ret == 0) {
            delete this;
        }
        return ret;
    }

    // IClassFactory
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv)
    {
        if (ppv == NULL) {
            return E_POINTER;
        }
        *ppv = NULL;

        if (pUnkOuter != NULL) {
            return CLASS_E_NOAGGREGATION;
        }

        ThumbProviderSVG* inst = new (std::nothrow) ThumbProviderSVG();
        if (inst == NULL) {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = inst->QueryInterface(riid, ppv);
        inst->Release();

        return hr;
    }

    STDMETHOD(LockServer)(BOOL fLock)
    {
        if (fLock) {
            DllRefCount::AddRef();
        } else {
            DllRefCount::Release();
        }
        return S_OK;
    }
};


#define String_CLSID_ThumbProviderSVG "{C9148B50-E15E-440d-A07F-CF385EC73914}"

// {C9148B50-E15E-440d-A07F-CF385EC73914}
//DEFINE_GUID(CLSID_ThumbProviderSVG, 0xc9148b50, 0xe15e, 0x440d, 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14);
const GUID CLSID_ThumbProviderSVG = { 0xc9148b50, 0xe15e, 0x440d, { 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14 } };

static HINSTANCE s_hinstDLL;

extern "C"
{

STDAPI DllCanUnloadNow(void)
{
    return (DllRefCount::GetLockCount() == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (ppv == NULL) {
        return E_POINTER;
    }
    *ppv = NULL;

    if (IsEqualCLSID(rclsid, CLSID_ThumbProviderSVG) == FALSE) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ClassFactory* inst = new ClassFactory();
    if (inst == NULL) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = inst->QueryInterface(riid, ppv);
    inst->Release();

    return hr;
}

STDAPI DllInstall(BOOL install, LPCWSTR cmdLine)
{
    UNREFERENCED_PARAMETER(cmdLine);

    wchar_t advPackPath[MAX_PATH];
    if (::GetSystemDirectoryW(advPackPath, ARRAYSIZE(advPackPath)) == 0) {
        return E_FAIL;
    }
    if (::PathAppendW(advPackPath, L"\\advpack.dll") == FALSE) {
        return E_FAIL;
    }

    HMODULE hmod = ::LoadLibraryW(advPackPath);
    if (hmod == NULL) {
        return E_FAIL;
    }

    REGINSTALL regInstall = reinterpret_cast<REGINSTALL>(::GetProcAddress(hmod, "RegInstall"));
    if (regInstall == NULL) {
        return E_FAIL;
    }

    char desc[64];
    if (::LoadStringA(s_hinstDLL, IDS_DESC, desc, ARRAYSIZE(desc)) == 0) {
        return E_FAIL;
    }
    char clsid[] = String_CLSID_ThumbProviderSVG;
    char k_clsid[] = "CLSID_ThisDLL";
    char k_desc[] = "DESC_ThisDLL";

    STRENTRY ase[] =
    {
        { k_clsid,  clsid },
        { k_desc,   desc },
    };
    const STRTABLE table = { ARRAYSIZE(ase), ase };

    HRESULT hr = regInstall(s_hinstDLL, install ? "RegDll" : "UnregDll", &table);
    if (SUCCEEDED(hr)) {
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    return hr;
}

STDAPI DllRegisterServer(void)
{
    return DllInstall(TRUE, NULL);
}

STDAPI DllUnregisterServer(void)
{
    return DllInstall(FALSE, NULL);
}

}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        s_hinstDLL = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
