#define _WIN32_WINNT    0x601
#define _WIN32_IE       0x700
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <advpub.h>
#include <shlwapi.h>

#include <new>

#include "dll.h"
#include "sdk.h"
#include "svg.hpp"


EXTERN_C const CLSID CLSID_ThumbProviderSVG;

class DllRefCount
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


class RefCount : public DllRefCount
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


class ThumbProviderSVG : public IInitializeWithFile, public IInitializeWithStream, public IThumbnailProvider
{
private:
    RefCount ref;
    Svg svg;

    static const size_t MAX_SVG_SIZE = 32U << 20;

    void Close()
    {
        this->svg.Close();
    }

public:
    ThumbProviderSVG()
    {
    }
    ~ThumbProviderSVG()
    {
        this->Close();
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
        this->Close();
        return this->svg.Load(pszFilePath);
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pstm, DWORD grfMode)
    {
        this->Close();
        STATSTG stat;
        HRESULT hr = pstm->Stat(&stat, STATFLAG_NONAME);
        if (SUCCEEDED(hr)) {
            hr = stat.cbSize.QuadPart <= MAX_SVG_SIZE ? S_OK : E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            ULONG size = stat.cbSize.LowPart;
            void* ptr = ::calloc(1, size);
            hr = ptr != NULL ? S_OK : E_OUTOFMEMORY;
            if (SUCCEEDED(hr)) {
                hr = pstm->Read(ptr, size, &size);
            }
            if (SUCCEEDED(hr)) {
                hr = this->svg.Load(ptr, size);
            }
            ::free(ptr);
        }
        return hr;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
    {
        if (pdwAlpha != NULL) {
            *pdwAlpha = WTSAT_ARGB;
        }
        return InternalGetThumbnail(cx, phbmp);
    }

    HRESULT InternalGetThumbnail(UINT cx, HBITMAP* phbmp)
    {
        if (phbmp == NULL) {
            return E_POINTER;
        }
        *phbmp = NULL;
        if (cx - 1 >= INT_MAX) {
            return E_INVALIDARG;
        }
        const int size = static_cast<int>(cx);
        return this->svg.RenderToBitmap(size, size, 0, true, phbmp);
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

    ClassFactory* inst = new (std::nothrow) ClassFactory();
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
    if (::PathAppendW(advPackPath, L"advpack.dll") == FALSE) {
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

    char desc[128];
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
