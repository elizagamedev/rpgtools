#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include "rgssa.h"
#include "os.h"
#include "util.h"
#include "bitmap.h"

/* ARCHIVE NAMESPACES */
namespace Wolf
{
void unpack(const std::string &filename, const std::string &outpath);
}

namespace Rgssa1
{
void pack(const std::string &filename, const std::string &srcpath, const std::vector<Rgssa::File> &srcfiles);
}

namespace Rgssa3
{
void pack(const std::string &filename, const std::string &srcpath, const std::vector<Rgssa::File> &srcfiles);
}

static inline void usage()
{
    std::cerr << "usage: rpgconv [game_or_project_dir]" << std::endl;
}

int unimain(const std::vector<std::string> &args)
{
    //Get game path
    std::string gamePath;
    if (args.size() == 0)
        gamePath = "." PATH_SEPARATOR;
    else
        gamePath = Util::sanitizeDirPath(args[0]);

    try {
        //Collect a bunch of information about the game for later
        std::string projFile;
        std::string rgssaFile;
        std::string wolfFile;
        std::string dataFolder;
        std::string graphicsFolder;
        std::string iniFile;
        std::vector<std::string> rpg2kFolders;
        int rgssver = 0;
        bool ldbFound = false;
        std::vector<std::string> gameFiles = Util::listFiles(gamePath);
        for (unsigned int i = 0; i < gameFiles.size(); ++i) {
            std::string file = Util::toLower(gameFiles[i]);
            std::string ext = Util::getExtension(file);
            if (file == "data.wolf") {
                wolfFile = gameFiles[i];
            } else if (ext == "rxproj") {
                rgssver = 1;
                projFile = gameFiles[i];
            } else if (ext == "rvproj") {
                rgssver = 2;
                projFile = gameFiles[i];
            } else if (ext == "rvproj2") {
                rgssver = 3;
                projFile = gameFiles[i];
            } else if (ext == "rgssad") {
                rgssver = 1;
                rgssaFile = gameFiles[i];
            } else if (ext == "rgss2a") {
                rgssver = 2;
                rgssaFile = gameFiles[i];
            } else if (ext == "rgss3a") {
                rgssver = 3;
                rgssaFile = gameFiles[i];
            } else if (ext == "ini") {
                iniFile = gameFiles[i];
            } else if (ext == "ldb") {
                ldbFound = true;
            } else if (Util::dirExists(gamePath + gameFiles[i])) {
                if (file == "data") {
                    dataFolder = gameFiles[i];
                } else if (file == "graphics") {
                    graphicsFolder = gameFiles[i];
                } else {
                    static const char *const rpg2kFoldersMaster[] = {
                        "backdrop",
                        "battle",
                        "battle2",
                        "battlecharset",
                        "battleweapon",
                        "charset",
                        "chipset",
                        "faceset",
                        "frame",
                        "gameover",
                        "monster",
                        "panorama",
                        "picture",
                        "system",
                        "system2",
                        "title",
                    };
                    //Check if this is a 2k/2k3 folder; add to vector
                    for (unsigned int j = 0; j < ARRAY_SIZE(rpg2kFoldersMaster); ++j) {
                        if (file == rpg2kFoldersMaster[j]) {
                            rpg2kFolders.push_back(gameFiles[i] + PATH_SEPARATOR);
                            break;
                        }
                    }
                }
            }
        }

        //If we don't know our rgss version, determine via ini
        if (rgssver == 0 && !iniFile.empty()) {
            std::string fileData = Util::toLower(Util::readFileContents(gamePath + iniFile));
            if (fileData.find("rtp=rpgvxace") != std::string::npos
                    || fileData.find("library=rgss3") != std::string::npos) {
                rgssver = 3;
            } else if (fileData.find("rtp=rpgvx") != std::string::npos
                       || fileData.find("library=rgss2") != std::string::npos) {
                rgssver = 2;
            } else if (fileData.find("rtp1=") != std::string::npos
                       || fileData.find("library=rgss1") != std::string::npos) {
                rgssver = 1;
            }
        }

        //Determine if we should be converting to or from "edit" or "release"
        //IF: any (INDEXED) PNG or BMP: TO RELEASE
        //IF: no PNG or BMP: TO EDIT
        //IF: Data AND !Data.wolf: TO RELEASE (not implemented)
        //IF: Data.wolf AND !Data: TO EDIT
        //IF: Data.wolf AND Data: ERROR
        //IF: rxproj/rvproj/rvproj2 AND !rgssad/rgss2a/rgss3a: TO RELEASE
        //IF: rgssad/rgss2a/rgss3a AND !rxproj/rvproj/rvproj2: TO EDIT
        //IF: rxproj/rvproj/rvproj2 AND rgssad/rgss2a/rgss3a: ERROR
        bool convertToProject;
        if (ldbFound) { //RPG Maker 2000/2003
            convertToProject = true;
            for (unsigned int i = 0; i < rpg2kFolders.size(); ++i) {
                std::string path = gamePath + rpg2kFolders[i];
                std::vector<std::string> files = Util::listFiles(path);
                for (unsigned int j = 0; j < files.size(); ++j) {
                    std::string ext = Util::getExtension(files[j]);
                    if ((ext == "png" || ext == "bmp") && Bitmap::isIndexed(path + files[j])) {
                        convertToProject = false;
                        break;
                    }
                }
                if (!convertToProject)
                    break;
            }
        } else if (rgssver == 0) { //Wolf RPG Editor (?)
            if (!wolfFile.empty() && !dataFolder.empty()) {
                std::cerr << "error: data folder and archive file both found; cannot determine which way to convert" << std::endl;
                return 1;
            }
            if (!dataFolder.empty() && wolfFile.empty())
                convertToProject = false;
            else if (!wolfFile.empty() && dataFolder.empty())
                convertToProject = true;
            else {
                std::cout << "error: " << gamePath << ": does not appear to be an RGSS or Wolf RPG game folder" << std::endl;
                usage();
                return 1;
            }
        } else { //RGSS
            if (!projFile.empty() && !rgssaFile.empty()) {
                std::cerr << "error: project and archive file both found; cannot determine which way to convert" << std::endl;
                return 1;
            }
            if (projFile.empty() && rgssaFile.empty())
                convertToProject = false;
            else
                convertToProject = projFile.empty();
        }

        //Do the conversion
        if (ldbFound) { //RPG Maker 2000/2003
            for (unsigned int i = 0; i < rpg2kFolders.size(); ++i) {
                std::string path = gamePath + rpg2kFolders[i];
                std::vector<std::string> files = Util::listFiles(path);
                for (unsigned int j = 0; j < files.size(); ++j) {
                    std::string file = path + files[j];
                    std::string outname = Util::getWithoutExtension(file);
                    std::string ext = Util::getExtension(files[j]);
                    try {
                        if (convertToProject) {
                            if (ext == "xyz") {
                                Bitmap bitmap(file);
                                bitmap.writeToPng(outname + ".png", false);
                                Util::deleteFile(file);
                            }
                        } else if (ext == "png" || ext == "bmp") {
                            Bitmap bitmap(file);
                            bitmap.writeToXyz(outname + ".xyz");
                            Util::deleteFile(file);
                        }
                    } catch (std::runtime_error &e) {
                        std::cerr << "warning: " << e.what() << std::endl;
                    }
                }
            }
        } else if (rgssver == 0) { //Wolf RPG
            if (convertToProject) {
                //Unpack archive, delete, done
                Wolf::unpack(gamePath + wolfFile, gamePath);
                Util::deleteFile(gamePath + wolfFile);
            } else {
                std::cerr << "error: cannot yet build release version of Wolf RPG Editor games" << std::endl;
                return 1;
            }
        } else { //RGSS
            if (convertToProject) {
                //Unpack archive
                Rgssa::unpack(gamePath + rgssaFile, gamePath);

                //Delete archive
                Util::deleteFile(gamePath + rgssaFile);

                //Create project file
                const char *const projexts[] = {".rxproj", ".rvproj", ".rvproj2"};
                const char *const projdata[] = {
                    "RPGXP 1.00", //"RPGXP 1.01",
                    "RPGVX 1.00", //"RPGVX 1.02",
                    "RPGVXAce 1.00", //"RPGVXAce 1.02",
                };
                ofstream projfile((gamePath + Util::getWithoutExtension(rgssaFile) + projexts[rgssver - 1]).c_str());
                projfile << projdata[rgssver - 1];
            } else {
                //Get list of files to archive
                std::vector<Rgssa::File> files;
                if (!dataFolder.empty())
                    listFilesRecursively(files, gamePath + dataFolder + PATH_SEPARATOR, dataFolder + PATH_SEPARATOR);
                if (!graphicsFolder.empty())
                    listFilesRecursively(files, gamePath + graphicsFolder + PATH_SEPARATOR, graphicsFolder + PATH_SEPARATOR);

                //Pack them
                const char *const arcexts[] = {".rgssad", ".rgss2a", ".rgss3a"};
                std::string filename;
                if (!rgssaFile.empty())
                    filename = Util::getWithoutExtension(rgssaFile) + arcexts[rgssver - 1];
                else if (!iniFile.empty())
                    filename = Util::getWithoutExtension(iniFile) + arcexts[rgssver - 1];
                else
                    filename = std::string("Game") + arcexts[rgssver - 1];
                if (rgssver == 3)
                    Rgssa3::pack(gamePath + filename, gamePath, files);
                else
                    Rgssa1::pack(gamePath + filename, gamePath, files);

                //Delete Data, Graphics, project file
                if (!projFile.empty())
                    Util::deleteFile(gamePath + projFile);
                if (!dataFolder.empty())
                    Util::deleteFolder(gamePath + dataFolder);
                if (!graphicsFolder.empty())
                    Util::deleteFolder(gamePath + graphicsFolder);
            }
        }
    } catch (std::runtime_error &e) {
        std::cout << "error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
