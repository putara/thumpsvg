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
#include <propkey.h>
#include <propvarutil.h>

#include <new>

#pragma comment(lib, "propsys.lib")

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

#define TRACE(...)  Debug::Print(__VA_ARGS__)


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


template <class T>
class ComBaseObject
    : public NoThrowObject
{
protected:
    static const size_t MAX_SVG_SIZE = 32U << 20;

    template <typename T>
    static void Swap(T& lhs, T& rhs) noexcept
    {
        T tmp = std::move(lhs);
        lhs = std::move(rhs);
        rhs = std::move(tmp);
    }

    struct MemData
    {
        void* data = nullptr;
        ULONG size = 0;

        MemData() noexcept = default;
        MemData(MemData&& src) noexcept
        {
            this->data = std::exchange(src.data, nullptr);
            this->size = std::exchange(src.size, 0);
        }
        ~MemData() noexcept
        {
            ::free(this->data);
        }
        HRESULT Alloc(ULONG size) noexcept
        {
            this->data = ::calloc(size, 1);
            this->size = size;
            return this->data != nullptr ? S_OK : E_OUTOFMEMORY;
        }
        MemData& operator =(MemData&& src) noexcept
        {
            Swap(this->data, src.data);
            Swap(this->size, src.size);
            return *this;
        }
    };

    static HRESULT StreamReadAll(IStream* pstm, void** ppv, ULONG* pcb) noexcept
    {
        *ppv = nullptr;
        *pcb = 0;
        void* data = nullptr;
        ULONG size = 0;
        STATSTG stat;
        HRESULT hr = IStream_Reset(pstm);
        if (SUCCEEDED(hr)) {
            hr = pstm->Stat(&stat, STATFLAG_NONAME);
        }
        if (SUCCEEDED(hr)) {
            hr = stat.cbSize.QuadPart <= MAX_SVG_SIZE ? S_OK : E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            size = stat.cbSize.LowPart;
            data = ::calloc(size, 1);
            hr = data != nullptr ? S_OK : E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hr)) {
            hr = pstm->Read(data, size, &size);
        }
        if (SUCCEEDED(hr)) {
            *ppv = data;
            *pcb = size;
        } else {
            ::free(data);
        }
        return hr;
    }

public:
    static LPCQITAB GetQITab() noexcept
    {
        static const QITAB c_atab[] = {{0}};
        return c_atab;
    }
};

