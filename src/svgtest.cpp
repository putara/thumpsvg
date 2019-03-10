#define WINVER          0x600
#define _WIN32_WINNT    0x600
#define _WIN32_IE       0x0700

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include "sdk.h"
#include "com.hpp"
#include "gdi.hpp"

#include <stdio.h>

typedef HRESULT (STDAPICALLTYPE* DLLGETCLASSOBJECT)(REFCLSID, REFIID, void**);

const GUID CLSID_ThumbProviderSVG = { 0xc9148b50, 0xe15e, 0x440d, { 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14 } };


HRESULT CreateBindCtxWithMode(DWORD grfMode, IBindCtx** ppbc)
{
    HRESULT hr = ::CreateBindCtx(0, ppbc);
    if (SUCCEEDED(hr)) {
        BIND_OPTS opts = { sizeof(opts), 0, grfMode, 0 };
        (*ppbc)->SetBindOptions(&opts);
    }
    return hr;
}

HRESULT GrabFilesAsShellItemArray(const wchar_t* dir, IShellItemArray** array)
{
    *array = NULL;
    ComPtr<IShellItem> item;
    ComPtr<INamespaceWalk> walk;
    UINT cItems = 0;
    PIDLIST_ABSOLUTE* ppidls = NULL;

    HRESULT hr = ::SHCreateItemFromParsingName(dir, NULL, IID_PPV_ARGS(&item));
    if (FAILED(hr)) {
        printf("SHCreateItemFromParsingName() failed with 0x%08x\n", hr);
        return hr;
    }
    hr = ::CoCreateInstance(CLSID_NamespaceWalker, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&walk));
    if (SUCCEEDED(hr)) {
        hr = walk->Walk(item, NSWF_DONT_RESOLVE_LINKS | NSWF_NONE_IMPLIES_ALL | NSWF_FILESYSTEM_ONLY, MAXLONG, NULL);
        if (SUCCEEDED(hr)) {
            hr = walk->GetIDArrayResult(&cItems, &ppidls);
            if (SUCCEEDED(hr)) {
                hr = ::SHCreateShellItemArrayFromIDLists(cItems, const_cast<PCIDLIST_ABSOLUTE_ARRAY>(ppidls), array);
                if (FAILED(hr)) {
                    printf("SHCreateShellItemArrayFromIDLists() failed with 0x%08x\n", hr);
                }
            } else {
                printf("INamespaceWalk::GetIDArrayResult() failed with 0x%08x\n", hr);
            }
        } else {
            printf("INamespaceWalk::Walk() failed with 0x%08x\n", hr);
        }
    } else {
        printf("CoCreateInstance(CLSID_NamespaceWalker) failed with 0x%08x\n", hr);
    }
    if (ppidls != NULL) {
        FreeIDListArrayFull(ppidls, cItems);
    }
    return hr;
}

HRESULT OpenStreamAsReadonly(IShellItem* item, IStream** stream)
{
    *stream = NULL;
    ComPtr<IBindCtx> ctx;
    HRESULT hr = CreateBindCtxWithMode(STGM_READ, &ctx);
    if (FAILED(hr)) {
        printf("CreateBindCtx() failed with 0x%08x\n", hr);
        return hr;
    }
    hr = item->BindToHandler(ctx, BHID_Stream, IID_PPV_ARGS(stream));
    if (FAILED(hr)) {
        printf("IShellItem::BindToHandler() failed with 0x%08x\n", hr);
    }
    return hr;
}

