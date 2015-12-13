#ifndef PATCH_H
#define PATCH_H

#include "tr.h"

#include <map>
#include <vector>

class Lcf;

struct Asset
{
    enum e {
        Battle,
        CharSet,
        ChipSet,
        FaceSet,
        GameOver,
        Music,
        Panorama,
        Picture,
        Sound,
        System,
        Title,

        Hack,
        MAX_NoHack = Hack,
        MAX
    };

    Asset() {}
    Asset(const std::string &path) :
        path(path)
    {}
    Asset(const std::string &path, const std::string &name) :
        path(path),
        name(name)
    {}

    std::string path;
    std::string name;

    //Static
    static const char *const names[];
    static const char *const lowerNames[];
};

typedef std::map<std::string, Asset> AssetMap;

class Patch
{
public:
    Patch(const std::string &patchPath, const std::string &gamePath, const std::string &rtpPath);

    void apply(const std::string &outPath);

private:
    /*
    void getLmtAssets(const std::string &filename);
    void getLdbAssets(const std::string &filename);
    void getLmuAssets(const std::string &filename);
    void getEventAssets(Lcf &lcf);
    */

    void applyToLmt(const std::string &inFilename, const std::string &outFilename);
    void applyToLdb(const std::string &inFilename, const std::string &outFilename);
    void applyToLmu(const std::string &inFilename, const std::string &outFilename, bool rmintro);
    void applyToEvent(Lcf &lcf, bool rmintro);

    void addAsset(Asset::e type, const std::string &name);
    std::string getAsset(Asset::e type, const std::string &name);
    void putAsset(Lcf &lcf, Asset::e type, int block);

    //Translations
    NameTrMap heroes;
    NameTrMap skills;
    NameTrMap items;
    VocabTr vocab;
    EventTrMap events;

    //Asset map
    int nAssets[Asset::MAX_NoHack];
    AssetMap assets[Asset::MAX];

    //Physical files
    std::string patchPath;
    std::string gamePath;
    std::string ldbPath;
    std::string lmtPath;
    std::vector<std::string> gameFileNames;
};

#endif // PATCH_H