class PreviewHandlerSVG
    : public IInitializeWithStream
    , public IObjectWithSite
    , public IOleWindow
    , public IPreviewHandler
    , public IPreviewHandlerVisuals
    , public ComBaseObject<PreviewHandlerSVG>
{
private:
    RefCount ref;
    SvgViewer viewer;
    void* data;
    ULONG size;
    COLORREF backColour;
    HWND hwndOwner;
    RECT rect;
    IUnknown* site;

    void Destroy() noexcept
    {
        if (this->site != nullptr) {
            this->site->Release();
            this->site = nullptr;
        }
        ::free(this->data);
        this->data = nullptr;
        this->size = 0;
        this->viewer.Destroy();
    }

public:
    PreviewHandlerSVG() noexcept
    {
        this->data = nullptr;
        this->size = 0;
        this->backColour = CLR_NONE;
        this->site = nullptr;
        this->hwndOwner = nullptr;
        ZeroMemory(&this->rect, sizeof(RECT));
    }

    ~PreviewHandlerSVG() noexcept
    {
        this->Destroy();
    }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) noexcept
    {
        static const QITAB c_atab[] =
        {
            QITABENT(PreviewHandlerSVG, IInitializeWithStream),
            QITABENT(PreviewHandlerSVG, IObjectWithSite),
            QITABENT(PreviewHandlerSVG, IOleWindow),
            QITABENT(PreviewHandlerSVG, IPreviewHandler),
            QITABENT(PreviewHandlerSVG, IPreviewHandlerVisuals),
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

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pstm, DWORD mode) noexcept
    {
        TRACE(__FUNCTION__ "(0x%p, %u)\n", pstm, mode);
        this->Destroy();
        void* data;
        ULONG size;
        HRESULT hr = StreamReadAll(pstm, &data, &size);
        if (SUCCEEDED(hr)) {
            ::free(this->data);
            this->data = data;
            this->size = size;
        }
        return hr;
    }

    // IObjectWithSite
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv)
    {
        TRACE(__FUNCTION__ "()\n");
        if (ppv == nullptr) {
            return E_INVALIDARG;
        }
        if (this->site == nullptr) {
            *ppv = nullptr;
            return E_FAIL;
        }
        return this->site->QueryInterface(riid, ppv);
    }

    IFACEMETHODIMP SetSite(IUnknown* site)
    {
        TRACE(__FUNCTION__ "(0x%p)\n", site);
        if (this->site != nullptr) {
            this->site->Release();
        }
        this->site = site;
        if (site != nullptr) {
            site->AddRef();
        }
        return S_OK;
    }

    // IOleWindow
    IFACEMETHODIMP ContextSensitiveHelp(BOOL enterMode)
    {
        TRACE(__FUNCTION__ "(%s)\n", enterMode ? "true" : "false");
        UNREFERENCED_PARAMETER(enterMode);
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetWindow(HWND* phwnd)
    {
        TRACE(__FUNCTION__ "()\n");
        if (phwnd == nullptr) {
            return E_INVALIDARG;
        }
        *phwnd = this->viewer.GetHwnd();
        if (*phwnd == nullptr) {
            return E_FAIL;
        }
        return S_OK;
    }

    // IPreviewHandler
    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc)
    {
        TRACE(__FUNCTION__ "(0x%p, {%d,%d,%d,%d})\n", hwnd, prc ? prc->left : 0, prc ? prc->top : 0, prc ? prc->right : 0, prc ? prc->bottom : 0);
        if (hwnd == nullptr || prc == nullptr) {
            return E_INVALIDARG;
        }
        if (this->viewer.GetHwnd() != nullptr) {
            return E_FAIL;
        }
        this->hwndOwner = hwnd;
        this->rect = *prc;
        return S_OK;
    }

    IFACEMETHODIMP SetRect(const RECT* prc)
    {
        TRACE(__FUNCTION__ "({%d,%d,%d,%d})\n", prc ? prc->left : 0, prc ? prc->top : 0, prc ? prc->right : 0, prc ? prc->bottom : 0);
        if (prc == nullptr) {
            return E_INVALIDARG;
        }
        if (this->viewer.GetHwnd() != nullptr) {
            ::SetWindowPos(this->viewer.GetHwnd(), nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            this->rect = *prc;
        }
        return S_OK;
    }

    IFACEMETHODIMP DoPreview()
    {
        TRACE(__FUNCTION__ "()\n");
        if (this->viewer.GetHwnd() != nullptr) {
            return E_FAIL;
        }
        if (this->data == nullptr) {
            return E_FAIL;
        }
        if (this->hwndOwner == nullptr) {
            return E_FAIL;
        }
        if (this->viewer.Create(1, this->hwndOwner) == false) {
            return E_FAIL;
        }
        this->viewer.SetBackgroundMode(SVGBGM_SOLID);
        this->viewer.SetBackgroundColour(this->backColour);
        this->SetRect(&this->rect);
        HRESULT hr = this->viewer.LoadFromMemory(this->data, this->size);
        ::free(this->data);
        this->data = nullptr;
        this->size = 0;
        if (FAILED(hr)) {
            this->Unload();
        }
        return hr;
    }

    IFACEMETHODIMP Unload()
    {
        TRACE(__FUNCTION__ "()\n");
        if (this->viewer.GetHwnd() == nullptr) {
            return S_OK;
        }
        this->viewer.Destroy();
        return S_OK;
    }

    IFACEMETHODIMP SetFocus()
    {
        TRACE(__FUNCTION__ "()\n");
        return E_NOTIMPL;
    }

    IFACEMETHODIMP QueryFocus(HWND* phwnd)
    {
        TRACE(__FUNCTION__ "()\n");
        if (phwnd == nullptr) {
            return E_INVALIDARG;
        }
        return E_NOTIMPL;
    }

    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg)
    {
        TRACE(__FUNCTION__ "()\n");
        if (pmsg == nullptr) {
            return E_INVALIDARG;
        }
        return E_NOTIMPL;
    }

    // IPreviewHandlerVisuals
    IFACEMETHODIMP SetBackgroundColor(COLORREF color)
    {
        TRACE(__FUNCTION__ "(0x%08x)\n", color);
        if (this->viewer.GetHwnd() != nullptr) {
            this->viewer.SetBackgroundColour(color);
        } else {
            this->backColour = color;
        }
        return S_OK;
    }

    IFACEMETHODIMP SetFont(const LOGFONTW* plf)
    {
        TRACE(__FUNCTION__ "(0x%p)\n", plf);
        if (plf == nullptr) {
            return E_INVALIDARG;
        }
        // TODO: do something
        return E_NOTIMPL;
    }

    IFACEMETHODIMP SetTextColor(COLORREF color)
    {
        TRACE(__FUNCTION__ "(0x%08x)\n", color);
        // TODO: do something
        UNREFERENCED_PARAMETER(color);
        return E_NOTIMPL;
    }
};

