#ifndef SVG_DEBUG_H
#define SVG_DEBUG_H

class Debug
{
private:
    static bool allocated;

    static void AllocConsoleIfNotYet()
    {
        if (!allocated) {
            ::AllocConsole();
            allocated = true;
        }
    }

public:
    Debug() = delete;
    ~Debug() = delete;

    static void Print(const char* format, ...) throw()
    {
        AllocConsoleIfNotYet();
        char buffer[1025];
        va_list argPtr;
        va_start(argPtr, format);
        ::StringCchVPrintfA(buffer, ARRAYSIZE(buffer), format, argPtr);
        ::WriteConsoleA(::GetStdHandle(STD_OUTPUT_HANDLE), buffer, static_cast<DWORD>(::strlen(buffer)), nullptr, nullptr);
        va_end(argPtr);
    }

    static void Print(const wchar_t* format, ...) throw()
    {
        AllocConsoleIfNotYet();
        wchar_t buffer[1025];
        va_list argPtr;
        va_start(argPtr, format);
        ::StringCchVPrintfW(buffer, ARRAYSIZE(buffer), format, argPtr);
        ::WriteConsoleW(::GetStdHandle(STD_OUTPUT_HANDLE), buffer, static_cast<DWORD>(::wcslen(buffer)), nullptr, nullptr);
        va_end(argPtr);
    }
};

bool Debug::allocated;

#endif
