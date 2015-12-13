#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <stdint.h>

#include "os.h"
#include "util.h"

//Extract defines
#define FILE_BUFFER_SIZE 1024

//Wolf-related defines
#define ATTRIBUTE_DIRECTORY	0x10
#define ATTRIBUTE_FILE		0x20
#define MIN_COMPRESS		4
#define NOT_COMPRESSED		0xffffffff
#define NO_PARENT			0xffffffff

#define WOLF_KEY_SIZE		12

static const char WOLF_MAGIC_NUM[] = {
    'D', 'X', 3, 0,
};

#ifdef OS_UNIX
#include <time.h>
#include <utime.h>
#include <sys/time.h>
#define EPOCH_DIFF 11644473600LL

namespace Unix
{
#ifdef OS_LINUX
void fromW32Time(time_t &dst, uint64_t src) {
    dst = (time_t)(src / 10000000 - EPOCH_DIFF);
}
#else
void fromW32Time(struct timeval &dst, uint64_t src) {
    dst.tv_sec = (long)(src / 10000000 - EPOCH_DIFF);
    dst.tv_usec = (long)((src % 10000000 - EPOCH_DIFF) / 10);
}
#endif
}
#endif

namespace Wolf
{
static void setTimestamp(const std::string &file, uint64_t created, uint64_t modified, uint64_t accessed)
{
#if defined OS_W32
    HANDLE hFile = CreateFileW(W32::toWide(file).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return;
    SetFileTime(hFile,
                reinterpret_cast<FILETIME*>(&created),
                reinterpret_cast<FILETIME*>(&accessed),
                reinterpret_cast<FILETIME*>(&modified));
    CloseHandle(hFile);
#elif defined OS_LINUX
    UNUSED(created);
    struct utimbuf ut;
    Unix::fromW32Time(ut.actime, accessed);
    Unix::fromW32Time(ut.modtime, modified);
    utime(file.c_str(), &ut);
#else
    struct timeval times[2];
    //Access time
    Unix::fromW32Time(times[0], accessed);
    //Birth time
    Unix::fromW32Time(times[1], created);
    utimes(file.c_str(), times);
    //Modify time
    Unix::fromW32Time(times[1], modified);
    utimes(file.c_str(), times);
#endif
}

size_t decompress(uint8_t *dst, const uint8_t *src)
{
    //Get destination and source buffer sizes
    size_t dstSize = *((uint32_t *)&src[0]);
    size_t srcSize = *((uint32_t *)&src[4]) - 9;

    //Control keycode
    int keycode = src[8];

    //Begin decompression
    src += 9;
    while (srcSize) {
        if (src[0] != keycode) {
            *dst = *src;
            dst++;
            src++;
            srcSize--;
            continue;
        }

        if (src[1] == keycode) {
            *dst = (uint8_t)keycode;
            dst++;
            src += 2;
            srcSize -= 2;
            continue;
        }

        int code = src[1];
        if (code > keycode)
        code--;

        src += 2;
        srcSize -= 2;

        int combo = code >> 3;
        if (code & (1<<2)) {
            combo |= *src << 5;
            src++;
            srcSize--;
        }
        combo += MIN_COMPRESS;

        int index;
        switch (code & 0x3) {
                case 0:
                index = *src;
                src++;
                srcSize--;
                break;
                case 1:
                index = *((uint16_t *)src);
                src += 2;
                srcSize -= 2;
                break;
                case 2:
                index = *((uint16_t *)src) | (src[2] << 16);
                src += 3;
                srcSize -= 3;
                break;
            default:
                return 0;
        }
        index++;

        //Decompress
        if (index < combo) {
            int num = index;
            while (combo > num) {
                std::memcpy(dst, dst - num, num);
                dst += num;
                combo -= num;
                num += num;
            }
            if (combo != 0) {
                std::memcpy(dst, dst - num, combo);
                dst += combo;
            }
        } else {
            std::memcpy(dst, dst - index, combo);
            dst += combo;
        }
    }

    return dstSize;
}

PACK(struct Filename
{
    uint16_t sizeDivBy4;
    uint16_t checksum;
});

PACK(struct File
{
    uint32_t offName;
    uint32_t attributes;
    uint64_t timeCreated;
    uint64_t timeAccessed;
    uint64_t timeModified;
    uint32_t offData;
    uint32_t size;
    uint32_t sizePress;
});

PACK(struct Directory
{
    uint32_t offFile;
    uint32_t offParentDir;
    uint32_t nChildren;
    uint32_t offFirstChild;
});

class Archive
{
public:
    Archive(const std::string &filename);

    //Helper funcs
    void read(void *dst, size_t size);
    size_t readSize()
    {
        size_t value = 0;
        read(&value, 4);
        return value;
    }
    std::string getFilename(unsigned int index);
    std::string getFilePath(unsigned int index);

    void extractFile(const std::string &dataPath, unsigned int index);
    void unpack(const std::string &outpath);

private:
    size_t fileSize;
    ifstream file;
    char key[WOLF_KEY_SIZE];

    std::vector<char> filenames;
    std::vector<File> files;
    std::vector<Directory> directories;
};

void Archive::read(void *dst, size_t size)
{
    int offset = file.tellg() % WOLF_KEY_SIZE;
    file.read(reinterpret_cast<char*>(dst), size);
    for (unsigned int i = 0; i < size; ++i) {
        reinterpret_cast<char*>(dst)[i] ^= key[offset];
        offset = (offset + 1) % WOLF_KEY_SIZE;
    }
}

std::string Archive::getFilename(unsigned int index)
{
    size_t size = reinterpret_cast<const Filename*>(filenames.data() + files[index].offName)->sizeDivBy4 * 4;
    const char *filename = filenames.data() + files[index].offName + 4 + size;
    return Util::fromJis(filename);
}

std::string Archive::getFilePath(unsigned int index)
{
    //Get this file's name
    std::string name = getFilename(index);

    //Find our parent directory
    unsigned int i = 0;
    for (; i < directories.size(); ++i) {
        unsigned int iFirstChild = directories[i].offFirstChild / sizeof(File);
        if (iFirstChild <= index && iFirstChild + directories[i].nChildren > index) {
            //This is our parent. Traverse the tree backwards till we hit root.
            while (directories[i].offParentDir != NO_PARENT) {
                name.insert(0, getFilename(directories[i].offFile / sizeof(File)) + PATH_SEPARATOR);
                i = directories[i].offParentDir / sizeof(Directory);
            }
            break;
        }
    }
    if (i == directories.size())
        return "";
    return name;
}

Archive::Archive(const std::string &filename)
{
    //Get file size, open file stream
    fileSize = Util::getFileSize(filename);
    file.open(filename.c_str());

    //The first 12 bytes will help us decrypt the file
    file.read(key, sizeof(key));

    //Let's try decrypting this thing
    //xor this with the magic number to get the first 4 bytes
    for (unsigned int i = 0; i < ARRAY_SIZE(WOLF_MAGIC_NUM); i++)
        key[i] ^= WOLF_MAGIC_NUM[i];
    //The last four bytes are 0x00000018
    key[8] ^= 0x18;

    //We can now decrypt the filenames offset...
    size_t offFilenames = readSize();

    //The size of the file minus the size of the filenames offset
    //is the same as the size of the file info.
    size_t sizeFileInfo = fileSize - offFilenames;
    key[4] ^= static_cast<char>((sizeFileInfo >> 0) & 0xFF);
    key[5] ^= static_cast<char>((sizeFileInfo >> 8) & 0xFF);
    key[6] ^= static_cast<char>((sizeFileInfo >> 16) & 0xFF);
    key[7] ^= static_cast<char>((sizeFileInfo >> 24) & 0xFF);

    //And we're done! Read the remainder of the header.
    size_t offFiles = readSize();
    size_t offDirectories = readSize();

    //Prepare buffers for file info
    filenames = std::vector<char>(offFiles);
    files = std::vector<File>((offDirectories - offFiles) / sizeof(File));
    directories = std::vector<Directory>((sizeFileInfo - offDirectories) / sizeof(Directory));

    //Read the file info into memory
    file.seekg(offFilenames);
    read(filenames.data(), filenames.size());
    file.seekg(offFilenames + offFiles);
    read(files.data(), files.size() * sizeof(File));
    file.seekg(offFilenames + offDirectories);
    read(directories.data(), directories.size() * sizeof(Directory));
}

void Archive::extractFile(const std::string &dataPath, unsigned int index)
{
    const File &wolfFile = files[index];
    std::ofstream outfile((dataPath + getFilePath(index)).c_str());

    //Write the file to the disk
    size_t size = wolfFile.size;
    file.seekg(0x18 + wolfFile.offData);

    if (wolfFile.sizePress == NOT_COMPRESSED) {
        char buffer[FILE_BUFFER_SIZE];
        for (size_t bytesDone = 0; bytesDone < size; bytesDone += FILE_BUFFER_SIZE) {
            //Read data into buffer
            size_t bytesRead = (size - bytesDone < FILE_BUFFER_SIZE) ? size - bytesDone : FILE_BUFFER_SIZE;
            read(buffer, bytesRead);
            //Write data to out buffer
            outfile.write(buffer, bytesRead);
        }
    } else {
        //Read and decompress the data
        std::vector<uint8_t> dataSrc(wolfFile.sizePress);
        std::vector<uint8_t> dataDst(size);
        read(dataSrc.data(), dataSrc.size());
        size = decompress(dataDst.data(), dataSrc.data());

        //Write to the file
        outfile.write(reinterpret_cast<char*>(dataDst.data()), size);
    }
}

void Archive::unpack(const std::string &outpath)
{
    std::string dataPath = outpath + "Data" PATH_SEPARATOR;

    //Extract everything
    Util::mkdir(dataPath);
    for (unsigned int i = 1; i < files.size(); ++i) {
        if (files[i].attributes & ATTRIBUTE_DIRECTORY) {
            //Create directory
            Util::mkdir(dataPath + getFilePath(i));
        } else {
            //Extract the file
            extractFile(dataPath, i);
        }
    }

    //Update timestamps
    for (unsigned int i = 1; i < files.size(); ++i) {
        const File &file = files[i];
        setTimestamp(dataPath + getFilePath(i), file.timeCreated, file.timeModified, file.timeAccessed);
    }
}

void unpack(const std::string &filename, const std::string &outpath)
{
    try {
        Archive archive(filename);
        archive.unpack(outpath);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": i/o error: " + e.what());
    }
}
}
