#ifndef RGSSA_H
#define RGSSA_H

#include <string>
#include <stdint.h>

#include "os.h"

#define RGSSA_MAGIC_NUM "RGSSAD"

namespace Rgssa
{
struct File
{
    File(const std::string &name, size_t size) :
        name(name),
        size(size)
    {
    }

    std::string name;
    size_t size;
};

union Key
{
    uint32_t i;
    char c[4];
};

void unpack(const std::string &filename, const std::string &outpath);

void listFilesRecursively(std::vector<File> &list, const std::string &realpath, const std::string &path);
void assertMagicNumber(ifstream &stream);
void extractFile(ifstream &file, Key key, const std::string &outname, size_t size);
void embedFile(ofstream &file, Key key, const std::string &srcname, size_t size);
}

#endif // RGSSA_H

