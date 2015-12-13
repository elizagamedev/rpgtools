#include "patch.h"

#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

#include "os.h"
#include "util.h"
#include "bitmap.h"

#include "common.h"
#include "tr.h"
#include "lcf.h"

const char *const Asset::names[] = {
    "Battle",
    "CharSet",
    "ChipSet",
    "FaceSet",
    "GameOver",
    "Music",
    "Panorama",
    "Picture",
    "Sound",
    "System",
    "Title",
};

const char *const Asset::lowerNames[] = {
    "battle",
    "charset",
    "chipset",
    "faceset",
    "gameover",
    "music",
    "panorama",
    "picture",
    "sound",
    "system",
    "title",
};

Patch::Patch(const std::string &patchPath, const std::string &gamePath, const std::string &rtpPath) :
    patchPath(patchPath),
    gamePath(gamePath)
{
    //Read string translations
    std::cout << "Reading text patches..." << std::endl;
    std::string stringsPath = patchPath + "STRINGS" PATH_SEPARATOR;
    heroes = NameTr::createMap(stringsPath + "hero.txt");
    skills = NameTr::createMap(stringsPath + "skill.txt");
    items = NameTr::createMap(stringsPath + "item.txt");
    vocab = VocabTr(stringsPath + "vocabulary.txt");
    events = EventTr::createMap(stringsPath + "events" PATH_SEPARATOR);

    //Zero out asset counts
    std::memset(nAssets, 0, sizeof(nAssets));

    //Get full list of all asset files
    std::cout << "Creating asset map..." << std::endl;
    const char *const assetFolderPaths[3] = {
        rtpPath.c_str(),
        gamePath.c_str(),
        patchPath.c_str(),
    };
    //For every base folder...
    for (size_t i = 0; i < ARRAY_SIZE(assetFolderPaths); ++i) {
#ifdef OS_W32
        for (size_t k = 0; k < ARRAY_SIZE(Asset::names); ++k) {
            std::string folderPath = assetFolderPaths[i] + folders[j] + PATH_SEPARATOR;
            if (!Util::dirExists(folderPath))
                continue;
            std::vector<std::string> files = Util::listFiles(folderPath);
            for (size_t m = 0; m < files.size(); ++m) {
                //Make sure this has the right extension
                std::string ext = Util::getExtension(files[m]);
                if (k == Asset::Music || k == Asset::Sound) {
                    if (ext != "wav" && ext != "mp3" && ext != "mid" && ext != "midi")
                        continue;
                } else {
                    if (ext != "xyz" && ext != "png" && ext != "bmp")
                        continue;
                }
                //Add to map
                assets[k][Util::toLower(Util::getWithoutExtension(files[m]))] = Asset(folderPath + files[m]);
            }
        }
#else
        //For every asset folder...
        std::vector<std::string> folders = Util::listFiles(assetFolderPaths[i]);
        for (size_t j = 0; j < folders.size(); ++j) {
            for (size_t k = 0; k < ARRAY_SIZE(Asset::lowerNames); ++k) {
                if (Util::toLower(folders[j]) != Asset::lowerNames[k])
                    continue;
                //Get ready to add the files in this folder into the asset map
                std::string folderPath = assetFolderPaths[i] + folders[j] + PATH_SEPARATOR;
                std::vector<std::string> files = Util::listFiles(folderPath);
                for (size_t m = 0; m < files.size(); ++m) {
                    //Make sure this has the right extension
                    std::string ext = Util::getExtension(files[m]);
                    if (k == Asset::Music || k == Asset::Sound) {
                        if (ext != "wav" && ext != "mp3" && ext != "mid" && ext != "midi")
                            continue;
                    } else {
                        if (ext != "xyz" && ext != "png" && ext != "bmp")
                            continue;
                    }
                    //Add to map
                    assets[k][Util::toLower(Util::getWithoutExtension(files[m]))] = Asset(folderPath + files[m]);
                }
                break;
            }
        }
#endif
    }

    //get file paths
    gameFileNames = Util::listFiles(gamePath);
    std::sort(gameFileNames.begin(), gameFileNames.end());

#ifdef OS_W32
    ldbPath = gamePath + "RPG_RT.ldb";
    lmtPath = gamePath + "RPG_RT.lmt";
#else
    for (size_t i = 0; i < gameFileNames.size(); ++i) {
        if (Util::toLower(gameFileNames[i]) == "rpg_rt.ldb")
            ldbPath = gamePath + gameFileNames[i];
        else if (Util::toLower(gameFileNames[i]) == "rpg_rt.lmt")
            lmtPath = gamePath + gameFileNames[i];
    }
    if (ldbPath.empty() || lmtPath.empty())
        throw std::runtime_error("RPG_RT.ldb and/or RPG_RT.lmt not found");
#endif
}

