#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include "util.h"
#include "bitmap.h"

int unimain(const std::vector<std::string> &args)
{
    if (args.size() < 2) {
        std::cerr << "usage: 2k2xp 2k_project_path xp_project_path" << std::endl;
        return 1;
    }

    std::string srcPath = Util::sanitizeDirPath(args[0]);
    std::string dstPath = Util::sanitizeDirPath(args[1]);

    if (!Util::dirExists(dstPath))
        Util::mkdir(dstPath);
    
    //Get all paths set up
#ifdef OS_W32
    std::string charsetPath = srcPath + "CharSet";
    std::string chipsetPath = srcPath + "ChipSet";
    std::string charactersPath = dstPath + "Characters";
    std::string autotilesPath = dstPath + "Autotiles";
    std::string tilesetsPath = dstPath + "Tilesets";
    if (!Util::dirExists(charactersPath))
        Util::mkdir(charactersPath);
    if (!Util::dirExists(autotilesPath))
        Util::mkdir(autotilesPath);
    if (!Util::dirExists(tilesetsPath))
        Util::mkdir(tilesetsPath);
#else
    std::string charsetPath;
    std::string chipsetPath;
    {
        std::vector<std::string> files = Util::listFiles(srcPath);
        for (unsigned int i = 0; i < files.size(); ++i) {
            std::string fullpath = srcPath + files[i] + PATH_SEPARATOR;
            if (!Util::dirExists(fullpath))
                continue;
            std::string file = Util::toLower(files[i]);
            if (file == "charset")
                charsetPath = fullpath;
            else if (file == "chipset")
                chipsetPath = fullpath;
        }
    }
    if (charsetPath.empty() || chipsetPath.empty()) {
        std::cerr << "error: couldn't find ChipSet and CharSet folders" << std::endl;
        return 1;
    }
    std::string charactersPath;
    std::string autotilesPath;
    std::string tilesetsPath;
    {
        std::vector<std::string> files = Util::listFiles(dstPath);
        for (unsigned int i = 0; i < files.size(); ++i) {
            std::string fullpath = dstPath + files[i] + PATH_SEPARATOR;
            if (!Util::dirExists(fullpath))
                continue;
            std::string file = Util::toLower(files[i]);
            if (file == "characters")
                charactersPath = fullpath;
            else if (file == "autotiles")
                autotilesPath = fullpath;
            else if (file == "tilesets")
                tilesetsPath = fullpath;
        }
    }
    if (charactersPath.empty()) {
        charactersPath = dstPath + "Characters" PATH_SEPARATOR;
        Util::mkdir(charactersPath);
    }
    if (autotilesPath.empty()) {
        autotilesPath = dstPath + "Autotiles" PATH_SEPARATOR;
        Util::mkdir(autotilesPath);
    }
    if (tilesetsPath.empty()) {
        tilesetsPath = dstPath + "Tilesets" PATH_SEPARATOR;
        Util::mkdir(tilesetsPath);
    }
#endif
    //Convert CharSets
    std::vector<std::string> files = Util::listFiles(charsetPath);
    for (unsigned int i = 0; i < files.size(); ++i) {
        try {
            std::string noext = Util::getWithoutExtension(files[i]);
            Bitmap src(charsetPath + files[i]);

            for (int j = 0; j < 8; ++j) {
                Bitmap dst(96, 128);
                static const int translation[4] = {2, 3, 1, 0};
                for (int k = 0; k < 4; ++k) {
                    //Walk down frames 1+2
                    dst.blit(24 * 0, 32 * k, src, ((j%4) * 24 * 3) + 24 * 1, (j/4 * 32 * 4) + 32 * translation[k], 24 * 2, 32 * 1);
                    //Walk down frame 3
                    dst.blit(24 * 2, 32 * k, src, ((j%4) * 24 * 3) + 24 * 1, (j/4 * 32 * 4) + 32 * translation[k], 24 * 1, 32 * 1);
                    //Walk down frame 4
                    dst.blit(24 * 3, 32 * k, src, ((j%4) * 24 * 3) + 24 * 0, (j/4 * 32 * 4) + 32 * translation[k], 24 * 1, 32 * 1);
                }

                std::ostringstream dstName;
                dstName << charactersPath << noext << "-" << (j+1) << ".png";
                dst.writeToPng(dstName.str(), true, src.getPalette());
            }
        } catch (std::runtime_error &e) {
            std::cerr << "warning: " << e.what() << std::endl;
        }
    }

    //Convert ChipSets
    files = Util::listFiles(chipsetPath);

    //Convert/split autotiles and chipset
    for (unsigned int i = 0; i < files.size(); ++i) {
        try {
            std::string noext = Util::getWithoutExtension(files[i]);
            Bitmap src(chipsetPath + files[i]);

            //Start with autotiles
            for (unsigned int j = 0; j < 12; ++j) {
                Bitmap dst(16 * 3, 16 * 4);

                int x, y;
                if (j < 4) {
                    x = (j%2) * 16 * 3;
                    y = (j/2+2) * 16 * 4;
                } else {
                    x = (j%2+2) * 16 * 3;
                    y = (j/2-2) * 16 * 4;
                }
                dst.blit(0, 0, src, x, y, 16*3, 16*4);

                std::ostringstream dstName;
                dstName << autotilesPath << noext << "-" << (j+1) << ".png";
                dst.writeToPng(dstName.str(), true, src.getPalette());
            }

            //Do water autotiles (ugh)
            for (int j = 0; j < 2; ++j) {
                Bitmap dst(16*3*4, 16*4);

                //First offset
                int x1 = j * 16 * 3;

                for(int k = 0; k < 4; ++k) {
                    //Second offset
                    int x2;
                    if(k == 3) {
                        x2 = 16;
                    } else {
                        x2 = 16 * k;
                    }

                    //Dest offset
                    int dstx = k * 16 * 3;

                    //Super-Top left (single tile)
                    dst.blit(dstx + 16 * 0, 16 * 0, src, x1 + x2, 16 * 0, 16, 16);
                    //Super-Top right (corner tiles)
                    dst.blit(dstx + 16 * 2, 16 * 0, src, x1 + x2, 16 * 3, 16, 16);
                    //Super-Top center ("blank" tile)
                    //dst.fill(dstx + 16 * 1, 16 * 0, 16, 16, 0, 255, 0);

                    //Fill rest with water tiles; we blit partially over them
                    for(int l = 0; l < 3*3; l++) {
                        dst.blit(dstx + 16 * (l%3), 16 * (l/3+1), src, x2, 16 * 4, 16, 16);
                    }

                    //Top-left
                    dst.blit(dstx + 16 * 0, 16 * 1, src, x1 + x2, 16 * 0, 8, 8); //corner
                    dst.blit(dstx + 16 * 0 + 8, 16 * 1, src, x1 + x2 + 8, 16 * 2, 8, 8); //top
                    dst.blit(dstx + 16 * 0, 16 * 1 + 8, src, x1 + x2, 16 * 1 + 8, 8, 8); //left

                    //Top-right
                    dst.blit(dstx + 16 * 2 + 8, 16 * 1, src, x1 + x2 + 8, 16 * 0, 8, 8); //corner
                    dst.blit(dstx + 16 * 2, 16 * 1, src, x1 + x2, 16 * 2, 8, 8); //top
                    dst.blit(dstx + 16 * 2 + 8, 16 * 1 + 8, src, x1 + x2 + 8, 16 * 1 + 8, 8, 8); //right

                    //Bottom-left
                    dst.blit(dstx + 16 * 0, 16 * 3 + 8, src, x1 + x2, 16 * 0 + 8, 8, 8); //corner
                    dst.blit(dstx + 16 * 0 + 8, 16 * 3 + 8, src, x1 + x2 + 8, 16 * 2 + 8, 8, 8); //bottom
                    dst.blit(dstx + 16 * 0, 16 * 3, src, x1 + x2, 16 * 1, 8, 8); //left

                    //Bottom-right
                    dst.blit(dstx + 16 * 2 + 8, 16 * 3 + 8, src, x1 + x2 + 8, 16 * 0 + 8, 8, 8); //corner
                    dst.blit(dstx + 16 * 2, 16 * 3 + 8, src, x1 + x2, 16 * 2 + 8, 8, 8); //bottom
                    dst.blit(dstx + 16 * 2 + 8, 16 * 3, src, x1 + x2 + 8, 16 * 1, 8, 8); //right

                    //Top
                    dst.blit(dstx + 16 * 1, 16 * 1, src, x1 + x2, 16 * 2, 16, 8);
                    //Bottom
                    dst.blit(dstx + 16 * 1, 16 * 3 + 8, src, x1 + x2, 16 * 2 + 8, 16, 8);
                    //Left
                    dst.blit(dstx + 16 * 0, 16 * 2, src, x1 + x2, 16 * 1, 8, 16);
                    //Right
                    dst.blit(dstx + 16 * 2 + 8, 16 * 2, src, x1 + x2 + 8, 16 * 1, 8, 16);
                }

                std::ostringstream dstName;
                dstName << autotilesPath << noext << "-water" << (j+1) << ".png";
                dst.writeToPng(dstName.str(), true, src.getPalette());
            }

            //Do the tileset (easy street)
            Bitmap dst(16 * 8, 16 * 36);
            //Copy layer 1 square chunk
            dst.blit(16 * 0, 16 * 0, src, 16 * 3 * 4, 16 * 0, 16 * 8, 16 * 8);
            //Copy layer 1 segmented square
            dst.blit(16 * 0, 16 * 8, src, 16 * 3 * 4 + 16 * 8, 16 * 0, 16 * 4, 16 * 8);
            dst.blit(16 * 4, 16 * 8, src, 16 * 3 * 4, 16 * 8, 16 * 4, 16 * 8);
            //Copy remainder of layer 1
            for(int j = 0; j < 4; j++) {
                dst.blit(16 * j * 2, 16 * 8 * 2, src, 16 * 3 * 4 + 16 * 4, 16 * 8 + 16 * j * 2, 16 * 2, 16 * 2);
            }

            //Copy layer 2 square chunk
            dst.blit(16 * 0, 16 * 8 * 2 + 16 * 2, src, 16 * 3 * 4 + 16 * 6, 16 * 8, 16 * 8, 16 * 8);
            //Copy layer 2 segmented square
            dst.blit(16 * 0, 16 * 8 * 3 + 16 * 2, src, 16 * 3 * 4 + 16 * 12, 16 * 0, 16 * 6, 16 * 8);
            dst.blit(16 * 6, 16 * 8 * 3 + 16 * 2, src, 16 * 3 * 4 + 16 * 14, 16 * 8, 16 * 2, 16 * 8);
            //Copy remainder of layer 2
            for(int j = 0; j < 4; j++) {
                dst.blit(16 * j * 2, 16 * 8 * 4 + 16 * 2, src, 16 * 3 * 4 + 16 * 16, 16 * 8 + 16 * j * 2, 16 * 2, 16 * 2);
            }

            //Write the file
            dst.writeToPng(tilesetsPath + files[i], true, src.getPalette());
        } catch (std::runtime_error &e) {
            std::cerr << "warning: " << e.what() << std::endl;
        }
    }

    return 0;
}
