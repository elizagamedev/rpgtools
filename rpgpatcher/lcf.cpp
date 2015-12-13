#include "lcf.h"

#include <memory>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cassert>

#include "util.h"

/*********************
 *        LCF        *
 * *******************/

Lcf::Lcf(const std::string &inFilename, const std::string &outFilename, const char *header) :
    in(inFilename.c_str())
{
    if (!in.is_open())
        throw std::runtime_error(inFilename + ": could not read");
    if (!outFilename.empty()) {
        out.open(outFilename.c_str());
        if (!out.is_open())
            throw std::runtime_error(outFilename + ": could not write");
    }

    //Read the header, verify it
    char headerSize = std::strlen(header);
    char headerSizeTest;
    in.read(&headerSizeTest, 1);
    if (headerSize != headerSizeTest)
        throw std::runtime_error(inFilename + ": invalid file header");
    std::vector<char> headerTest(headerSize);
    in.read(headerTest.data(), headerSize);
    if (!std::equal(headerTest.begin(), headerTest.end(), header))
        throw std::runtime_error(inFilename + ": invalid file header");

    //Write the header to the output file
    if (out.is_open()) {
        out.write(&headerSize, 1);
        out.write(header, headerSize);
    }
}

Lcf::~Lcf()
{
    while (!blocks.empty())
        end();
    //Write remainder of file
    if (out.is_open()) {
        std::streampos pos = in.tellg();
        in.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(in.tellg() - pos);
        in.seekg(pos);
        std::vector<char> data(size);
        in.read(data.data(), size);
        out.write(data.data(), size);
    }
}

bool Lcf::begin(int block, bool eventhack)
{
    //Seek to the block ID, add a new block to the stack
    if (eventhack) {
        if (!seekBlock(block - 1, true))
            return false;
        in.seekg(readInt(), std::ios::cur);
        if (readInt() != block)
            return false;
        writeInt(block - 1);
    } else {
        if (!seekBlock(block, true))
            return false;
        writeInt(block);
    }
    size_t size = readInt();
    blocks.push_back(new LcfBlock(block, in.tellg() + static_cast<std::streamoff>(size), eventhack));
    return true;
}

void Lcf::end()
{
    assert(blocks.size() > 0);

    //Make sure we're at the end of the block
    seek(blocks.back()->end);

    //Pop top block from the stack
    std::auto_ptr<LcfBlock> block(blocks.back());
    blocks.pop_back();

    //Shitty event hack
    if (block->eventhack) {
        size_t size = block->stream.tellp();
        writeInt(getIntSize(size));
        writeInt(size);
        writeInt(block->block);
    }

    //Write data in block
    writeInt(block->stream.tellp());
    write(block->stream);
}

bool Lcf::bytesRemain()
{
    assert(blocks.size() > 0);

    //return true if there are any bytes left in the block
    return in.tellg() < blocks.back()->end;
}

void Lcf::advance()
{
    seekBlock(0, true);
    writeInt(0);
}

/* read/write funcs */
int Lcf::rwInt()
{
    int value = readInt();
    writeInt(value);
    return value;
}

/* peek funcs */
int Lcf::peekInt(int block)
{
    std::streampos pos = in.tellg();
    if (!seekBlock(block, false)) {
        in.seekg(pos);
        return 0;
    }
    readInt(); //size; utterly useless
    int value = readInt();
    value = readInt();
    in.seekg(pos);
    return value;
}

std::string Lcf::peekString(int block)
{
    //Seek to block ID, read string, move back to beginning of block and return
    std::streampos pos = in.tellg();
    if (!seekBlock(block, false)) {
        in.seekg(pos);
        return std::string();
    }
    size_t size = readInt();
    if (size == 0) {
        in.seekg(pos);
        return std::string();
    }
    std::vector<char> buffer(size);
    in.read(buffer.data(), buffer.size());
    in.seekg(pos);
    return std::string(buffer.begin(), buffer.end());
}

