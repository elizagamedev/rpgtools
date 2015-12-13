#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <ctime>

#include "rgssa.h"
#include "os.h"
#include "util.h"

namespace Rgssa3
{
static inline Rgssa::Key generateKey()
{
    Rgssa::Key key = {static_cast<uint32_t>(
                      ((rand() & 0xFF) << 24) |
                      ((rand() & 0xFF) << 16) |
                      ((rand() & 0xFF) << 8) |
                      (rand() & 0xFF)
                      )};
    return key;
}

void writeSize(ofstream &file, Rgssa::Key key, size_t value)
{
    value ^= key.i;
    file.write(reinterpret_cast<char*>(&value), 4);
}

void writeString(ofstream &file, Rgssa::Key key, const std::string &string) {
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
        buffer[i] ^= key.c[i % 4];
    }

    //Write to file
    file.write(buffer.data(), buffer.size());
}

void pack(const std::string &filename, const std::string &srcpath, const std::vector<Rgssa::File> &srcfiles)
{
    //Initialize RNG for generating keys
    std::srand(std::time(NULL));

    //Find the offset to begin embedding files
    size_t embedOffset = sizeof(RGSSA_MAGIC_NUM) + 1 + 4 + (srcfiles.size() + 1) * (4 * 4);
    for (unsigned int i = 0; i < srcfiles.size(); ++i)
        embedOffset += srcfiles[i].name.size();

    //Vector of file keys
    std::vector<Rgssa::Key> fileKeys(srcfiles.size());

    try {
        char version = 3;

        //Write magic num + version
        ofstream file(filename.c_str());
        file.write(RGSSA_MAGIC_NUM, sizeof(RGSSA_MAGIC_NUM));
        file.write(&version, 1);

        //Write the key
        Rgssa::Key key = generateKey();
        file.write(reinterpret_cast<char*>(&key.i), 4);
        key.i *= 9;
        key.i += 3;

        //Write the file information
        for (unsigned int i = 0; i < srcfiles.size(); ++i) {
            fileKeys[i] = generateKey();
            writeSize(file, key, embedOffset);
            writeSize(file, key, srcfiles[i].size);
            writeSize(file, key, static_cast<size_t>(fileKeys[i].i));
            writeString(file, key, srcfiles[i].name);
            embedOffset += srcfiles[i].size;
        }

        //Write a 0 entry
        for (unsigned int i = 0; i < 4; ++i)
            writeSize(file, key, 0);

        //Write the file data
        for (unsigned int i = 0; i < srcfiles.size(); ++i) {
            Rgssa::embedFile(file, fileKeys[i], srcpath + srcfiles[i].name, srcfiles[i].size);
        }
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": i/o error: " + e.what());
    }
}

size_t readSize(ifstream &file, Rgssa::Key key)
{
    size_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 4);
    return value ^ key.i;
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
        buffer[i] ^= key.c[i % 4];
#ifdef OS_UNIX
        if (buffer[i] == '\\')
            buffer[i] = '/';
#endif
    }

    //Return std::string
    return std::string(buffer.begin(), buffer.end());
}

void unpack(ifstream &file, const std::string &outpath)
{
    //Read key
    Rgssa::Key key;
    file.read(reinterpret_cast<char*>(&key.i), 4);
    key.i *= 9;
    key.i += 3;

    for (;;) {
        //Read file info
        size_t offset = readSize(file, key);
        if (offset == 0)
            break; //end of file info
        size_t size = readSize(file, key);
        Rgssa::Key fileKey = {static_cast<uint32_t>(readSize(file, key))};
        std::string outname = readString(file, key);

        //Extract
        size_t pos = file.tellg();
        file.seekg(offset);
        Rgssa::extractFile(file, fileKey, outpath + outname, size);
        file.seekg(pos);
    }
}
}