class ThumbProviderSVG
    : public IInitializeWithFile
    , public IInitializeWithStream
    , public IThumbnailProvider
    , public IPropertyStore
    , public IPropertyStoreCapabilities
    , public ComBaseObject<ThumbProviderSVG>
{
private:
    RefCount ref;
    Svg svg;
    IPropertyStoreCache* cache;

    void Destroy() noexcept
    {
        this->svg.Destroy();
        if (this->cache != nullptr) {
            this->cache->Release();
            this->cache = nullptr;
        }
    }

public:
    ThumbProviderSVG() noexcept
    {
        this->cache = nullptr;
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
            QITABENT(ThumbProviderSVG, IPropertyStore),
            QITABENT(ThumbProviderSVG, IPropertyStoreCapabilities),
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
        opt.SetSpeedOverQuality(true);
        return this->svg.Load(filePath, opt);
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pstm, DWORD mode) noexcept
    {
        this->Destroy();
        void* data;
        ULONG size;
        HRESULT hr = StreamReadAll(pstm, &data, &size);
        if (SUCCEEDED(hr)) {
            SvgOptions opt;
            // TODO: loading system fonts might be overkill
            opt.LoadSystemFonts();
            hr = this->svg.Load(data, size, opt);
        }
        return hr;
    }

    HRESULT CachePropertyStore() noexcept
    {
        if (this->cache != nullptr) {
            return S_OK;
        }
        IPropertyStoreCache* cache = nullptr;
        HRESULT hr = ::PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache));
        if (SUCCEEDED(hr)) {
            auto size = this->svg.GetSize();
            WCHAR buff[256] = {};
            ::StringCchPrintfW(buff, ARRAYSIZE(buff), L"%u x %u", size.width, size.height);
            PROPVARIANT propvar;
            if (SUCCEEDED(::InitPropVariantFromString(buff, &propvar))) {
                cache->SetValueAndState(PKEY_Image_Dimensions, &propvar, PSC_NORMAL);
                ::PropVariantClear(&propvar);
            }
            if (SUCCEEDED(::InitPropVariantFromDouble(size.width, &propvar))) {
                cache->SetValueAndState(PKEY_Image_HorizontalSize, &propvar, PSC_NORMAL);
                ::PropVariantClear(&propvar);
            }
            if (SUCCEEDED(::InitPropVariantFromDouble(size.height, &propvar))) {
                cache->SetValueAndState(PKEY_Image_VerticalSize, &propvar, PSC_NORMAL);
                ::PropVariantClear(&propvar);
            }
            if (SUCCEEDED(::InitPropVariantFromUInt32(32, &propvar))) {
                cache->SetValueAndState(PKEY_Image_BitDepth, &propvar, PSC_NORMAL);
                ::PropVariantClear(&propvar);
            }
            if (SUCCEEDED(::InitPropVariantFromString(L"image/svg+xml", &propvar))) {
                cache->SetValueAndState(PKEY_MIMEType, &propvar, PSC_NORMAL);
                ::PropVariantClear(&propvar);
            }
            this->cache = cache;
            cache = nullptr;
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
        opt.SetCanvasSize(cx, cx).SetToContain();
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

    // IPropertyStore
    IFACEMETHODIMP STDMETHODCALLTYPE GetCount(DWORD* cProps)
    {
        if (cProps == nullptr) {
            return E_INVALIDARG;
        }
        HRESULT hr = this->CachePropertyStore();
        if (SUCCEEDED(hr)) {
            hr = this->cache->GetCount(cProps);
        }
        return hr;
    }

    IFACEMETHODIMP STDMETHODCALLTYPE GetAt(DWORD iProp, PROPERTYKEY* pkey)
    {
        if (pkey == nullptr) {
            return E_INVALIDARG;
        }
        HRESULT hr = this->CachePropertyStore();
        if (SUCCEEDED(hr)) {
            hr = this->cache->GetAt(iProp, pkey);
        }
        return hr;
    }

    IFACEMETHODIMP STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key, PROPVARIANT* pv)
    {
        if (pv == nullptr) {
            return E_INVALIDARG;
        }
        ::PropVariantInit(pv);
        HRESULT hr = this->CachePropertyStore();
        if (SUCCEEDED(hr)) {
            hr = this->cache->GetValue(key, pv);
        }
        return hr;
    }

    IFACEMETHODIMP STDMETHODCALLTYPE SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar)
    {
        return STG_E_ACCESSDENIED;
    }

    IFACEMETHODIMP STDMETHODCALLTYPE Commit()
    {
        return STG_E_ACCESSDENIED;
    }

    // IPropertyStoreCapabilities
    IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key)
    {
        return S_FALSE;
    }
};


enum ClassType
{
    Class_ThumbProvider,
    Class_PreviewHandler,
};