void Patch::apply(const std::string &outPath)
{
    //First, delete the output directory so we can copy shit over.
    Util::deleteFolder(outPath);
    Util::mkdir(outPath);

    //Apply patch
    std::cout << "Applying text patches..." << std::endl;
    applyToLmt(lmtPath, outPath + "RPG_RT.lmt");
    applyToLdb(ldbPath, outPath + "RPG_RT.ldb");
    for (size_t i = 0; i < gameFileNames.size(); ++i) {
        if (Util::getExtension(gameFileNames[i]) == "lmu") {
            std::string idstr = Util::toLower(Util::getWithoutExtension(gameFileNames[i]));
            if (idstr.compare(0, 3, "map") != 0)
                continue;
            int id = std::atoi(idstr.c_str() + 3);
            std::ostringstream outName;
            outName << outPath << "Map" << std::setw(4) << std::setfill('0') << id << ".lmu";
            applyToLmu(gamePath + gameFileNames[i], outName.str(), id == 8);
        }
    }

    //Now we need to copy all the game assets to the new directory.
    std::cout << "Copying assets..." << std::endl;
    for (size_t i = 0; i < Asset::MAX_NoHack; ++i) {
        if (assets[i].empty())
            continue;

        //Make destination asset folder
        std::string outFolderPath = outPath + Asset::names[i] + PATH_SEPARATOR;
        Util::mkdir(outFolderPath);

        for (AssetMap::iterator it = assets[i].begin(); it != assets[i].end(); ++it) {
            //Skip empty entries
            if (it->second.name.empty())
                continue;

            if (it->second.path.empty()) {
                std::cout << "FUCK! " << Asset::names[i] << "\\" << it->first << " " << it->second.path << std::endl;
            }

            std::string dst = outFolderPath + it->second.name;
            if (i == Asset::Music || i == Asset::Sound) {
                Util::copyFile(it->second.path, dst + "." + Util::getExtension(it->second.path));
            } else {
                Bitmap bitmap(it->second.path);
                bitmap.writeToXyz(dst + ".xyz");
            }
        }
    }

    //Copy RPG_RT.exe and RPG_RT.ini
    //im not even gonna bother with differing case because im sick of this shit
    Util::copyFile(patchPath + "RPG_RT.exe", outPath + "RPG_RT.exe");
    Util::copyFile(patchPath + "RPG_RT.ini", outPath + "RPG_RT.ini");
    Util::copyFile(gamePath + "pc_back.png", outPath + "pc_back.png");
}

#if 0
void Patch::getLmtAssets(const std::string &filename)
{
    Lcf lcf(filename, "", LMT_HEADER);
    int nMaps = lcf.readInt();
    for (int i = 0; i < nMaps; ++i) {
        lcf.readInt(); //ID
        lcf.begin(0x0C);
        addAsset(Asset::Music, lcf.peekString(0x01));
        lcf.end();
        lcf.advance();
    }
}

