#ifndef SVG_UTF8STRING_H
#define SVG_UTF8STRING_H


class Utf8String
{
private:
    char* str = nullptr;

public:
    Utf8String(LPCWSTR u16str) noexcept
    {
        int cchUtf8 = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, u16str, -1, nullptr, 0, nullptr, nullptr);
        if (cchUtf8 > 0) {
            this->str = static_cast<char*>(::calloc(1, cchUtf8));
            if (this->str != nullptr) {
                *this->str = '\0';
                ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, u16str, -1, this->str, cchUtf8, nullptr, nullptr);
            }
        }
    }

    ~Utf8String() noexcept
    {
        ::free(this->str);
    }

    Utf8String(Utf8String&& source) noexcept
    {
        this->str = source.str;
        source.str = nullptr;
    }

    Utf8String& operator =(Utf8String&& source) noexcept
    {
        auto t = this->str;
        this->str = source.str;
        source.str = t;
        return *this;
    }

    Utf8String(const Utf8String&) = delete;
    Utf8String& operator =(const Utf8String&) = delete;

    const char* Get() const noexcept
    {
        return this->str != nullptr ? this->str : "";
    }
};

#endif
