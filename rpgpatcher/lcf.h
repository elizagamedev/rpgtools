#ifndef LCF_H
#define LCF_H

#include <vector>
#include <sstream>
#include <stdint.h>

#include "os.h"

#define LDB_HEADER "LcfDataBase"
#define LMU_HEADER "LcfMapUnit"
#define LMT_HEADER "LcfMapTree"

enum LmuBlock {
    Hero = 0x0B,
    Skills = 0x0C,
    Items = 0x0D,
    Monsters = 0x0E,
    MonsterParty = 0x0F,
    Terrain = 0x10,
    Attributes = 0x11,
    Conditions = 0x12,
    BattleAnimation = 0x13,
    ChipSet = 0x14,
    Vocabulary = 0x15,
    System = 0x16,
    Switches = 0x17,
    Variables = 0x18,
    CommonEvents = 0x19,
};

struct LcfBlock
{
    LcfBlock(int block, std::streampos end, bool eventhack) :
        block(block),
        end(end),
        eventhack(eventhack)
    {}

    int block;
    std::streampos end;
    std::stringstream stream;
    bool eventhack;
};

class Lcf
{
public:
    Lcf(const std::string &inFilename, const std::string &outFilename, const char *header);
    ~Lcf();

    bool begin(int block, bool eventhack = false);
    void end();

    bool bytesRemain();
    void advance(); //advance to the separating 0 in the list

    //rw
    int rwInt();

    //peek
    int peekInt(int block);
    std::string peekString(int block);

    //put
    void putInt(int block, int value, bool create = true);
    void putString(int block, const std::string &str, bool create = true);

    /* "RAW" funcs */
    //read
    int readInt();
    std::string readString();
    std::string readIntString();

    //write
    void writeInt(int value);
    void writeString(const std::string &str);

    //tell/seek
    std::streampos tell();
    void seek(std::streampos pos);

    //misc
    int getIntSize(int value);

private:
    bool seekBlock(int block, bool writeData);

    //Write
    void write(const char *data, size_t size);
    void write(std::stringstream &ss);

    ifstream in;
    ofstream out;

    std::vector<LcfBlock*> blocks;
};

#endif // LCF_H

