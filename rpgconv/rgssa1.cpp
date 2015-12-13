#include <string>
#include <vector>
#include <stdexcept>

#include "os.h"
#include "rgssa.h"
#include "util.h"

#define RGSSA1_KEY 0xDEADCAFE

namespace Rgssa1
{

void writeSize(ofstream &file, Rgssa::Key &key, size_t value)
{
    value ^= key.i;
    file.write(reinterpret_cast<char*>(&value), 4);
    key.i *= 7;
    key.i += 3;
}

void writeString(ofstream &file, Rgssa::Key &key, const std::string &string)
{
    //Write size of string
    writeSize(file, key, string.size());

    //Copy string into buffer
    std::vector<char> buffer(string.begin(), string.end());

    //Apply encryption transformation to buffer
    for (unsigned int i = 0; i < buffer.size(); ++i) {
#ifdef OS_UNIX
        if (buffer[i] == '/')
            buffer[i] = '\\';
#endif
        buffer[i] ^= key.c[0];
        key.i *= 7;
        key.i += 3;
    }

    //Write to file
    file.write(buffer.data(), buffer.size());
}

void pack(const std::string &filename, const std::string &srcpath, const std::vector<Rgssa::File> &srcfiles)
{
    try {
        char version = 1;

        //Write magic num + version
        ofstream file(filename.c_str());
        file.write(RGSSA_MAGIC_NUM, sizeof(RGSSA_MAGIC_NUM));
        file.write(&version, 1);

        //Pack the files
        Rgssa::Key key = {RGSSA1_KEY};
        for (unsigned int i = 0; i < srcfiles.size(); ++i) {
            writeString(file, key, srcfiles[i].name);
            writeSize(file, key, srcfiles[i].size);
            Rgssa::embedFile(file, key, srcpath + srcfiles[i].name, srcfiles[i].size);
        }
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": i/o error: " + e.what());
    }
}

size_t readSize(ifstream &file, Rgssa::Key &key)
{
    size_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 4);
    value ^= key.i;
    key.i *= 7;
    key.i += 3;
    return value;
}

std::string readString(ifstream &file, Rgssa::Key &key)
{
    //Get size of string
    size_t size = readSize(file, key);
    if (size == 0)
        throw std::runtime_error("read a string of size 0");

    //Read string into buffer
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    //Apply decryption transformation to buffer
    for (unsigned int i = 0; i < size; ++i) {
        buffer[i] ^= key.c[0];
#ifdef OS_UNIX
        if (buffer[i] == '\\')
            buffer[i] = '/';
#endif
        key.i *= 7;
        key.i += 3;
    }

    //Return std::string
    return std::string(buffer.begin(), buffer.end());
}

void unpack(ifstream &file, const std::string &outpath, size_t fileSize)
{
    Rgssa::Key key = {RGSSA1_KEY};
    for (;;) {
        std::string outname = readString(file, key);
        size_t size = readSize(file, key);
        Rgssa::extractFile(file, key, outpath + outname, size);

        //Stop reading at end of file
        if (static_cast<size_t>(file.tellg()) == fileSize)
            break;
    }
}
}
