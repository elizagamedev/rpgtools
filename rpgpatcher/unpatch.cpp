#include "unpatch.h"

#include <stdexcept>
#include <map>
#include <algorithm>
#include <cstdlib>

#include "util.h"

#include "common.h"
#include "lcf.h"

void unpatchEvent(ofstream &out, Lcf &lcf, const std::string &name)
{
    //Skip events with the name "debug"
    if (name.find("ﾃﾞﾊﾞｯｸﾞ") != std::string::npos)
        return;

    std::vector<EventText> messages = parseEventText(lcf);

    //write messages to file
    for (size_t i = 0; i < messages.size(); ++i) {
        //skip empty messages
        if (messages[i].text.empty())
            continue;
        //skip ASCII-only messages
        if (isAscii(messages[i].text))
            continue;
        out << "# " << name << " [" << messages[i].pos;
        if (messages[i].type == 0)
            out << ", Message]\n";
        else if (messages[i].type == 1)
            out << ", Choices]\n";
        else
            out << ", Hero]\n";
        for (int j = 0; j < 80; ++j)
            out << "=";
        out << "\n";
        out << messages[i].text;
        for (int j = 0; j < 80; ++j)
            out << "-";
        out << "\n";
        for (int lines = std::count(messages[i].text.begin(), messages[i].text.end(), '\n'); lines > 0; --lines)
            out << "\n";
        for (int j = 0; j < 80; ++j)
            out << "=";
        out << "\n\n";
    }
}

void unpatchLmu(ofstream &out, const std::string &filename, int mapid, const std::string &mapname)
{
    Lcf lcf(filename, "", LMU_HEADER);
    lcf.begin(0x51); //events
    int nEvents = lcf.readInt();
    for (int i = 0; i < nEvents; ++i) {
        int id = lcf.readInt(); //ID
        std::string name = lcf.peekString(0x1);
        lcf.begin(0x05);
        int nPages = lcf.readInt();
        for (int j = 0; j < nPages; ++j) {
            int page = lcf.readInt(); //ID
            lcf.begin(0x34, true);
            std::stringstream ss;
            ss << "Map [" << mapid << "]";
            if (!mapname.empty())
                ss << " " << mapname;
            ss << ": [" << id << ", " << page << "]";
            if (!name.empty())
                ss << " " << name;
            unpatchEvent(out, lcf, ss.str());
            lcf.end();
            lcf.advance();
        }
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

void unpatchLdb(ofstream &out, const std::string &filename)
{
    Lcf lcf(filename, "", LDB_HEADER);
    lcf.begin(CommonEvents);
    int nEvents = lcf.readInt();
    for (int i = 0; i < nEvents; ++i) {
        int id = lcf.readInt();
        std::string name = lcf.peekString(0x1);
        lcf.begin(0x16, true);
        std::stringstream ss;
        ss << "Common Event: [" << id << "]";
        if (!name.empty())
            ss << " " << name;
        unpatchEvent(out, lcf, ss.str());
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

std::map<int, std::string> getMapNames(const std::string &filename)
{
    Lcf lcf(filename, "", LMT_HEADER);
    std::map<int, std::string> maps;

    int nMaps = lcf.readInt();
    for (int i = 0; i < nMaps; ++i) {
        int id = lcf.readInt();
        maps[id] = lcf.peekString(0x1);
        lcf.advance();
    }
    return maps;
}

void unpatch(const std::string &gamePath, const std::string &outFilename)
{
    size_t i;

    ofstream out(outFilename.c_str());
    std::vector<std::string> files = Util::listFiles(gamePath);
    std::sort(files.begin(), files.end());

    //Read map of map names!
    std::map<int, std::string> maps;
    for (i = 0; i < files.size(); ++i) {
        if (Util::toLower(files[i]) == "rpg_rt.lmt") {
            maps = getMapNames(gamePath + files[i]);
            break;
        }
    }
    if (i == files.size())
        throw std::runtime_error("RPG_RT.lmt not found");

    //Do LDB first
    for (i = 0; i < files.size(); ++i) {
        if (Util::toLower(files[i]) == "rpg_rt.ldb") {
            unpatchLdb(out, gamePath + files[i]);
            break;
        }
    }
    if (i == files.size())
        throw std::runtime_error("RPG_RT.ldb not found");

    //Now do LMUs
    for (i = 0; i < files.size(); ++i) {
        if (Util::getExtension(files[i]) == "lmu") {
            //Determine ID
            std::string idstr = Util::toLower(Util::getWithoutExtension(files[i]));
            if (idstr.compare(0, 3, "map") != 0)
                continue;
            int id = std::atoi(idstr.c_str() + 3);
            unpatchLmu(out, gamePath + files[i], id, maps[id]);
        }
    }
}
