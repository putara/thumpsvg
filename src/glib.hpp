#ifndef GLIB_XX_HEADER
#define GLIB_XX_HEADER


namespace G
{

class Error
{
protected:
    mutable GError* error;

private:
    Error(const Error&);
    Error& operator =(const Error&);

public:
    Error()
        : error()
    {
    }
    ~Error()
    {
        this->Clear();
    }
    void Clear()
    {
        if (this->error != NULL) {
            ::g_error_free(this->error);
            this->error = NULL;
        }
    }
    operator bool() const
    {
        return this->error != NULL;
    }
    GError** operator &()
    {
        this->Clear();
        return &this->error;
    }
    GError* Get() const
    {
        return this->error;
    }
};


class MemInputStream
{
protected:
    mutable GInputStream* stream;

    void Close()
    {
        if (this->stream != NULL) {
            ::g_object_unref(this->stream);
            this->stream = NULL;
        }
    }

private:
    MemInputStream(const MemInputStream&);
    MemInputStream& operator =(const MemInputStream&);

public:
    MemInputStream(const void* ptr, size_t cb)
        : stream(::g_memory_input_stream_new_from_data(ptr, cb, NULL))
    {
    }
    ~MemInputStream()
    {
        this->Close();
    }
    operator bool() const
    {
        return this->stream != NULL;
    }
    GInputStream* Get() const
    {
        return this->stream;
    }
};


class Cancellable
{
protected:
    GCancellable* cancellable;

private:
    Cancellable(const Cancellable&);
    Cancellable& operator =(const Cancellable&);

public:
    Cancellable()
        : cancellable(::g_cancellable_new())
    {
    }
    ~Cancellable()
    {
        if (this->cancellable != NULL) {
            ::g_object_unref(this->cancellable);
        }
    }
    void Cancel()
    {
        if (this->cancellable != NULL) {
            ::g_cancellable_cancel(this->cancellable);
        }
    }
    GCancellable* Get() const
    {
        return this->cancellable;
    }
};


}


#endif  // GLIB_XX_HEADER
