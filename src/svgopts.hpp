#ifndef SVG_OPTIONS_H
#define SVG_OPTIONS_H

#include "utf8str.hpp"

class SvgOptions
{
private:
    resvg_options* opt = nullptr;
    bool workaround = true;

public:
    SvgOptions() noexcept
    {
        this->opt = ::resvg_options_create();
        (*this)
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

    SvgOptions& SetResourcesDir(LPCWSTR path) noexcept
    {
        if (this->opt != nullptr) {
            Utf8String u8path(path);
            ::resvg_options_set_resources_dir(this->opt, u8path.Get());
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

    SvgOptions& SetSpeedOverQuality(bool enabled) noexcept
    {
        if (enabled) {
            ::resvg_options_set_shape_rendering_mode(this->opt, RESVG_SHAPE_RENDERING_OPTIMIZE_SPEED);
            ::resvg_options_set_text_rendering_mode(this->opt, RESVG_TEXT_RENDERING_OPTIMIZE_SPEED);
            ::resvg_options_set_image_rendering_mode(this->opt, RESVG_IMAGE_RENDERING_OPTIMIZE_SPEED);
        } else {
            ::resvg_options_set_shape_rendering_mode(this->opt, RESVG_SHAPE_RENDERING_GEOMETRIC_PRECISION);
            ::resvg_options_set_text_rendering_mode(this->opt, RESVG_TEXT_RENDERING_OPTIMIZE_LEGIBILITY);
            ::resvg_options_set_image_rendering_mode(this->opt, RESVG_IMAGE_RENDERING_OPTIMIZE_QUALITY);
        }
        return *this;
    }

    resvg_options* GetOptions() const noexcept
    {
        return this->opt;
    }

    SvgOptions& SetWorkaround(bool enabled) noexcept {
        this->workaround = enabled;
        return *this;
    }

    bool GetWorkaround() const noexcept
    {
        return this->workaround;
    }
};

#endif
