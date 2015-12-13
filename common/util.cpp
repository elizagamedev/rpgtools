#include "util.h"

#if defined OS_W32
#include <shellapi.h>
#elif defined OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <dirent.h>
#include <unistd.h>
#ifndef NOUCONV
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#endif
#endif

#include <stdexcept>
#include <algorithm>
#include <cassert>

namespace Util
{
/* PRIVATE FUNCS */
static inline bool isDotOrDotDot(const unichar *str)
{
    if (str[0] == '.') {
        if (str[1] == 0)
            return true;
        if (str[1] == '.' && str[2] == 0)
            return true;
    }
    return false;
}

#if defined OS_W32

FILE *fopen(const std::string &str, const unichar *args)
{
    return _wfopen(W32::toWide(str).c_str(), const_cast<const WCHAR*>(args));
}

void mkdir(const std::string &dirname)
{
    CreateDirectoryW(W32::toWide(dirname).c_str(), NULL);
}

void mkdirsForFile(const std::string &filename)
{
    std::wstring tmp = W32::toWide(filename);
    for (unsigned int i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            tmp[i] = 0;
            CreateDirectoryW(tmp.c_str(), NULL);
            tmp[i] = '\\';
        }
    }
}

bool dirExists(const std::string &dirname)
{
    DWORD attrib = GetFileAttributesW(W32::toWide(dirname).c_str());
    return attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::string> listFiles(const std::string &path)
{
    assert(*path.rbegin() == PATH_SEPARATOR[0]);
    WIN32_FIND_DATAW fd;
    HANDLE hFind = NULL;

    std::vector<std::string> list;
    if ((hFind = FindFirstFileW(W32::toWide(path + "*").c_str(), &fd)) == INVALID_HANDLE_VALUE)
        throw std::runtime_error(path + ": could not list files");
    do {
        if (!isDotOrDotDot(fd.cFileName))
            list.push_back(W32::fromWide(fd.cFileName));
    } while(FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return list;
}

size_t getFileSize(const std::string &filename)
{
    //TODO getFileSize windows
    HANDLE hFile = CreateFileW(W32::toWide(filename).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);
    return static_cast<size_t>(size.QuadPart);
}

void deleteFile(const std::string &filename)
{
    DeleteFileW(W32::toWide(filename).c_str());
}

void deleteFolder(const std::string &filename)
{
    std::wstring wfilename = W32::toWide(filename);
    SHFILEOPSTRUCTW ops = {
        0,
        FO_DELETE,
        wfilename.c_str(),
        NULL,
        FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
        FALSE,
        NULL,
        NULL,
    };
    SHFileOperationW(&ops);
}

std::string sanitizeDirPath(std::string path)
{
    path = sanitizePath(path);
    if (*path.rbegin() != PATH_SEPARATOR[0])
        path.append(PATH_SEPARATOR);
    return path;
}

std::string sanitizePath(std::string path)
{
    for (;;) {
        size_t index = path.find('/');
        if (index == std::string::npos)
            break;
        path.replace(index, 1, "\\");
    }
    return path;
}

#elif defined OS_UNIX

FILE *fopen(const std::string &str, const unichar *ops)
{
    return ::fopen(str.c_str(), const_cast<const char*>(ops));
}

void mkdir(const std::string &filename)
{
    ::mkdir(filename.c_str(), 0777);
}

void mkdirsForFile(const std::string &filename)
{
    std::string tmp = filename;
    for (unsigned int i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            ::mkdir(tmp.c_str(), 0777);
            tmp[i] = '/';
        }
    }
}

bool dirExists(const std::string &dirname)
{
    struct stat st;
    return stat(dirname.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> listFiles(const std::string &path)
{
    assert(*path.rbegin() == PATH_SEPARATOR[0]);
    std::vector<std::string> list;
    DIR *dp = opendir(path.c_str());
    if (dp == NULL)
        throw std::runtime_error(path + ": could not list files");

    //Populate list
    dirent *ep;
    while ((ep = readdir(dp)) != NULL) {
        //TODO OS X UTF-8 conversion!
        if (!isDotOrDotDot(ep->d_name))
            list.push_back(ep->d_name);
    }
    closedir(dp);

    return list;
}

size_t getFileSize(const std::string &filename)
{
    struct stat st;
    stat(filename.c_str(), &st);
    return st.st_size;
}

void deleteFile(const std::string &filename)
{
    unlink(filename.c_str());
}

int deleteFolder_func(const char *filename, const struct stat *st, int flags, struct FTW *fwt)
{
    UNUSED(st);
    UNUSED(flags);
    UNUSED(fwt);
    return remove(filename);
}

void deleteFolder(const std::string &filename)
{
    nftw(filename.c_str(), deleteFolder_func, 64, FTW_DEPTH | FTW_PHYS);
}

std::string sanitizeDirPath(std::string path)
{
    if (*path.rbegin() != PATH_SEPARATOR[0])
        path.append(PATH_SEPARATOR);
    return path;
}

#endif

std::string getExtension(const std::string &filename)
{
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos)
        return "";
    return toLower(filename.substr(dot + 1));
}

std::string getWithoutExtension(const std::string &filename)
{
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos)
        return filename;
    return filename.substr(0, dot);
}

std::string readFileContents(const std::string &filename)
{
    size_t size = Util::getFileSize(filename);
    ifstream file(filename.c_str());
    std::vector<char> data(size);
    file.read(data.data(), size);
    return std::string(data.begin(), data.end());
}

void copyFile(const std::string &src, const std::string &dst)
{
    ifstream fsrc(src.c_str());
    ofstream fdst(dst.c_str());
    fdst << fsrc.rdbuf();
}

/******************
 * SHIFT-JIS SHIT *
 ******************/

#define YEN "\u00A5"

#ifdef OS_W32
#define ENC_UTF8 CP_UTF8
#define ENC_SJIS 932
static std::string encode(const std::string &string, int from, int to, bool tolower)
{
    if (string.empty())
        return std::string();

    //Convert to wide
    int size = MultiByteToWideChar(from, 0, string.data(), string.size(), NULL, 0);
    if (size <= 0)
        throw std::runtime_error("codepage conversion error");

    std::vector<wchar_t> wbuffer(size);
    if (MultiByteToWideChar(from, 0, string.data(), string.size(), wbuffer.data(), wbuffer.size()) != size)
        throw std::runtime_error("codepage conversion error");

    //Convert case
    if (tolower)
        CharLowerBuffW(wbuffer.data(), wbuffer.size());

    //Convert to UTF-8
    size = WideCharToMultiByte(to, 0, wbuffer.data(), wbuffer.size(), NULL, 0, NULL, NULL);
    if (size <= 0)
        throw std::runtime_error("codepage conversion error");

    std::vector<char> buffer(size);
    if (WideCharToMultiByte(to, 0, wbuffer.data(), wbuffer.size(), buffer.data(), buffer.size(), NULL, NULL) != size)
        throw std::runtime_error("codepage conversion error");

    return std::string(buffer.begin(), buffer.end());
}
#else
#ifdef UCONV
#define ENC_UTF8 "utf8"
#define ENC_SJIS "Windows-31J"
static std::string encode(const std::string &string, const char *from, const char *to, bool tolower)
{
    if (string.empty())
        return std::string();

    UConverter *conv;
    UErrorCode status = U_ZERO_ERROR;
    int size;

    /* TO UNICODE */
    conv = ucnv_open(from, &status);
    if (status != U_ZERO_ERROR) {
        ucnv_close(conv);
        throw std::runtime_error("could not open uconv object");
    }

    //Calculate size
    size = ucnv_toUChars(conv, NULL, 0, string.data(), string.size(), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        ucnv_close(conv);
        throw std::runtime_error("\"" + string + "\": conversion error");
    }

    //Convert
    status = U_ZERO_ERROR;
    std::vector<UChar> ubuffer(size);
    ucnv_toUChars(conv, ubuffer.data(), size, string.data(), string.size(), &status);
    ucnv_close(conv);
    if (status != U_STRING_NOT_TERMINATED_WARNING)
        throw std::runtime_error("\"" + string + "\": conversion error");

    /* TO LOWERCASE */
    if (tolower) {
        //Get size
        status = U_ZERO_ERROR;
        size = u_strToLower(NULL, 0, ubuffer.data(), ubuffer.size(), "", &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            throw std::runtime_error("\"" + string + "\": could not convert to lowercase");

        //Convert
        status = U_ZERO_ERROR;
        std::vector<UChar> ubufferMixed(size);
        std::swap(ubuffer, ubufferMixed);
        u_strToLower(ubuffer.data(), size, ubufferMixed.data(), ubufferMixed.size(), "", &status);
        if (status != U_STRING_NOT_TERMINATED_WARNING)
            throw std::runtime_error("\"" + string + "\": could not convert to lowercase");
    }

    /* TO DESTINATION */
    status = U_ZERO_ERROR;
    conv = ucnv_open(to, &status);
    if (status != U_ZERO_ERROR) {
        ucnv_close(conv);
        throw std::runtime_error("could not open uconv object");
    }

    //Calculate size
    status = U_ZERO_ERROR;
    size = ucnv_fromUChars(conv, NULL, 0, ubuffer.data(), ubuffer.size(), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        ucnv_close(conv);
        throw std::runtime_error("\"" + string + "\": conversion error");
    }

    //Convert
    status = U_ZERO_ERROR;
    std::vector<char> buffer(size);
    size = ucnv_fromUChars(conv, buffer.data(), size, ubuffer.data(), ubuffer.size(), &status);
    ucnv_close(conv);
    if (status != U_STRING_NOT_TERMINATED_WARNING)
        throw std::runtime_error("\"" + string + "\": conversion error");

    return std::string(buffer.data(), size);
}
#endif
#endif

#if !defined UCONV && defined OS_UNIX
std::string toLower(std::string string)
{
    std::transform(string.begin(), string.end(), string.begin(), ::tolower);
    return string;
}
#else
std::string fromJis(const std::string &jstring)
{
    std::string string = encode(jstring, ENC_SJIS, ENC_UTF8, false);
    //Replace yen with backslash
    size_t pos = 0;
    while ((pos = string.find(YEN, pos)) != std::string::npos) {
        string.replace(pos, ARRAY_SIZE(YEN) - 1, "\\");
        ++pos;
    }
    return string;
}

std::string toJis(std::string string)
{
    //Replace backslash with yen
    size_t pos = 0;
    while ((pos = string.find('\\', pos)) != std::string::npos) {
        string.replace(pos, 1, YEN);
        pos += ARRAY_SIZE(YEN) - 1;
    }
    return encode(string, ENC_UTF8, ENC_SJIS, false);
}

std::string fromJisToLower(const std::string &jstring)
{
    std::string string = encode(jstring, ENC_SJIS, ENC_UTF8, true);
    //Replace yen with backslash
    size_t pos = 0;
    while ((pos = string.find(YEN, pos)) != std::string::npos) {
        string.replace(pos, ARRAY_SIZE(YEN) - 1, "\\");
        ++pos;
    }
    return string;
}

std::string toLower(const std::string &string)
{
    return encode(string, ENC_UTF8, ENC_UTF8, true);
}
#endif
}