class ClassFactory : public IClassFactory, public NoThrowObject
{
private:
    RefCount ref;
    ClassType type;

public:
    ClassFactory(ClassType type) noexcept
        : type(type)
    {
    }

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

        IUnknown* inst;
        if (this->type == Class_PreviewHandler) {
            inst = static_cast<IPreviewHandler*>(new PreviewHandlerSVG());
        } else {
            inst = static_cast<IThumbnailProvider*>(new ThumbProviderSVG());
        }

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


bool IsBlackListed()
{
    HMODULE mod = ::GetModuleHandleW(nullptr);
    if (mod == nullptr) {
        return false;
    }
    bool blackListed = false;
    DWORD cbRequired = 0;
    auto lr = ::RegGetValueW(HKEY_CURRENT_USER, L"Software\\thumpsvg", L"BlackListedProcesses", RRF_RT_REG_MULTI_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, nullptr, &cbRequired);
    if (lr == ERROR_SUCCESS) {
        auto data = static_cast<PZZWSTR>(::calloc((cbRequired + 1) / 2, 2));
        if (data != nullptr) {
            DWORD cb = cbRequired;
            lr = ::RegGetValueW(HKEY_CURRENT_USER, L"Software\\thumpsvg", L"BlackListedProcesses", RRF_RT_REG_MULTI_SZ | RRF_ZEROONFAILURE | RRF_SUBKEY_WOW6464KEY, nullptr, data, &cbRequired);
            if (lr == ERROR_SUCCESS) {
                for (auto path = data; *path != L'\0' && blackListed == false; path += ::wcslen(path) + 1) {
                    blackListed = ::GetModuleHandleW(path) == mod;
                }
            }
            ::free(data);
        }
    }
    return blackListed;
}


#define String_CLSID_ThumbProviderSVG "{C9148B50-E15E-440d-A07F-CF385EC73914}"
#define String_CLSID_PreviewHandlerSVG "{9CDB270D-CB17-4d54-8F4F-7A9AE2D68392}"

// {C9148B50-E15E-440d-A07F-CF385EC73914}
//DEFINE_GUID(CLSID_ThumbProviderSVG, 0xc9148b50, 0xe15e, 0x440d, 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14);
const GUID CLSID_ThumbProviderSVG = { 0xc9148b50, 0xe15e, 0x440d, { 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14 } };

// {9CDB270D-CB17-4d54-8F4F-7A9AE2D68392}
//DEFINE_GUID(CLSID_PreviewHandlerSVG, 0x9cdb270d, 0xcb17, 0x4d54, 0x8f, 0x4f, 0x7a, 0x9a, 0xe2, 0xd6, 0x83, 0x92);
const GUID CLSID_PreviewHandlerSVG = { 0x9cdb270d, 0xcb17, 0x4d54, { 0x8f, 0x4f, 0x7a, 0x9a, 0xe2, 0xd6, 0x83, 0x92 } };

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

    ClassType type;
    if (IsEqualCLSID(rclsid, CLSID_ThumbProviderSVG)) {
        type = Class_ThumbProvider;
    } else if (IsEqualCLSID(rclsid, CLSID_PreviewHandlerSVG)) {
        type = Class_PreviewHandler;
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    if (IsBlackListed()) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ClassFactory* inst = new ClassFactory(type);
    HRESULT hr = inst != nullptr ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = inst->QueryInterface(riid, ppv);
        inst->Release();
    }

    return hr;
}

STDAPI DllInstall(BOOL install, LPCWSTR cmdLine)
{
    bool asUser = cmdLine != nullptr && ::StrCmpIW(cmdLine, L"user") == 0;

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
        char descThump[128], descPreview[128];
        if (::LoadStringA(HINST_THISCOMPONENT, IDS_DESC_THUMBNAIL, descThump, ARRAYSIZE(descThump)) <= 0) {
            strcpy_s(descThump, "?");
        }
        if (::LoadStringA(HINST_THISCOMPONENT, IDS_DESC_PREVIEW, descPreview, ARRAYSIZE(descPreview)) <= 0) {
            strcpy_s(descPreview, "?");
        }
        char clsidThump[] = String_CLSID_ThumbProviderSVG;
        char clsidPreview[] = String_CLSID_PreviewHandlerSVG;
        char k_clsidThump[] = "CLSID_ThumbProvider";
        char k_clsidPreview[] = "CLSID_PreviewHandler";
        char k_descThump[] = "DESC_ThumbProvider";
        char k_descPreview[] = "DESC_PreviewHandler";
        STRENTRYA ase[] =
        {
            { k_clsidThump, clsidThump },
            { k_clsidPreview, clsidPreview },
            { k_descThump, descThump },
            { k_descPreview, descPreview },
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
