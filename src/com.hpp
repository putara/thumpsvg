#ifndef COM_HELPER_H
#define COM_HELPER_H


#ifndef ASSERT
#ifdef _DEBUG
#define ASSERT(e)   do if (!(e)) { printf("%s(%d): Assertion failed\n", __FILE__, __LINE__); if (::IsDebuggerPresent()) { ::DebugBreak(); } } while(0)
#else
#define ASSERT(e)
#endif
#endif  // ASSERT


template <class T>
void IUnknown_SafeAssign(T*& dst, T* src) throw()
{
    if (src != NULL) {
        src->AddRef();
    }
    if (dst != NULL) {
        dst->Release();
    }
    dst = src;
}

template <class T>
void IUnknown_SafeRelease(T*& unknown) throw()
{
    if (unknown != NULL) {
        unknown->Release();
        unknown = NULL;
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
    ComPtr() throw()
        : ptr()
    {
    }
    ~ComPtr() throw()
    {
        this->Release();
    }
    ComPtr(const ComPtr<T>& src) throw()
        : ptr()
    {
        operator =(src);
    }
    T* Detach() throw()
    {
        T* ptr = this->ptr;
        this->ptr = NULL;
        return ptr;
    }
    void Release() throw()
    {
        IUnknown_SafeRelease(this->ptr);
    }
    ComPtr<T>& operator =(T* src) throw()
    {
        IUnknown_SafeAssign(this->ptr, src);
        return *this;
    }
    operator T*() const throw()
    {
        return this->ptr;
    }
    T* operator ->() const throw()
    {
        ASSERT(this->ptr != NULL);
        return static_cast<NoAddRefRelease*>(this->ptr);
    }
    T** operator &() throw()
    {
        ASSERT(this->ptr == NULL);
        return &this->ptr;
    }
    void CopyTo(T** outPtr) throw()
    {
        ASSERT(this->ptr != NULL);
        (*outPtr = this->ptr)->AddRef();
    }
    HRESULT CoCreateInstance(REFCLSID clsid, IUnknown* outer, DWORD context) throw()
    {
        ASSERT(this->ptr == NULL);
        return ::CoCreateInstance(clsid, outer, context, IID_PPV_ARGS(&this->ptr));
    }
    template <class Q>
    HRESULT QueryInterface(Q** outPtr) throw()
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

    void Free() throw()
    {
        if (this->owns) {
            ::CoTaskMemFree(this->ptr);
            this->ptr = NULL;
            this->owns = false;
        }
    }

public:
    ComAutoPtr() throw()
        : ptr()
        , owns(false)
    {
    }
    explicit ComAutoPtr(T* p) throw()
        : ptr(p)
        , owns(p != NULL)
    {
    }
    ComAutoPtr(const ComAutoPtr<T>& src) throw()
        : ptr(src.ptr)
        , owns(false)
    {
    }
    ~ComAutoPtr() throw()
    {
        this->Free();
    }
    operator T* () throw()
    {
        return this->ptr;
    }
    operator const T* () const throw()
    {
        return this->ptr;
    }
    T* operator ->() const throw()
    {
        return this->ptr;
    }
    T** operator &() throw()
    {
        ASSERT(this->ptr == NULL);
        return &this->ptr;
    }
    const T* Get() const throw()
    {
        return this->ptr;
    }
    ComAutoPtr<T>& operator =(const ComAutoPtr<T>& src) throw()
    {
        this->Free();
        this->ptr = src.ptr;
        this->owns = false;
        return this;
    }
};


#endif  // COM_HELPER_H
