#include "os.h"

#include <stdexcept>
#include <string>
#include <vector>

extern int unimain(const std::vector<std::string> &args);

#if defined OS_W32
namespace W32
{
std::string fromWide(const wchar_t *wstr)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, 0, 0, 0, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        if (WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buffer.data(), size, 0, 0) == size)
            return std::string(buffer.begin(), buffer.end() - 1);
    }
    return "";
}

std::wstring toWide(const char *str)
{
    if (str[0]) {
        int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
        if (size > 0) {
            std::vector<wchar_t> buffer(size);
            if (MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer.data(), size) == size)
                return std::wstring(buffer.begin(), buffer.end() - 1);
        }
    }
    return L"";
}
}

std::string unicodeCompatPath(const std::string &str)
{
    std::wstring wstr = W32::toWide(str);
    DWORD size = GetShortPathNameW(wstr.c_str(), NULL, 0);
    if (size == 0)
        throw std::runtime_error(str + ": file does not exist");
    std::vector<wchar_t> shortstr(size);
    GetShortPathNameW(wstr.c_str(), shortstr.data(), size);
    return W32::fromWide(std::wstring(shortstr.begin(), shortstr.end() - 1));
}

int main()
{
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args(argc - 1);
    for (int i = 1; i < argc; ++i)
        args[i - 1] = W32::fromWide(argv[i]);
    LocalFree(argv);
    return unimain(args);
}

#elif defined OS_UNIX

int main(int argc, char **argv)
{
    std::vector<std::string> args(argc - 1);
    for (int i = 1; i < argc; ++i)
        args[i - 1] = argv[i];
    return unimain(args);
}

#endif