void Patch::getLdbAssets(const std::string &filename)
{
    Lcf lcf(filename, "", LDB_HEADER);

    lcf.begin(Hero);
    int nHeroes = lcf.readInt();
    for (int i = 0; i < nHeroes; ++i) {
        lcf.readInt(); //ID; we dont need this
        addAsset(Asset::CharSet, lcf.peekString(0x03));
        addAsset(Asset::FaceSet, lcf.peekString(0x0F));
        lcf.advance();
    }
    lcf.end();

    lcf.begin(BattleAnimation);
    int nAnims = lcf.readInt();
    for (int i = 0; i < nAnims; ++i) {
        lcf.readInt(); //ID; we dont need this

        //battle anim image
        addAsset(Asset::Battle, lcf.peekString(0x02));

        //sound effects
        if (lcf.begin(0x06)) {
            int nSounds = lcf.readInt();
            for (int i = 0; i < nSounds; ++i) {
                lcf.readInt(); //ID
                if (lcf.begin(0x02)) {
                    addAsset(Asset::Sound, lcf.peekString(0x01));
                    lcf.end();
                }
                lcf.advance();
            }
            lcf.end();
        }
        lcf.advance();
    }
    lcf.end();

    lcf.begin(ChipSet);
    int nChipsets = lcf.readInt();
    for (int i = 0; i < nChipsets; ++i) {
        lcf.readInt();
        addAsset(Asset::ChipSet, lcf.peekString(0x02));
        lcf.advance();
    }
    lcf.end();

    lcf.begin(System);
    //vehicles
    addAsset(Asset::CharSet, lcf.peekString(0x0B));
    addAsset(Asset::CharSet, lcf.peekString(0x0C));
    addAsset(Asset::CharSet, lcf.peekString(0x0D));
    //title/game over
    addAsset(Asset::Title, lcf.peekString(0x11));
    addAsset(Asset::GameOver, lcf.peekString(0x12));
    //system graphics
    addAsset(Asset::System, lcf.peekString(0x13));
    //music and sound effects
    for (int i = 0x1F; i <= 0x34; ++i) {
        //vgperson says 0x27 and 0x28 aren't sounds
        if (i == 0x27)
            i = 0x29;

        if (lcf.begin(i)) {
            std::string name = lcf.peekString(0x01);
            if (i < 0x27) {
                addAsset(Asset::Music, name);
            } else {
                addAsset(Asset::Sound, name);
            }
            lcf.end();
        }
    }
    lcf.end();

    lcf.begin(CommonEvents);
    int nEvents = lcf.readInt();
    for (int i = 0; i < nEvents; ++i) {
        lcf.readInt();
        lcf.begin(0x16, true);
        getEventAssets(lcf);
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

void Patch::getLmuAssets(const std::string &filename)
{
    Lcf lcf(filename, "", LMU_HEADER);

    addAsset(Asset::Panorama, lcf.peekString(0x20));

    lcf.begin(0x51); //events
    int nEvents = lcf.readInt();
    for (int i = 0; i < nEvents; ++i) {
        lcf.readInt();
        lcf.begin(0x05); //page
        int nPages = lcf.readInt();
        for (int j = 0; j < nPages; ++j) {
            lcf.readInt();
            addAsset(Asset::CharSet, lcf.peekString(0x15));
            lcf.begin(0x34, true);
            getEventAssets(lcf);
            lcf.end();
            lcf.advance();
        }
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

void Patch::getEventAssets(Lcf &lcf)
{
    //This isn't gonna be fun
    while (lcf.bytesRemain()) {
        //Get event info
        int code = lcf.readInt();
        if (code == 0)
            continue;
        //discard indent
        lcf.readInt();
        std::string string = lcf.readString();

        //Parse event!
        switch (code) {
        case 11110:
            addAsset(Asset::Picture, string);
            break;
        case 11510:
        case 10660:
            addAsset(Asset::Music, string);
            break;
        case 11550:
        case 10670:
            addAsset(Asset::Sound, string);
            break;
        case 11720:
            addAsset(Asset::Panorama, string);
            break;
        case 10630:
            addAsset(Asset::CharSet, string);
            break;
        case 10640:
            addAsset(Asset::CharSet, string);
            break;
        case 10680:
            addAsset(Asset::System, string);
            break;
        }

        int argc = lcf.readInt();
        if (code == 11330) {
            //Move event

            //discard first 4 args
            for (int i = 0; i < 4; ++i)
                lcf.readInt();
            argc -= 4;

            while (argc > 0) {
                --argc;
                switch (lcf.readInt()) {
                case 32: //switch on
                case 33: //switch off
                    lcf.readInt();
                    --argc; //discard
                    break;
                case 34: //change charset
                    string = lcf.readIntString();
                    addAsset(Asset::CharSet, string);
                    lcf.readInt();
                    argc -= 1 + string.size() + 1;
                    break;
                case 35: //play sound
                    string = lcf.readIntString();
                    addAsset(Asset::Sound, string);
                    lcf.readInt();
                    lcf.readInt();
                    lcf.readInt();
                    argc -= 1 + string.size() + 3;
                    break;
                }
            }
        } else {
            //discard args
            for (int i = 0; i < argc; ++i)
                lcf.readInt();
        }
    }
}
#endif

void Patch::applyToLmt(const std::string &inFilename, const std::string &outFilename)
{
    Lcf lcf(inFilename, outFilename, LMT_HEADER);
    int nMaps = lcf.rwInt();
    for (int i = 0; i < nMaps; ++i) {
        lcf.rwInt(); //ID
        lcf.begin(0x0C);
        putAsset(lcf, Asset::Music, 0x01);
        lcf.end();
        lcf.advance();
    }
}

void Patch::applyToLdb(const std::string &inFilename, const std::string &outFilename)
{
    Lcf lcf(inFilename, outFilename, LDB_HEADER);

    lcf.begin(Hero);
    int nHeroes = lcf.rwInt();
    for (int i = 0; i < nHeroes; ++i) {
        lcf.rwInt(); //ID; we dont need this
        std::string name = lcf.peekString(0x1);
        const NameTr *tr = mapfind<NameTr>(heroes, Util::fromJis(name));
        if (tr == NULL) {
            lcf.putString(0x1, "", false);
            lcf.putString(0x2, "", false);
        } else {
            lcf.putString(0x1, tr->name);
            lcf.putString(0x2, tr->desc);
        }
        putAsset(lcf, Asset::CharSet, 0x03);
        putAsset(lcf, Asset::FaceSet, 0x0F);
        lcf.advance();
    }
    lcf.end();

    lcf.begin(Skills);
    int nSkills = lcf.rwInt();
    for (int i = 0; i < nSkills; ++i) {
        lcf.rwInt(); //ID; we dont need this
        std::string name = lcf.peekString(0x1);
        const NameTr *tr = mapfind<NameTr>(skills, Util::fromJis(name));
        if (tr == NULL) {
            lcf.putString(0x1, "", false);
            lcf.putString(0x2, "", false);
        } else {
            lcf.putString(0x1, tr->name);
            lcf.putString(0x2, tr->desc);
        }
        lcf.advance();
    }
    lcf.end();

    lcf.begin(Items);
    int nItems = lcf.rwInt();
    for (int i = 0; i < nItems; ++i) {
        lcf.rwInt(); //ID; we dont need this
        std::string name = lcf.peekString(0x1);
        const NameTr *tr = mapfind<NameTr>(items, Util::fromJis(name));
        if (tr == NULL) {
            lcf.putString(0x1, "", false);
            lcf.putString(0x2, "", false);
        } else {
            lcf.putString(0x1, tr->name);
            lcf.putString(0x2, tr->desc);
        }
        lcf.advance();
    }
    lcf.end();

    lcf.begin(Terrain);
    int nTerrain = lcf.rwInt();
    for (int i = 0; i < nTerrain; ++i) {
        lcf.rwInt();
        lcf.putString(0x1, "", false);
        lcf.advance();
    }
    lcf.end();

    lcf.begin(BattleAnimation);
    int nAnims = lcf.rwInt();
    for (int i = 0; i < nAnims; ++i) {
        lcf.rwInt();
        lcf.putString(0x1, "", false);

        //battle anim image
        putAsset(lcf, Asset::Battle, 0x02);

        //sound effects
        if (lcf.begin(0x06)) {
            int nSounds = lcf.rwInt();
            for (int i = 0; i < nSounds; ++i) {
                lcf.rwInt(); //ID
                if (lcf.begin(0x02)) {
                    putAsset(lcf, Asset::Sound, 0x01);
                    lcf.end();
                }
                lcf.advance();
            }
            lcf.end();
        }
        lcf.advance();
    }
    lcf.end();

    lcf.begin(ChipSet);
    int nChipsets = lcf.rwInt();
    for (int i = 0; i < nChipsets; ++i) {
        lcf.rwInt();
        lcf.putString(0x01, "", false);
        putAsset(lcf, Asset::ChipSet, 0x02);
        lcf.advance();
    }
    lcf.end();

    lcf.begin(Vocabulary);
    lcf.putString(0x5F, vocab.currency);
    lcf.putString(0x6A, vocab.command_items);
    lcf.putString(0x6B, vocab.command_skills);
    lcf.putString(0x6C, vocab.command_equip);
    lcf.putString(0x70, vocab.command_end);
    lcf.putString(0x72, vocab.title_new);
    lcf.putString(0x73, vocab.title_load);
    lcf.putString(0x75, vocab.title_quit);
    lcf.putString(0x80 + 0x00, vocab.level_abbr);
    //lcf.putString(0x80 + 0x01, vocab.hp_abbr);
    lcf.putString(0x80 + 0x08, vocab.arms);
    lcf.putString(0x80 + 0x12, vocab.choose_save);
    lcf.putString(0x80 + 0x13, vocab.choose_load);
    lcf.putString(0x80 + 0x14, vocab.choose_file);
    lcf.putString(0x80 + 0x17, vocab.quit_confirm);
    lcf.putString(0x80 + 0x18, vocab.yes);
    lcf.putString(0x80 + 0x19, vocab.no);
    lcf.end();

    lcf.begin(System);
    //vehicles
    putAsset(lcf, Asset::CharSet, 0x0B);
    putAsset(lcf, Asset::CharSet, 0x0C);
    putAsset(lcf, Asset::CharSet, 0x0D);
    //title/game over
    putAsset(lcf, Asset::Title, 0x11);
    putAsset(lcf, Asset::GameOver, 0x12);
    //system graphics
    putAsset(lcf, Asset::System, 0x13);
    //music and sound effects
    for (int i = 0x1F; i <= 0x34; ++i) {
        //vgperson says 0x27 and 0x28 aren't sounds
        if (i == 0x27)
            i = 0x29;

        if (lcf.begin(i)) {
            if (i < 0x27) {
                putAsset(lcf, Asset::Music, 0x01);
            } else {
                putAsset(lcf, Asset::Sound, 0x01);
            }
            lcf.end();
        }
    }
    lcf.end();

    lcf.begin(Switches);
    int nSwitches = lcf.rwInt();
    for (int i = 0; i < nSwitches; ++i) {
        lcf.rwInt();
        lcf.putString(0x01, "", false);
        lcf.advance();
    }
    lcf.end();

    lcf.begin(Variables);
    int nVariables = lcf.rwInt();
    for (int i = 0; i < nVariables; ++i) {
        lcf.rwInt();
        lcf.putString(0x01, "", false);
        lcf.advance();
    }
    lcf.end();

    lcf.begin(CommonEvents);
    int nEvents = lcf.rwInt();
    for (int i = 0; i < nEvents; ++i) {
        lcf.rwInt();
        lcf.putString(0x01, "", false);
        lcf.begin(0x16, true);
        applyToEvent(lcf, false);
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

void Patch::applyToLmu(const std::string &inFilename, const std::string &outFilename, bool rmintro)
{
    Lcf lcf(inFilename, outFilename, LMU_HEADER);

    putAsset(lcf, Asset::Panorama, 0x20);

    lcf.begin(0x51); //events
    int nEvents = lcf.rwInt();
    for (int i = 0; i < nEvents; ++i) {
        int id = lcf.rwInt();
        lcf.putString(0x01, "", false);
        lcf.begin(0x05); //page
        int nPages = lcf.rwInt();
        for (int j = 0; j < nPages; ++j) {
            lcf.rwInt(); //ID; we dont need this
            putAsset(lcf, Asset::CharSet, 0x15);
            lcf.begin(0x34, true);
            applyToEvent(lcf, rmintro && id == 1);
            lcf.end();
            lcf.advance();
        }
        lcf.end();
        lcf.advance();
    }
    lcf.end();
}

void Patch::applyToEvent(Lcf &lcf, bool rmintro)
{
    //Get messages
    std::streampos pos = lcf.tell();
    std::vector<EventText> messages = parseEventText(lcf);
    lcf.seek(pos);

    //Translate messages
    for (size_t i = 0; i < messages.size(); ++i) {
        std::string text = Util::fromJis(messages[i].text);
        const EventTr *tr = mapfind<EventTr>(events, text);
        if (tr != NULL && !tr->tr.empty()) {
            messages[i].text = tr->tr;
        } else if (!isAscii(text)) {
            if (messages[i].type == 1) {
                int lines = std::count(text.begin(), text.end(), '\n');
                messages[i].text = "\\>$c\n";
                while (--lines > 0)
                    messages[i].text.append("\\>$c\n");
            } else {
                messages[i].text = "\\>$c\n";
            }
        }
    }

    //Apply patch
    size_t iMessage = 0;

    while (lcf.bytesRemain()) {
        //Get event info
        int code = lcf.readInt();
        if (code == 0) {
            lcf.writeInt(0);
            continue;
        }
        int indent = lcf.readInt();
        std::string string = lcf.readString();
        int argc = lcf.readInt();

        if (rmintro) {
            //First call event; we can stop removing stuff
            if (code == 12330)
                rmintro = false;
            //Discard args
            for (int i = 0; i < argc; ++i)
                lcf.readInt();
            continue;
        }
        //Parse event!
        switch (code) {
        /***********
         * COMMENT */
        case 12410:
        case 22410:
            continue; //don't write comments
        /***************
         * ASSET STUFF */
        case 11110:
            string = getAsset(Asset::Picture, string);
            break;
        case 11510:
        case 10660:
            string = getAsset(Asset::Music, string);
            break;
        case 11550:
        case 10670:
            string = getAsset(Asset::Sound, string);
            break;
        case 11720:
            string = getAsset(Asset::Panorama, string);
            break;
        case 10630:
            string = getAsset(Asset::CharSet, string);
            break;
        case 10640:
            string = getAsset(Asset::FaceSet, string);
            break;
        case 10680:
            string = getAsset(Asset::System, string);
            break;

        /*****************
         * MESSAGE STUFF */
        case 10110: //begin message
        {
            if (iMessage >= messages.size())
                break;
            //Write entire message
            size_t lastpos = 0, pos = 0;
            while ((pos = messages[iMessage].text.find('\n', lastpos)) != std::string::npos) {
                lcf.writeInt(code);
                lcf.writeInt(indent);
                string = messages[iMessage].text.substr(lastpos, pos - lastpos);
                lcf.writeString(string);
                lcf.writeInt(0);
                lastpos = pos + 1;
                code = 20110;
            }
            ++iMessage;
        } //NO BREAK HERE
        case 20110: //continue message
            //don't write anything; we write the entire message in begin message
            //just read past the args
        {
            for (int i = 0; i < argc; ++i)
                lcf.readInt();
            continue;
        }

        case 10140: //show choice begin
            if (iMessage >= messages.size())
                break;
            //change initial string
            if (!messages[iMessage].text.empty()) {
                string = messages[iMessage].text;
                string.resize(string.size() - 1);
                size_t pos = 0;
                while ((pos = string.find('\n', pos)) != std::string::npos) {
                    string.replace(pos, 1, "/");
                    ++pos;
                }
            }
            ++iMessage;
            break;

        case 20140: //show choice option
            if (iMessage > messages.size())
                break;
            if (!string.empty()) {
                //find first message with same indent
                for (int i = iMessage - 1; i >= 0; --i) {
                    if (messages[i].type == 1 && messages[i].indent == indent) {
                        //set the string to the next line of the message
                        size_t pos = messages[i].text.find('\n');
                        //if (pos == std::string::npos)
                            //throw std::runtime_error("not enough lines in choice string");
                        string = messages[i].text.substr(0, pos);
                        messages[i].text.erase(0, pos + 1);
                        break;
                    }
                }
            }
            break;
        case 20141: //show choice end
            //do nothing
            break;
        case 10610: //change hero name
            if (iMessage >= messages.size())
                break;
            string = messages[iMessage++].text;
            string.resize(string.size() - 1);
            break;
        }

        //Write entry
        lcf.writeInt(code);
        lcf.writeInt(indent);
        lcf.writeString(string);
        if (code == 11330) {
            //Move event

            std::streamoff start = lcf.tell();

            /* FIRST, CONSTRUCT A LIST OF WHAT WE NEED TO WRITE AND THE SIZE */
            int size = 0;
            std::vector<std::string> strings;

            //skip first 4 args
            for (int i = 0; i < 4; ++i)
                lcf.readInt();
            argc -= 4;
            size += 4;

            while (argc > 0) {
                --argc;
                ++size;
                switch (lcf.readInt()) {
                case 32: //switch on
                case 33: //switch off
                    lcf.readInt();
                    --argc; //discard
                    ++size;
                    break;
                case 34: //change charset
                    string = lcf.readIntString();
                    strings.push_back(getAsset(Asset::CharSet, string));
                    lcf.readInt();
                    argc -= 1 + string.size() + 1;
                    size += 1 + strings.back().size() + 1;
                    break;
                case 35: //play sound
                    string = lcf.readIntString();
                    strings.push_back(getAsset(Asset::Sound, string));
                    lcf.readInt();
                    lcf.readInt();
                    lcf.readInt();
                    argc -= 1 + string.size() + 3;
                    size += 1 + strings.back().size() + 3;
                    break;
                }
            }

            /* ACTUALLY WRITE SHIT NOW */
            lcf.seek(start);
            lcf.writeInt(size);
            int iString = 0;

            //skip first 4 args
            for (int i = 0; i < 4; ++i)
                lcf.rwInt();
            size -= 4;

            while (size > 0) {
                --size;
                switch (lcf.rwInt()) {
                case 32: //switch on
                case 33: //switch off
                    lcf.rwInt();
                    --size; //discard
                    break;
                case 34: //change charset
                    lcf.readIntString();
                    //we should be okay here writing normal strings, because we're only using ascii
                    lcf.writeString(strings[iString]);
                    lcf.rwInt();
                    size -= 1 + strings[iString++].size() + 1;
                    break;
                case 35: //play sound
                    lcf.readIntString();
                    //we should be okay here writing normal strings, because we're only using ascii
                    lcf.writeString(strings[iString]);
                    lcf.rwInt();
                    lcf.rwInt();
                    lcf.rwInt();
                    size -= 1 + strings[iString++].size() + 3;
                    break;
                }
            }
        } else {
            //Advance past the args
            lcf.writeInt(argc);
            for (int i = 0; i < argc; ++i)
                lcf.rwInt();
        }
    }
}

std::string Patch::getAsset(Asset::e type, const std::string &name)
{
    //Return empty string for empty names
    if (name.empty())
        return std::string();
    //Return empty sound for empty sounds
    if ((type == Asset::Music || type == Asset::Sound) && name == "(OFF)")
        return "(OFF)";

    std::string lower = Util::fromJisToLower(name);

    //Check for relative path hack
    if (lower.compare(0, 3, "..\\") == 0) {
        //search for another \\; if one doesn't exist, we can just return the original
        if (lower.find('\\', 3) == std::string::npos)
            return name;

        //If it already exists, return the name
        AssetMap::iterator it = assets[Asset::Hack].find(lower);
        if (it != assets[Asset::Hack].end())
            return it->second.name;

        //Determine if asset exists and add it to the map
        size_t backslash = lower.find('\\', 3);
        std::string folder = lower.substr(3, backslash - 3);
        std::string file = lower.substr(backslash + 1);

        //Determine which map to use from folder name
        type = Asset::Hack;
        for (size_t i = 0; i < ARRAY_SIZE(Asset::lowerNames); ++i) {
            if (folder == Asset::lowerNames[i]) {
                type = static_cast<Asset::e>(i);
                break;
            }
        }
        if (type == Asset::Hack)
            throw std::runtime_error(name + ": could resolve relative path to game folder");

        //Does this asset exist?
        it = assets[type].find(file);
        if (it == assets[type].end() || it->second.path.empty()) {
            //Warn about missing asset, assign empty entry to both hack the asset
            std::cerr << "warning: " << Asset::names[type] << "\\" << file << ": asset does not exist" << std::endl;

            //create empty asset entry
            assets[type][file] = Asset();
            assets[Asset::Hack][lower] = Asset();
            return std::string();
        }

        //Does the asset have a name yet?
        if (it->second.name.empty())
            it->second.name = zeropad(nAssets[type]++);

        //Add to hack map
        std::ostringstream newname;
        newname << "..\\" << Asset::names[type] << "\\" << it->second.name;
        assets[Asset::Hack][lower] = Asset(it->second.path, newname.str());
        return newname.str();
    }

    //Add to map
    AssetMap::iterator it = assets[type].find(lower);
    if (it == assets[type].end()) {
        std::cerr << "warning: " << Asset::names[type] << "\\" << lower << ": asset does not exist" << std::endl;
        assets[type][lower] = Asset(); //empty path and name
        return std::string();
    } else {
        if (!it->second.path.empty() && it->second.name.empty())
            it->second.name = zeropad(nAssets[type]++);
        return it->second.name;
    }
}

void Patch::putAsset(Lcf &lcf, Asset::e type, int block)
{
    lcf.putString(block, getAsset(type, lcf.peekString(block)), false);
}