void runfile(DLLGETCLASSOBJECT dllGetClassObject, IShellItem* item, int size)
{
    ComAutoPtr<wchar_t> path;
    HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
    if (FAILED(hr)) {
        return;
    }
    wchar_t* ext = ::PathFindExtensionW(path);
    if (::StrCmpICW(ext, L".svg") != 0 && ::StrCmpICW(ext, L".svgz") != 0) {
        return;
    }
    ComPtr<IStream> stream;
    hr = OpenStreamAsReadonly(item, &stream);
    if (FAILED(hr)) {
        return;
    }
    ComPtr<IClassFactory> factory;
    hr = dllGetClassObject(CLSID_ThumbProviderSVG, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        printf("DllGetClassObject() failed with 0x%08x\n", hr);
        return;
    }
    ComPtr<IThumbnailProvider> thump;
    hr = factory->CreateInstance(NULL, IID_PPV_ARGS(&thump));
    if (SUCCEEDED(hr)) {
        ComPtr<IInitializeWithStream> init;
        hr = thump->QueryInterface(IID_PPV_ARGS(&init));
        if (SUCCEEDED(hr)) {
            hr = init->Initialize(stream, STGM_READ);
            if (SUCCEEDED(hr)) {
                Bitmap bmp;
                WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
                hr = thump->GetThumbnail(static_cast<UINT>(size), bmp.GetAddressOf(), &alpha);
                if (SUCCEEDED(hr)) {
                    if (bmp.Get() != NULL) {
                        printf("%S: All good!\n", path.Get());
                    } else {
                        printf("%S: IThumbnailProvider::GetThumbnail() succeeded but hbmp = NULL\n", path.Get());
                    }
                    if (alpha == WTSAT_UNKNOWN) {
                        printf("%S: IThumbnailProvider::GetThumbnail() succeeded but alpha = UNKNOWN\n", path.Get());
                    }
                } else {
                    printf("%S: IThumbnailProvider::GetThumbnail() failed with 0x%08x\n", path.Get(), hr);
                }
            } else {
                printf("%S: IInitializeWithStream::Initialize() failed with 0x%08x\n", path.Get(), hr);
            }
        } else {
            printf("IThumbnailProvider::QueryInterface() failed with 0x%08x\n", hr);
        }
    } else {
        printf("IClassFactory::CreateInstance() failed with 0x%08x\n", hr);
    }
}

void run(DLLGETCLASSOBJECT dllGetClassObject, const wchar_t* dir, int size)
{
    printf("Scanning '%S' ...\n", dir);
    ComPtr<IShellItemArray> array;
    DWORD count = 0;
    HRESULT hr = GrabFilesAsShellItemArray(dir, &array);
    if (SUCCEEDED(hr)) {
        hr = array->GetCount(&count);
    }
    if (SUCCEEDED(hr)) {
        for (DWORD i = 0; i < count; i++) {
            ComPtr<IShellItem> item;
            if (FAILED(array->GetItemAt(i, &item))) {
                break;
            }
            runfile(dllGetClassObject, item, size);
        }
    }
}

int main()
{
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    wchar_t path[MAX_PATH];
    HRESULT hr = ::OleInitialize(NULL);
    if (FAILED(hr)) {
        printf("OleInitialize() failed with 0x%08x\n", hr);
        return 1;
    }

    ::GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    ::PathAppendW(path, L"..\\thumpsvg.dll");

    HMODULE hmod = ::LoadLibraryW(path);
    if (hmod == NULL) {
        printf("LoadLibrary() failed with %u\n", ::GetLastError());
        ::OleUninitialize();
        return 1;
    }

    DLLGETCLASSOBJECT dllGetClassObject = reinterpret_cast<DLLGETCLASSOBJECT>(::GetProcAddress(hmod, "DllGetClassObject"));
    if (dllGetClassObject == NULL) {
        printf("LoadLibrary() failed with %u\n", ::GetLastError());
        ::FreeLibrary(hmod);
        ::OleUninitialize();
        return 1;
    }

    ::GetFullPathNameW(argc < 2 ? L"." : argv[1], ARRAYSIZE(path), path, NULL);
    int size = argc < 3 ? 128 : ::StrToIntW(argv[2]);
    if (size <= 0 || size > 256) {
        size = 128;
    }
    run(dllGetClassObject, path, size);

    ::FreeLibrary(hmod);
    ::OleUninitialize();
    return 0;
}