/* put funcs */
void Lcf::putInt(int block, int value, bool create)
{
    //Seek to block ID (and advance input buffer if the block exists)
    std::streampos pos = in.tellg();
    if (seekBlock(block, true)) {
        in.seekg(readInt(), std::ios::cur);
    } else {
        in.seekg(pos);
        if (!create)
            return;
    }
    writeInt(block);
    writeInt(getIntSize(value));
    writeInt(value);
}

void Lcf::putString(int block, const std::string &str, bool create)
{
    //Seek to block ID (and advance input buffer if the block exists)
    std::streampos pos = in.tellg();
    if (seekBlock(block, true)) {
        in.seekg(readInt(), std::ios::cur);
    } else {
        in.seekg(pos);
        if (!create)
            return;
    }
    writeInt(block);
    writeInt(str.size());
    if (!str.empty())
        write(str.data(), str.size());
}

/* raw funcs */
//read
int Lcf::readInt()
{
    uint32_t value = 0;
    char temp;
    do {
        value <<= 7;
        in.read(&temp, 1);
        value |= temp & 0x7F;
    } while (temp & 0x80);
    return static_cast<int>(value);
}

std::string Lcf::readString()
{
    size_t size = readInt();
    if (size == 0)
        return std::string();
    std::vector<char> buffer(size);
    in.read(buffer.data(), buffer.size());
    return std::string(buffer.begin(), buffer.end());
}

std::string Lcf::readIntString()
{
    size_t size = readInt();
    if (size == 0)
        return std::string();
    std::vector<char> buffer(size);
    for (size_t i = 0; i < size; ++i)
        buffer[i] = static_cast<char>(readInt());
    return std::string(buffer.begin(), buffer.end());
}

//write
void Lcf::writeInt(int value)
{
    if (!out.is_open())
        return;
    for (int i = 28; i >= 0; i -= 7) {
        if (static_cast<uint32_t>(value) >= (1U << i) || i == 0) {
            char temp = static_cast<char>(((value >> i) & 0x7F) | (i > 0 ? 0x80 : 0));
            write(&temp, 1);
        }
    }
}

void Lcf::writeString(const std::string &str)
{
    writeInt(str.size());
    if (!str.empty())
        write(str.data(), str.size());
}

/* seek/tell */
std::streampos Lcf::tell()
{
    return in.tellg();
}

void Lcf::seek(std::streampos pos)
{
    if (out.is_open()) {
        std::streampos now = in.tellg();
        if (now == pos)
            return;
        if (now < pos) {
            //Read all data until pos, then write it
            size_t size = static_cast<size_t>(pos - now);
            std::vector<char> data(size);
            in.read(data.data(), size);
            write(data.data(), size);
        } else {
            //Just go backwards; this should ONLY happen
            //after some raw read funcs were used or we're in
            //read-only mode
            in.seekg(pos);
        }
    } else {
        in.seekg(pos);
    }
}

/* PRIVATE FUNCS */
bool Lcf::seekBlock(int block, bool writeData)
{
    int curblock;
    while ((curblock = readInt()) != block) {
        //The block doesn't exist!
        if (block != 0 && curblock > block)
            return false;
        if (block != 0 && curblock == 0)
            return false;
        size_t size = readInt();
        //Skip/write data
        if (writeData && out.is_open()) {
            writeInt(curblock);
            writeInt(size);
            if (size > 0) {
                std::vector<char> data(size);
                in.read(data.data(), size);
                write(data.data(), size);
            }
        } else {
            in.seekg(size, std::ios::cur);
        }
    }
    return true;
}

void Lcf::write(const char *data, size_t size)
{
    if (!out.is_open())
        return;
    if (blocks.empty())
        out.write(data, size);
    else
        blocks.back()->stream.write(data, size);
}

void Lcf::write(std::stringstream &ss)
{
    if (!out.is_open())
        return;
    if (blocks.empty())
        out << ss.rdbuf();
    else
        blocks.back()->stream << ss.rdbuf();
}

int Lcf::getIntSize(int value)
{
    int size = 0;
    for (int i = 28; i >= 0; i -= 7) {
        if (static_cast<uint32_t>(value) >= (1U << i) || i == 0) {
            ++size;
        }
    }
    return size;
}
