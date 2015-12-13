#include "rgssa.h"

#include <sstream>
#include <stdexcept>
#include <cstring>

#include "os.h"
#include "util.h"

//MUST BE A MULTIPLE OF 4
#define FILE_BUFFER_SIZE 1024

namespace Rgssa1
{
void unpack(ifstream &file, const std::string &outpath, size_t fileSize);
}

namespace Rgssa3
{
void unpack(ifstream &file, const std::string &outpath);
}

namespace Rgssa
{
void unpack(const std::string &filename, const std::string &outpath)
{
    try {
        ifstream file(filename.c_str());
        assertMagicNumber(file);
        char version;
        file.read(&version, 1);
        if (version == 1)
            Rgssa1::unpack(file, outpath, Util::getFileSize(filename));
        else if (version == 3)
            Rgssa3::unpack(file, outpath);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": i/o error: " + e.what());
    }
}

void listFilesRecursively(std::vector<File> &list, const std::string &realpath, const std::string &path)
{
    std::vector<std::string> files = Util::listFiles(realpath);

    for (unsigned int i = 0; i < files.size(); ++i) {
        if (Util::dirExists(realpath + files[i]))
            listFilesRecursively(list, realpath + files[i] + PATH_SEPARATOR, path + files[i] + PATH_SEPARATOR);
        else
            list.push_back(File(path + files[i], Util::getFileSize(realpath + files[i])));
    }
}

void assertMagicNumber(ifstream &file)
{
    char magicNum[sizeof(RGSSA_MAGIC_NUM)];
    file.read(magicNum, sizeof(magicNum));
    if (std::memcmp(magicNum, RGSSA_MAGIC_NUM, sizeof(magicNum)))
        throw std::runtime_error("does not appear to be a valid RGSS archive");
}

void extractFile(ifstream &file, Key key, const std::string &outname, size_t size) {
    Util::mkdirsForFile(outname);
    ofstream outfile(outname.c_str());

    char buffer[FILE_BUFFER_SIZE];
    for (size_t bytesDone = 0; bytesDone < size; bytesDone += FILE_BUFFER_SIZE) {
        //Read data into buffer, decrypt
        size_t bytesRead = (size - bytesDone < FILE_BUFFER_SIZE) ? size - bytesDone : FILE_BUFFER_SIZE;
        file.read(buffer, bytesRead);
        for (unsigned int i = 0; i < bytesRead; ++i) {
            buffer[i] ^= key.c[i % 4];
            if (i % 4 == 3) {
                key.i *= 7;
                key.i += 3;
            }
        }

        //Write data to out buffer
        outfile.write(buffer, bytesRead);
    }
}

void embedFile(ofstream &file, Key key, const std::string &srcname, size_t size) {
    ifstream infile(srcname.c_str());

    char buffer[FILE_BUFFER_SIZE];
    for (size_t bytesDone = 0; bytesDone < size; bytesDone += FILE_BUFFER_SIZE) {
        //Read data into buffer, decrypt
        size_t bytesRead = (size - bytesDone < FILE_BUFFER_SIZE) ? size - bytesDone : FILE_BUFFER_SIZE;
        infile.read(buffer, bytesRead);
        for (unsigned int i = 0; i < bytesRead; ++i) {
            buffer[i] ^= key.c[i % 4];
            if (i % 4 == 3) {
                key.i *= 7;
                key.i += 3;
            }
        }

        //Write data to out buffer
        file.write(buffer, bytesRead);
    }
}
}
