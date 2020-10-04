#define _WIN32_WINNT    0x0A00
#define _WIN32_IE       0x0A00
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <advpub.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <strsafe.h>
#include <pathcch.h>
#include <wincodec.h>

#include <new>

// The WICCreateImagingFactory_Proxy function is not declared in the public SDK header
EXTERN_C DECLSPEC_IMPORT HRESULT WINAPI WICCreateImagingFactory_Proxy(_In_ UINT SDKVersion, _Out_ IWICImagingFactory** ppIImagingFactory);


#define SVG_DLL
#include "thumpsvg.h"
#include "svg.hpp"
#include "render.hpp"
#include "bitmap.hpp"

#include "common.h"
#include "thumpsvg.rc"
#include "viewimpl.hpp"

EXTERN_C const CLSID CLSID_ThumbProviderSVG;


class DllRefCount
{
private:
    static volatile LONG s_cRef;

public:
    static ULONG AddRef() noexcept
    {
        return ::InterlockedIncrement(&s_cRef);
    }

    static ULONG Release() noexcept
    {
        return ::InterlockedDecrement(&s_cRef);
    }

    static ULONG GetLockCount() noexcept
    {
        return s_cRef;
    }
};

volatile LONG DllRefCount::s_cRef = 0;


class RefCount : public DllRefCount
{
private:
    volatile LONG cRef;

public:
    RefCount() noexcept
        : cRef(1)
    {
        DllRefCount::AddRef();
    }

    ~RefCount() noexcept
    {
        this->cRef = INT_MAX / 2;
        DllRefCount::Release();
    }

    ULONG AddRef() noexcept
    {
        return ::InterlockedIncrement(&this->cRef);
    }

    ULONG Release() noexcept
    {
        return ::InterlockedDecrement(&this->cRef);
    }
};


class ThumbProviderSVG : public IInitializeWithFile, public IInitializeWithStream, public IThumbnailProvider, public NoThrowObject
{
private:
    RefCount ref;
    Svg svg;

    static const size_t MAX_SVG_SIZE = 32U << 20;

    void Destroy() noexcept
    {
        this->svg.Destroy();
    }

public:
    ThumbProviderSVG() noexcept
    {
    }

    ~ThumbProviderSVG() noexcept
    {
        this->Destroy();
    }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) noexcept
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

    STDMETHOD_(ULONG, AddRef)() noexcept
    {
        return this->ref.AddRef();
    }

    STDMETHOD_(ULONG, Release)() noexcept
    {
        const ULONG ret = this->ref.Release();
        if (ret == 0) {
            delete this;
        }
        return ret;
    }

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR filePath, DWORD mode) noexcept
    {
        this->Destroy();
        SvgOptions opt;
        // TODO: loading system fonts might be overkill
        opt.LoadSystemFonts();
        return this->svg.Load(filePath, opt);
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pstm, DWORD mode) noexcept
    {
        this->Destroy();
        SvgOptions opt;
        // TODO: loading system fonts might be overkill
        opt.LoadSystemFonts();
        STATSTG stat;
        HRESULT hr = pstm->Stat(&stat, STATFLAG_NONAME);
        if (SUCCEEDED(hr)) {
            hr = stat.cbSize.QuadPart <= MAX_SVG_SIZE ? S_OK : E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            ULONG size = stat.cbSize.LowPart;
            void* ptr = ::calloc(1, size);
            hr = ptr != nullptr ? S_OK : E_OUTOFMEMORY;
            if (SUCCEEDED(hr)) {
                hr = pstm->Read(ptr, size, &size);
            }
            if (SUCCEEDED(hr)) {
                hr = this->svg.Load(ptr, size, opt);
            }
            ::free(ptr);
        }
        return hr;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* type) noexcept
    {
        if (type != nullptr) {
            *type = WTSAT_ARGB;
        }
        return InternalGetThumbnail(cx, phbmp);
    }

    HRESULT InternalGetThumbnail(UINT cx, HBITMAP* phbmp) noexcept
    {
        if (phbmp == nullptr) {
            return E_POINTER;
        }
        *phbmp = nullptr;
        if (cx >= INT_MAX) {
            return E_INVALIDARG;
        }
        SvgRenderTarget::RenderOptions opt;
        opt.SetTransparent().SetCanvasSize(cx, cx).SetToContain();
        SvgRenderTarget target;
        HRESULT hr = this->svg.Render(opt, &target);
        if (SUCCEEDED(hr)) {
            Bitmap bmp;
            if (SUCCEEDED(target.ToGdiBitmap(false, bmp.GetAddressOf()))) {
                *phbmp = bmp.Detach();
            }
        }
        return hr;
    }
};


