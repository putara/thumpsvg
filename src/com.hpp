#ifndef COM_HELPER_H
#define COM_HELPER_H


#ifndef ASSERT
#ifdef _DEBUG
#define ASSERT(e)   do if (!(e)) { Debug::Print("%s(%d): Assertion failed\n", __FILE__, __LINE__); if (::IsDebuggerPresent()) { ::DebugBreak(); } } while(0)
#else
#define ASSERT(e)
#endif
#endif  // ASSERT


template <class T>
void IUnknown_SafeAssign(T*& dst, T* src) noexcept
{
    if (src != nullptr) {
        src->AddRef();
    }
    if (dst != nullptr) {
        dst->Release();
    }
    dst = src;
}

template <class T>
void IUnknown_SafeRelease(T*& unknown) noexcept
{
    if (unknown != nullptr) {
        unknown->Release();
        unknown = nullptr;
    }
}


template <class T>
class ComPtr
{
protected:
    mutable T* ptr;

    class NoAddRefRelease : public T
    {
    private:
        STDMETHOD_(ULONG, AddRef)() = 0;
        STDMETHOD_(ULONG, Release)() = 0;
    };

public:
    ComPtr() noexcept
        : ptr()
    {
    }
    ~ComPtr() noexcept
    {
        this->Release();
    }
    explicit ComPtr(T* p) noexcept
        : ptr(p)
    {
        if (p != nullptr) {
            p->AddRef();
        }
    }
    ComPtr(const ComPtr<T>& src) noexcept
        : ComPtr(src.p)
    {
    }
    ComPtr(ComPtr<T>&& src) noexcept
    {
        operator =(std::move(src));
    }
    T* Detach() noexcept
    {
        T* ptr = this->ptr;
        this->ptr = nullptr;
        return ptr;
    }
    void Release() noexcept
    {
        IUnknown_SafeRelease(this->ptr);
    }
    ComPtr<T>& operator =(T* src) noexcept
    {
        IUnknown_SafeAssign(this->ptr, src);
        return *this;
    }
    ComPtr<T>& operator =(const ComPtr<T>& src) noexcept
    {
        return operator =(src.ptr);
    }
    ComPtr<T>& operator =(ComPtr<T>&& src) noexcept
    {
        T* ptr = src.ptr;
        src.ptr = this->ptr;
        this->ptr = ptr;
        return *this;
    }
    operator T*() const noexcept
    {
        return this->ptr;
    }
    T* operator ->() const noexcept
    {
        ASSERT(this->ptr != nullptr);
        return static_cast<NoAddRefRelease*>(this->ptr);
    }
    T** operator &() noexcept
    {
        ASSERT(this->ptr == nullptr);
        return &this->ptr;
    }
    void CopyTo(T** outPtr) noexcept
    {
        ASSERT(this->ptr != nullptr);
        (*outPtr = this->ptr)->AddRef();
    }
    HRESULT CoCreateInstance(REFCLSID clsid, IUnknown* outer, DWORD context) noexcept
    {
        ASSERT(this->ptr == nullptr);
        return ::CoCreateInstance(clsid, outer, context, IID_PPV_ARGS(&this->ptr));
    }
    template <class Q>
    HRESULT QueryInterface(Q** outPtr) noexcept
    {
        return this->ptr->QueryInterface(IID_PPV_ARGS(outPtr));
    }
};


template <class T>
class ComAutoPtr
{
protected:
    mutable T* ptr;
    bool owns;

    void Free() noexcept
    {
        if (this->owns) {
            ::CoTaskMemFree(this->ptr);
            this->ptr = nullptr;
            this->owns = false;
        }
    }

public:
    ComAutoPtr() noexcept
        : ptr()
        , owns(false)
    {
    }
    explicit ComAutoPtr(T* p) noexcept
        : ptr(p)
        , owns(p != nullptr)
    {
    }
    ComAutoPtr(const ComAutoPtr<T>& src) noexcept
        : ptr(src.ptr)
        , owns(false)
    {
    }
    ~ComAutoPtr() noexcept
    {
        this->Free();
    }
    operator T* () noexcept
    {
        return this->ptr;
    }
    operator const T* () const noexcept
    {
        return this->ptr;
    }
    T* operator ->() const noexcept
    {
        return this->ptr;
    }
    T** operator &() noexcept
    {
        ASSERT(this->ptr == nullptr);
        return &this->ptr;
    }
    const T* Get() const noexcept
    {
        return this->ptr;
    }
    ComAutoPtr<T>& operator =(const ComAutoPtr<T>& src) noexcept
    {
        this->Free();
        this->ptr = src.ptr;
        this->owns = false;
        return this;
    }
};


#endif  // COM_HELPER_H
