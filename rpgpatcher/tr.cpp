#include "tr.h"

#include <stdexcept>
#include <sstream>
#include <algorithm>

#include "os.h"
#include "util.h"

#include "common.h"

NameTrMap NameTr::createMap(const std::string &filename)
{
    ifstream file(filename.c_str());
    file.exceptions(static_cast<std::ios::iostate>(0));
    NameTrMap result;

    for (;;) {
        std::string jname = readline(file);
        std::string name = readline(file);
        std::string desc = readline(file);
        if (jname.empty() || name.empty() || desc.empty())
            break;
        result[jname] = NameTr(jname, name, desc);
    }
    return result;
}

VocabTr::VocabTr(const std::string &filename)
{
    ifstream file(filename.c_str());
    file.exceptions(static_cast<std::ios::iostate>(0));
    for (;;) {
        std::string name = readline(file);
        std::string text = readline(file);
        if (name.empty() || text.empty())
            break;
#define VOCAB(_name) if (name == #_name) _name = text; else
        VOCAB(currency)
        VOCAB(level_abbr)
        //VOCAB(hp_abbr)
        VOCAB(arms)
        VOCAB(command_items)
        VOCAB(command_skills)
        VOCAB(command_equip)
        VOCAB(command_end)
        VOCAB(title_new)
        VOCAB(title_load)
        VOCAB(title_quit)
        VOCAB(choose_save)
        VOCAB(choose_load)
        VOCAB(choose_file)
        VOCAB(quit_confirm)
        VOCAB(yes)
        VOCAB(no)
            throw std::runtime_error(name + ": unknown vocabulary");
#undef VOCAB
    }
}

EventTrMap EventTr::createMap(const std::string &path) {
    EventTrMap result;
    std::vector<std::string> files = Util::listFiles(path);

    for (size_t i = 0; i < files.size(); ++i) {
        ifstream file((path + files[i]).c_str());
        file.exceptions(static_cast<std::ios::iostate>(0));


        enum {
            Text,
            BeginEndBlock,
            Separator,
        } lineType = Text;

        enum {
            WaitingForBegin,
            ReadingJapanese,
            ReadingEnglish,
        } state = WaitingForBegin;

        //Buffers for the original and translated text
        std::stringstream orig;
        std::stringstream tr;

        for (;;) {
            //Get the line, line type
            std::string line;
            if (state == WaitingForBegin)
                line = readline(file);
            else
                std::getline(file, line);
            if (file.eof())
                break;

            if (line.size() == 80) {
                char initial = line[0];
                if (initial == '=' || initial == '-') {
                    int i = 1;
                    for (; i < 80; ++i) {
                        if (line[i] != initial)
                            break;
                    }
                    if (i == 80) {
                        if (initial == '=')
                            lineType = BeginEndBlock;
                        else
                            lineType = Separator;
                    } else {
                        lineType = Text;
                    }
                } else {
                    lineType = Text;
                }
            } else {
                lineType = Text;
            }

            //Do stuff
            switch (lineType) {
            case Text:
                if (state == WaitingForBegin)
                    throw std::runtime_error("\"" + line + "\": not in block");
                if (state == ReadingJapanese)
                    orig << line << "\n";
                else if (state == ReadingEnglish)
                    tr << line << "\n";
                break;
            case BeginEndBlock:
                if (state == ReadingJapanese) {
                    std::string firstline;
                    std::getline(orig, firstline);
                    if (firstline.empty())
                        throw std::runtime_error("no text to translate specified");
                    else
                        throw std::runtime_error("\"" + firstline + " ...\": no translation specified");
                } else if (state == WaitingForBegin) {
                    state = ReadingJapanese;
                } else if (state == ReadingEnglish) {
                    state = WaitingForBegin;
                    std::string origstr = orig.str();
                    std::string trstr = tr.str();
                    //if trstr only consists of newlines, make it empty
                    if (static_cast<size_t>(std::count(trstr.begin(), trstr.end(), '\n')) == trstr.size())
                        trstr = std::string();
                    if (!isAscii(trstr))
                        std::cerr << "warning: " << files[i] << ": not ASCII:\n" << trstr << std::endl;
                    result[origstr] = EventTr(origstr, trstr);
                    orig.str(std::string());
                    orig.clear();
                    tr.str(std::string());
                    tr.clear();
                }
                break;
            case Separator:
                if (state == WaitingForBegin) {
                    throw std::runtime_error("stray separator");
                } else if (state == ReadingEnglish) {
                    std::string firstline;
                    std::getline(tr, firstline);
                    if (firstline.empty())
                        throw std::runtime_error("double separator");
                    else
                        throw std::runtime_error("\"" + firstline + " ...\": double separator");
                } else if (state == ReadingJapanese) {
                    state = ReadingEnglish;
                }
                break;
            }
        }
    }

    return result;
}