class ClassFactory : public IClassFactory, public NoThrowObject
{
private:
    RefCount ref;

public:
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) noexcept
    {
        static const QITAB c_atab[] =
        {
            QITABENT(ClassFactory, IClassFactory),
            { 0 },
        };
        return ::QISearch(this, c_atab, riid, ppv);
    }

    STDMETHOD_(ULONG, AddRef)() noexcept
    {
        return this->ref.AddRef();
    }

    STDMETHOD_(ULONG, Release)() noexcept
    {
        const ULONG ret = this->ref.Release();
        if (ret == 0) {
            delete this;
        }
        return ret;
    }

    // IClassFactory
    STDMETHOD(CreateInstance)(IUnknown* unkOuter, REFIID riid, void** ppv) noexcept
    {
        if (ppv == nullptr) {
            return E_POINTER;
        }
        *ppv = nullptr;

        if (unkOuter != nullptr) {
            return CLASS_E_NOAGGREGATION;
        }

        ThumbProviderSVG* inst = new ThumbProviderSVG();
        HRESULT hr = inst != nullptr ? S_OK : E_OUTOFMEMORY;
        if (SUCCEEDED(hr)) {
            hr = inst->QueryInterface(riid, ppv);
            inst->Release();
        }

        return hr;
    }

    STDMETHOD(LockServer)(BOOL lock) noexcept
    {
        if (lock) {
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

extern "C"
{

STDAPI DllCanUnloadNow(void)
{
    return (DllRefCount::GetLockCount() == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_ThumbProviderSVG) == FALSE) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ClassFactory* inst = new ClassFactory();
    HRESULT hr = inst != nullptr ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = inst->QueryInterface(riid, ppv);
        inst->Release();
    }

    return hr;
}

STDAPI DllInstall(BOOL install, LPCWSTR cmdLine)
{
    UNREFERENCED_PARAMETER(cmdLine);

    wchar_t advPackPath[MAX_PATH];
    HMODULE hmod = nullptr;
    REGINSTALLA regInstall = nullptr;

    HRESULT hr = ::GetSystemDirectoryW(advPackPath, ARRAYSIZE(advPackPath)) ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
    if (SUCCEEDED(hr)) {
        hr = ::PathCchAppend(advPackPath, ARRAYSIZE(advPackPath), L"advpack.dll");
    }
    if (SUCCEEDED(hr)) {
        hmod = ::LoadLibraryExW(advPackPath, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        hr = hmod != nullptr ? S_OK : ResultFromLastError();
    }
    if (SUCCEEDED(hr)) {
        regInstall = reinterpret_cast<REGINSTALLA>(::GetProcAddress(hmod, "RegInstall"));
        hr = regInstall != nullptr ? S_OK : ResultFromLastError();
    }
    if (SUCCEEDED(hr)) {
        char desc[128];
        hr = ::LoadStringA(HINST_THISCOMPONENT, IDS_DESC, desc, ARRAYSIZE(desc)) > 0 ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
        char clsid[] = String_CLSID_ThumbProviderSVG;
        char k_clsid[] = "CLSID_ThisDLL";
        char k_desc[] = "DESC_ThisDLL";
        STRENTRYA ase[] =
        {
            { k_clsid,  clsid },
            { k_desc,   desc },
        };
        const STRTABLEA table = { ARRAYSIZE(ase), ase };
        hr = regInstall(HINST_THISCOMPONENT, install ? "RegDll" : "UnregDll", &table);
    }
    if (SUCCEEDED(hr)) {
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    return hr;
}

STDAPI DllRegisterServer(void)
{
    return DllInstall(TRUE, nullptr);
}

STDAPI DllUnregisterServer(void)
{
    return DllInstall(FALSE, nullptr);
}

} // extern "C"


static ATOM s_atom;

SVGDLL_API RegisterViewerClass(HINSTANCE* phinst, ATOM* patom)
{
    ATOM atom = s_atom;
    if (atom == 0) {
        atom = SvgViewerImpl::RegisterClass(HINST_THISCOMPONENT);
        if (atom == 0) {
            return ResultFromLastError();
        }
        s_atom = atom;
    }
    *phinst = HINST_THISCOMPONENT;
    *patom = atom;
    return S_OK;
}

STDAPI_(void) UnregisterViewerClass()
{
    ATOM atom = s_atom;
    s_atom = 0;
    ::UnregisterClass(MAKEINTATOM(atom), HINST_THISCOMPONENT);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(hinstDLL);
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        // Oops, calling out a non-kernel function from DllMain
        UnregisterViewerClass();
    }
    return TRUE;
}
