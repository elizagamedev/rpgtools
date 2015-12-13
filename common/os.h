#ifndef OS_H
#define OS_H

#include <fstream>
#include <vector>
#include <iostream>

#if defined _WIN32
    //#define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define OS_W32
#elif defined __APPLE__ && __MACH__
    #define OS_UNIX
    #define OS_OSX
#elif defined __linux__
    #define OS_UNIX
    #define OS_LINUX
#else
    #define OS_UNIX
#endif

#if defined OS_W32
/* WINDOWS */
#include <string>

namespace W32
{
std::wstring toWide(const char *str);
std::string fromWide(const WCHAR *wstr);
static inline std::wstring toWide(const std::string &str) { return toWide(str.c_str()); }
static inline std::string fromWide(const std::wstring &wstr) { return fromWide(wstr.c_str()); }
}

/* UNICODE */
#define PATH_SEPARATOR "\\"
typedef WCHAR unichar;
#define U(str) (const_cast<const unichar*>(L##str))
std::string unicodeCompatPath(const std::string &path);

#ifdef _MSC_VER
/* UNICODE FILE I/O */
class ifstream : public std::ifstream
{
public:
    ifstream() :
        std::ifstream()
    {
        exceptions(std::ifstream::failbit | std::ifstream::badbit);
    }

    explicit ifstream(const char* fileName) :
        std::ifstream(W32::toWide(fileName), std::ios::in | std::ios::binary)
    {
        exceptions(std::ifstream::failbit | std::ifstream::badbit);
    }

    void open(const char *fileName)
    {
        std::ifstream::open(fileName, std::ios::in | std::ios::binary);
    }
};

class ofstream : public std::ofstream
{
public:
    ofstream() :
        std::ofstream()
    {
        exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    explicit ofstream(const char* fileName) :
        std::ofstream(W32::toWide(fileName), std::ios::out | std::ios::binary)
    {
        exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    void open(const char *fileName)
    {
        std::ofstream::open(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    }
};
#else
#include <ext/stdio_filebuf.h>
#include <fcntl.h>
#include <sys/stat.h>

/* UNICODE FILE I/O */
class ifstream : public std::istream
{
public:
    ifstream() :
        std::istream()
    {
    }

    ~ifstream()
    {
        delete rdbuf();
    }

    explicit ifstream(const char* fileName) :
        std::istream()
    {
        open(fileName);
    }

    void open(const char *fileName)
    {
        delete rdbuf();
        rdbuf(new __gnu_cxx::stdio_filebuf<char>(_wopen(W32::toWide(fileName).c_str(), _O_RDONLY | _O_BINARY),
                                                 std::ios::in | std::ios::binary));
        exceptions(std::istream::failbit | std::istream::badbit);
    }

    bool is_open()
    {
        return rdbuf() != NULL;
    }
};

class ofstream : public std::ostream
{
public:
    ofstream() :
        std::ostream()
    {
    }

    ~ofstream()
    {
        delete rdbuf();
    }

    explicit ofstream(const char* fileName) :
        std::ostream()
    {
        open(fileName);
    }

    void open(const char *fileName)
    {
        delete rdbuf();
        rdbuf(new __gnu_cxx::stdio_filebuf<char>(_wopen(W32::toWide(fileName).c_str(),
                                                        _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC,
                                                        _S_IREAD | _S_IWRITE),
                                                 std::ios::out | std::ios::binary | std::ios::trunc));
        exceptions(std::istream::failbit | std::istream::badbit);
    }

    bool is_open()
    {
        return rdbuf() != NULL;
    }
};
#endif

#elif defined OS_UNIX

/* UNICODE */
#define PATH_SEPARATOR "/"
typedef char unichar;
#define U(str) (const_cast<const unichar*>(str))
#define unicodeCompatPath(path) (path)

/* UNICODE FILE I/O */
class ifstream : public std::ifstream
{
public:
    ifstream() :
        std::ifstream()
    {
        exceptions(std::ifstream::failbit | std::ifstream::badbit);
    }

    explicit ifstream(const char* fileName) :
        std::ifstream(fileName, std::ios::in | std::ios::binary)
    {
        exceptions(std::ifstream::failbit | std::ifstream::badbit);
    }

    void open(const char *fileName)
    {
        std::ifstream::open(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    }
};

class ofstream : public std::ofstream
{
public:
    ofstream() :
        std::ofstream()
    {
        exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    explicit ofstream(const char* fileName) :
        std::ofstream(fileName, std::ios::out | std::ios::binary | std::ios::trunc)
    {
        exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    void open(const char *fileName)
    {
        std::ofstream::open(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    }
};

#endif

#endif // OS_H
