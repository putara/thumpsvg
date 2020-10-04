#ifndef SVG_OPTIONS_H
#define SVG_OPTIONS_H

#include "utf8str.hpp"

class SvgOptions
{
private:
    resvg_options* opt = nullptr;

public:
    SvgOptions() noexcept
    {
        this->opt = ::resvg_options_create();
        (*this)
            .SetFilePath(nullptr)
            // TODO: dpi aware
            .SetDpi(96.)
            // TODO: use EnumFontFamilies()
            .SetFontFamily("Microsoft Sans Serif")
            .SetFontSize(12.)
            .SetMonospaceFamily("Consolas")
            // TODO: use GetSystemPreferredUILanguages()
            .SetLanguages("en");
    }

    ~SvgOptions() noexcept
    {
        if (this->opt) {
            ::resvg_options_destroy(this->opt);
        }
    }

    SvgOptions(SvgOptions&& src) noexcept
    {
        this->opt = src.opt;
        src.opt = nullptr;
    }

    SvgOptions(const SvgOptions&) = delete;
    SvgOptions& operator =(const SvgOptions&) = delete;

    SvgOptions& LoadSystemFonts() noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_load_system_fonts(this->opt);
        }
        return *this;
    }

    SvgOptions& SetDpi(double dpi) noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_set_dpi(this->opt, dpi);
        }
        return *this;
    }

    SvgOptions& SetFontSize(double size) noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_set_font_size(this->opt, size);
        }
        return *this;
    }

    SvgOptions& SetFilePath(LPCWSTR path) noexcept
    {
        if (this->opt != nullptr) {
            Utf8String u8path(path);
            ::resvg_options_set_file_path(this->opt, u8path.Get());
        }
        return *this;
    }

    SvgOptions& SetFontFamily(LPCWSTR name) noexcept
    {
        if (this->opt != nullptr) {
            Utf8String u8name(name);
            ::resvg_options_set_font_family(this->opt, u8name.Get());
        }
        return *this;
    }

    SvgOptions& SetFontFamily(const char* name) noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_set_font_family(this->opt, name);
        }
        return *this;
    }

    SvgOptions& SetMonospaceFamily(LPCWSTR name) noexcept
    {
        if (this->opt != nullptr) {
            Utf8String u8name(name);
            ::resvg_options_set_monospace_family(this->opt, u8name.Get());
        }
        return *this;
    }

    SvgOptions& SetMonospaceFamily(const char* name) noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_set_monospace_family(this->opt, name);
        }
        return *this;
    }

    SvgOptions& SetLanguages(LPCWSTR langs) noexcept
    {
        if (this->opt != nullptr) {
            Utf8String u8langs(langs);
            ::resvg_options_set_languages(opt, u8langs.Get());
        }
        return *this;
    }

    SvgOptions& SetLanguages(const char* langs) noexcept
    {
        if (this->opt != nullptr) {
            ::resvg_options_set_languages(this->opt, langs);
        }
        return *this;
    }

    resvg_options* GetOptions() const noexcept
    {
        return this->opt;
    }
};

#endif
