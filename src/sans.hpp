#ifndef SANS_HEADER
#define SANS_HEADER

#include <zlib.h>
#pragma comment(lib, "zlibstatic.lib")

// Replace BackgroundImage with SourceGraphic until the issue #257 is resolved
// https://github.com/RazrFalcon/resvg/issues/257

// The XML parser is based on NanoSVG, released under the zlib license
// Copyright (c) 2013-14 Mikko Mononen memon@inside.org
// https://github.com/memononen/nanosvg
class Sanitiser {
private:
    void* memPtr;
    size_t cbMem;

    static bool IsCharSpace(char c)
    {
        return ::strchr(" \t\n\v\f\r", c) != nullptr;
    }

    static bool IsProblematicTag(const char* tag, size_t len)
    {
        return ::strncmp(tag, "feBlend", len) == 0 || ::strncmp(tag, "feComposite", len) == 0 || ::strncmp(tag, "feDisplacementMap", len) == 0;
    }

    static bool IsProblematicAttr(const char* name, size_t len)
    {
        return ::strncmp(name, "in", len) == 0 || ::strncmp(name, "in2", len) == 0;
    }

    static bool IsProblematicValue(const char* value, size_t len)
    {
        return ::strncmp(value, "BackgroundImage", len) == 0;
    }

    static char* InElement(const char* in, const char* end, char* out)
    {
        // Skip white space after the '<'
        while (in < end && IsCharSpace(*in)) {
            *out++ = *in++;
        }
        // Skip end tag, comments, data and preprocessor stuff
        if (in < end && *in != '/' && *in != '?' && *in != '!') {
            // Get tag name
            const char* tag = in;
            while (in < end && !IsCharSpace(*in)) {
                *out++ = *in++;
            }
            if (in < end && IsProblematicTag(tag, in - tag)) {
                const char* s = in;
                while (s < end) {
                    // Skip white space before the attrib name
                    while (s < end && IsCharSpace(*s)) {
                        s++;
                    }
                    if (s >= end || *s == '/') {
                        break;
                    }
                    const char* name = s;
                    // Find end of the attrib name.
                    while (s < end && !IsCharSpace(*s) && *s != '=') {
                        s++;
                    }
                    const bool aoi = s < end && IsProblematicAttr(name, s - name);
                    // Skip until the beginning of the value.
                    while (s < end && *s != '\"' && *s != '\'') {
                        s++;
                    }
                    if (s >= end) {
                        break;
                    }
                    const char quote = *s++;
                    // Store value and find the end of it.
                    const char* value = s;
                    while (s < end && *s != quote) {
                        s++;
                    }
                    const size_t len = s - value;
                    if (s < end) {
                        s++;
                    }
                    if (aoi && IsProblematicValue(value, len)) {
                        while (in < value) {
                            *out++ = *in++;
                        }
                        ::memcpy(out, "SourceGraphic", 13);
                        out += 13;
                        *out++ = quote;
                        in = s;
                    } else {
                        while (in < s) {
                            *out++ = *in++;
                        }
                    }
                }
            }
        }
        while (in < end) {
            *out++ = *in++;
        }
        return out;
    }

    static size_t RunWorker(const char* in, size_t cb, char* out)
    {
        bool tag = false;
        const char* const end = in + cb;
        const char* mark = in;
        char* const outp = out;
        while (in < end) {
            if (*in == '<' && tag == false) {
                // Start of a tag
                in++;
                while (mark < in) {
                    *out++ = *mark++;
                }
                if (in >= end) {
                    break;
                }
                mark = in;
                tag = true;
            } else if (*in == '>' && tag) {
                // Start of a content or new tag.
                in++;
                out = InElement(mark, in, out);
                mark = in;
                tag = false;
            } else {
                in++;
            }
        }
        while (mark < in) {
            *out++ = *mark++;
        }
        return out - outp;
    }

    static void* GZipDecompress(const void* data, size_t cb, size_t* pcbOut)
    {
        *pcbOut = 0;
        Bytef* in = static_cast<Bytef*>(const_cast<void*>(data));
        if (cb < 2 || cb > INT_MAX || (in[0] | in[1] << 8) != 0x8b1f) {
            return nullptr;
        }
        z_stream zs = {};
        zs.next_in = in;
        int ret = inflateInit2(&zs, 15 | 32);
        if (ret < 0) {
            return nullptr;
        }
        zs.next_in = in;
        zs.avail_in = static_cast<uInt>(cb);
        Bytef* outPtr = nullptr;
        size_t size = 0;
        do {
            Bytef buf[2048];
            zs.next_out = buf;
            zs.avail_out = sizeof(buf);
            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_OK || ret == Z_STREAM_END) {
                const size_t cbWritten = sizeof(buf) - zs.avail_out;
                Bytef* newPtr = nullptr;
                if (SIZE_MAX - size >= cbWritten) {
                    newPtr = static_cast<Bytef*>(::realloc(outPtr, size + cbWritten));
                }
                if (newPtr == nullptr) {
                    ::free(outPtr);
                    return nullptr;
                }
                outPtr = newPtr;
                ::memcpy(outPtr + size, buf, cbWritten);
                size += cbWritten;
            } else {
                ::free(outPtr);
                return nullptr;
            }
        } while (ret != Z_STREAM_END && ret != Z_BUF_ERROR);
        inflateEnd(&zs);
        *pcbOut = size;
        return outPtr;
    }

public:
    Sanitiser()
    {
        this->memPtr = nullptr;
        this->cbMem = 0;
    }

    ~Sanitiser()
    {
        ::free(this->memPtr);
    }

    Sanitiser(Sanitiser&& src)
    {
        this->memPtr = src.memPtr;
        this->cbMem = src.cbMem;
        src.memPtr = nullptr;
    }

    Sanitiser(const Sanitiser&) = delete;
    Sanitiser& operator =(const Sanitiser&) = delete;

    const void* GetData() const
    {
        return this->memPtr;
    }

    size_t GetSize() const
    {
        return this->cbMem;
    }

    bool Run(const void* data, size_t cb)
    {
        ::free(this->memPtr);
        this->memPtr = nullptr;
        this->cbMem = 0;
        size_t cbDec;
        void* gzDec = GZipDecompress(data, cb, &cbDec);
        if (gzDec != nullptr) {
            data = gzDec;
            cb = cbDec;
        }
        char* outPtr = static_cast<char*>(calloc(cb + 1, 1));
        if (outPtr == nullptr) {
            return false;
        }
        cb = RunWorker(static_cast<const char*>(data), cb, outPtr);
        outPtr[cb] = '\0';
        ::free(gzDec);
        this->memPtr = outPtr;
        this->cbMem = cb;
        return true;
    }
};

#endif
