#ifndef SVG_BRUSH_H
#define SVG_BRUSH_H

class Brush
{
protected:
    HBRUSH hbrush;

public:
    Brush()
    {
        this->hbrush = nullptr;
    }

    ~Brush()
    {
        this->Destroy();
    }

    explicit Brush(HBRUSH hbrush)
    {
        this->hbrush = hbrush;
    }

    Brush(Brush&& src)
    {
        this->hbrush = src.hbrush;
        src.hbrush = nullptr;
    }

    Brush& operator =(Brush&& src)
    {
        auto t = this->hbrush;
        this->hbrush = src.hbrush;
        src.hbrush = t;
        return *this;
    }

    Brush(const Brush&) = delete;
    Brush& operator =(const Brush&) = delete;

    HBRUSH GetHbrush() const
    {
        return this->hbrush;
    }

    bool IsNull() const
    {
        return this->hbrush == nullptr;
    }

    void Destroy()
    {
        if (this->hbrush != nullptr) {
            ::DeleteObject(this->hbrush);
            this->hbrush = nullptr;
        }
    }

    void Attach(HBRUSH hbrush)
    {
        this->Destroy();
        this->hbrush = hbrush;
    }

    HBRUSH Detach()
    {
        HBRUSH hbrush = this->hbrush;
        this->hbrush = nullptr;
        return hbrush;
    }

    HBRUSH* GetAddressOf()
    {
        this->Destroy();
        return &this->hbrush;
    }
};

#endif
