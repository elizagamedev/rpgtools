#ifndef TR_H
#define TR_H

#include <map>
#include <string>

class NameTr;
class EventTr;
typedef std::map<std::string, NameTr> NameTrMap;
typedef std::map<std::string, EventTr> EventTrMap;

class EventTr
{
public:
    EventTr() {}
    EventTr(const std::string &orig, const std::string &tr) :
        orig(orig),
        tr(tr)
    {}

    std::string orig, tr;

    static EventTrMap createMap(const std::string &filename);
};

class NameTr
{
public:
    NameTr() {}
    NameTr(const std::string &jname, const std::string &name, const std::string &desc) :
        jname(jname),
        name(name),
        desc(desc)
    {}

    std::string jname, name, desc;

    static NameTrMap createMap(const std::string &filename);
};

class VocabTr
{
public:
    VocabTr() {}
    VocabTr(const std::string &filename);

    std::string currency;
    std::string level_abbr;
    //std::string hp_abbr;
    std::string arms;
    std::string command_items;
    std::string command_skills;
    std::string command_equip;
    std::string command_end;
    std::string title_new;
    std::string title_load;
    std::string title_quit;
    std::string choose_save;
    std::string choose_load;
    std::string choose_file;
    std::string quit_confirm;
    std::string yes;
    std::string no;
};

#endif // TR_H

