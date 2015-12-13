#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <cstdio>

#include "os.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define UNUSED(x) (void)(x)

#ifdef _MSC_VER
#define PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#else
#define PACK(decl) decl __attribute__((__packed__))
#endif

namespace Util
{
FILE *fopen(const std::string &str, const unichar *args);
void mkdir(const std::string &dirname);
void mkdirsForFile(const std::string &filename);
bool dirExists(const std::string &dirname);
std::vector<std::string> listFiles(const std::string &path);
std::string getExtension(const std::string &filename);
std::string getWithoutExtension(const std::string &filename);
size_t getFileSize(const std::string &filename);
void deleteFile(const std::string &filename);
void deleteFolder(const std::string &filename);
std::string readFileContents(const std::string &filename);
std::string sanitizeDirPath(std::string path);
void copyFile(const std::string &src, const std::string &dst);
#ifdef OS_W32
std::string sanitizePath(std::string path);
#else
static inline std::string sanitizePath(const std::string &path)
{
    return path;
}
#endif

#if !defined UCONV && defined OS_UNIX
std::string toLower(std::string string);
#else
/* CODEPAGE CONVERSION STUFF */
std::string fromJis(const std::string &jstring);
std::string toJis(std::string string);
std::string fromJisToLower(const std::string &jstring);
std::string toLower(const std::string &string);
#endif
}

#endif // UTIL_H
