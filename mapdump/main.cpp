#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

#include <data.h>
#include <reader_lcf.h>
#include <ldb_reader.h>
#include <lmt_reader.h>
#include <lmu_reader.h>

#include "os.h"
#include "bitmap.h"
#include "util.h"

#define OUT_DIR_NAME "DUMP"

/*
 * Big shoutout to
 * https://github.com/EasyRPG/Player/blob/ac12fc6827f8ddac3392644ffb27fd78a8ab0d2e/src/tilemap_layer.cpp
 * for the code for these insane autotiles
 */

// Blocks subtiles IDs
// Mess with this code and you will die in 3 days...
// [tile-id][row][col]
static const int8_t waterAutotileIds[47][2][2] = {
#define N -1
    {{N, N}, {N, N}},
    {{3, N}, {N, N}},
    {{N, 3}, {N, N}},
    {{3, 3}, {N, N}},
    {{N, N}, {N, 3}},
    {{3, N}, {N, 3}},
    {{N, 3}, {N, 3}},
    {{3, 3}, {N, 3}},
    {{N, N}, {3, N}},
    {{3, N}, {3, N}},
    {{N, 3}, {3, N}},
    {{3, 3}, {3, N}},
    {{N, N}, {3, 3}},
    {{3, N}, {3, 3}},
    {{N, 3}, {3, 3}},
    {{3, 3}, {3, 3}},
    {{1, N}, {1, N}},
    {{1, 3}, {1, N}},
    {{1, N}, {1, 3}},
    {{1, 3}, {1, 3}},
    {{2, 2}, {N, N}},
    {{2, 2}, {N, 3}},
    {{2, 2}, {3, N}},
    {{2, 2}, {3, 3}},
    {{N, 1}, {N, 1}},
    {{N, 1}, {3, 1}},
    {{3, 1}, {N, 1}},
    {{3, 1}, {3, 1}},
    {{N, N}, {2, 2}},
    {{3, N}, {2, 2}},
    {{N, 3}, {2, 2}},
    {{3, 3}, {2, 2}},
    {{1, 1}, {1, 1}},
    {{2, 2}, {2, 2}},
    {{0, 2}, {1, N}},
    {{0, 2}, {1, 3}},
    {{2, 0}, {N, 1}},
    {{2, 0}, {3, 1}},
    {{N, 1}, {2, 0}},
    {{3, 1}, {2, 0}},
    {{1, N}, {0, 2}},
    {{1, 3}, {0, 2}},
    {{0, 0}, {1, 1}},
    {{0, 2}, {0, 2}},
    {{1, 1}, {0, 0}},
    {{2, 0}, {2, 0}},
    {{0, 0}, {0, 0}}
#undef N
};

// [tile-id][row][col][x/y]
static const uint8_t normalAutotileIds[50][2][2][2] = {
//     T-L     T-R       B-L     B-R
    {{{1, 2}, {1, 2}}, {{1, 2}, {1, 2}}},
    {{{2, 0}, {1, 2}}, {{1, 2}, {1, 2}}},
    {{{1, 2}, {2, 0}}, {{1, 2}, {1, 2}}},
    {{{2, 0}, {2, 0}}, {{1, 2}, {1, 2}}},
    {{{1, 2}, {1, 2}}, {{1, 2}, {2, 0}}},
    {{{2, 0}, {1, 2}}, {{1, 2}, {2, 0}}},
    {{{1, 2}, {2, 0}}, {{1, 2}, {2, 0}}},
    {{{2, 0}, {2, 0}}, {{1, 2}, {2, 0}}},
    {{{1, 2}, {1, 2}}, {{2, 0}, {1, 2}}},
    {{{2, 0}, {1, 2}}, {{2, 0}, {1, 2}}},
    {{{1, 2}, {2, 0}}, {{2, 0}, {1, 2}}},
    {{{2, 0}, {2, 0}}, {{2, 0}, {1, 2}}},
    {{{1, 2}, {1, 2}}, {{2, 0}, {2, 0}}},
    {{{2, 0}, {1, 2}}, {{2, 0}, {2, 0}}},
    {{{1, 2}, {2, 0}}, {{2, 0}, {2, 0}}},
    {{{2, 0}, {2, 0}}, {{2, 0}, {2, 0}}},
    {{{0, 2}, {0, 2}}, {{0, 2}, {0, 2}}},
    {{{0, 2}, {2, 0}}, {{0, 2}, {0, 2}}},
    {{{0, 2}, {0, 2}}, {{0, 2}, {2, 0}}},
    {{{0, 2}, {2, 0}}, {{0, 2}, {2, 0}}},
    {{{1, 1}, {1, 1}}, {{1, 1}, {1, 1}}},
    {{{1, 1}, {1, 1}}, {{1, 1}, {2, 0}}},
    {{{1, 1}, {1, 1}}, {{2, 0}, {1, 1}}},
    {{{1, 1}, {1, 1}}, {{2, 0}, {2, 0}}},
    {{{2, 2}, {2, 2}}, {{2, 2}, {2, 2}}},
    {{{2, 2}, {2, 2}}, {{2, 0}, {2, 2}}},
    {{{2, 0}, {2, 2}}, {{2, 2}, {2, 2}}},
    {{{2, 0}, {2, 2}}, {{2, 0}, {2, 2}}},
    {{{1, 3}, {1, 3}}, {{1, 3}, {1, 3}}},
    {{{2, 0}, {1, 3}}, {{1, 3}, {1, 3}}},
    {{{1, 3}, {2, 0}}, {{1, 3}, {1, 3}}},
    {{{2, 0}, {2, 0}}, {{1, 3}, {1, 3}}},
    {{{0, 2}, {2, 2}}, {{0, 2}, {2, 2}}},
    {{{1, 1}, {1, 1}}, {{1, 3}, {1, 3}}},
    {{{0, 1}, {0, 1}}, {{0, 1}, {0, 1}}},
    {{{0, 1}, {0, 1}}, {{0, 1}, {2, 0}}},
    {{{2, 1}, {2, 1}}, {{2, 1}, {2, 1}}},
    {{{2, 1}, {2, 1}}, {{2, 0}, {2, 1}}},
    {{{2, 3}, {2, 3}}, {{2, 3}, {2, 3}}},
    {{{2, 0}, {2, 3}}, {{2, 3}, {2, 3}}},
    {{{0, 3}, {0, 3}}, {{0, 3}, {0, 3}}},
    {{{0, 3}, {2, 0}}, {{0, 3}, {0, 3}}},
    {{{0, 1}, {2, 1}}, {{0, 1}, {2, 1}}},
    {{{0, 1}, {0, 1}}, {{0, 3}, {0, 3}}},
    {{{0, 3}, {2, 3}}, {{0, 3}, {2, 3}}},
    {{{2, 1}, {2, 1}}, {{2, 3}, {2, 3}}},
    {{{0, 1}, {2, 1}}, {{0, 3}, {2, 3}}},
    {{{1, 2}, {1, 2}}, {{1, 2}, {1, 2}}},
    {{{1, 2}, {1, 2}}, {{1, 2}, {1, 2}}},
    {{{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}}
};

int getMapIndex(int id)
{
    for (unsigned int i = 0; i < Data::treemap.maps.size(); ++i) {
        if (Data::treemap.maps[i].ID == id)
            return i;
    }
    return 0;
}

std::string getMapPath(int id)
{
    std::string path;
    for (int index = getMapIndex(id); id != 0; id = Data::treemap.maps[index].parent_map, index = getMapIndex(id)) {
        path.insert(0, PATH_SEPARATOR);
        path.insert(0, Data::treemap.maps[index].name);
    }
    return path;
}

void drawTile(Bitmap &dst, const Bitmap &src, int dX, int dY, int tile)
{
    if (tile >= 10000 && tile < 10000 + 24 * 6) {
        //Upper layer
        tile -= 10000;
        int x = 18 + tile % 6;
        int y = tile / 6;
        if (tile < 6 * 8) {
            y += 8;
        } else {
            y -= 8;
            x += 6;
        }
        dst.blit(dX * 16, dY * 16, src, x * 16, y * 16, 16, 16);
    } else if (tile >= 5000 && tile < 5000 + 24 * 6) {
        //Normal lower layer
        tile -= 5000;
        int x = 12 + tile % 6;
        int y = tile / 6;
        if (tile >= 6 * 16) {
            y -= 16;
            x += 6;
        }
        dst.blit(dX * 16, dY * 16, src, x * 16, y * 16, 16, 16);
    } else if (tile >= 3000 && tile < 3050) {
        //Animated single water tile A
        dst.blit(dX * 16, dY * 16, src, 3 * 16, 4 * 16, 16, 16);
    } else if (tile >= 3050 && tile < 3100) {
        //Animated single water tile B
        dst.blit(dX * 16, dY * 16, src, 4 * 16, 4 * 16, 16, 16);
    } else if (tile >= 3100 && tile < 3150) {
        //Animated single water tile C
        dst.blit(dX * 16, dY * 16, src, 5 * 16, 4 * 16, 16, 16);
    } else if (tile >= 4000 && tile < 5000) {
        //Normal autotile
        int block = (tile - 4000) / 50;
        int subtile = tile - 4000 - block * 50;

        // Get Block chipset coords
        int block_x, block_y;
        if (block < 4) {
            // If from first column
            block_x = (block % 2) * 3;
            block_y = 8 + (block / 2) * 4;
        } else {
            // If from second column
            block_x = 6 + (block % 2) * 3;
            block_y = ((block - 4) / 2) * 4;
        }

        // Calculate D block subtiles
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                // Get the block D subtiles ids and get their coordinates on the chipset
                int x = (block_x + normalAutotileIds[subtile][j][i][0]) * 16 + i * 8;
                int y = (block_y + normalAutotileIds[subtile][j][i][1]) * 16 + j * 8;
                dst.blit(dX * 16 + i * 8, dY * 16 + j * 8, src, x, y, 8, 8);
            }
        }
    } else if (tile < 3000) {
        //Water tile
        int block = tile / 1000;
        int b_subtile = (tile - block * 1000) / 50;
        int a_subtile = tile - block * 1000 - b_subtile * 50;

        uint8_t quarters[2][2][2];

        // Determine block B subtiles
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                // Skip the subtile if it will be used one from A block instead
                if (waterAutotileIds[a_subtile][j][i] != -1) continue;

                // Get the block B subtiles ids and get their coordinates on the chipset
                int t = (b_subtile >> (j * 2 + i)) & 1;
                if (block == 2) t ^= 3;

                quarters[j][i][0] = 1;
                quarters[j][i][1] = 4 + t;
            }
        }

        // Determine block A subtiles
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                // Skip the subtile if it was used one from B block
                if (waterAutotileIds[a_subtile][j][i] == -1) continue;

                // Get the block A subtiles ids and get their coordinates on the chipset
                quarters[j][i][0] = 1 + (block == 1 ? 3 : 0);
                quarters[j][i][1] = waterAutotileIds[a_subtile][j][i];
            }
        }

        // Determine block B subtiles when combining A and B
        if (b_subtile != 0 && a_subtile != 0) {
            for (int j = 0; j < 2; j++) {
                for (int i = 0; i < 2; i++) {
                    // calculate tile (row 0..3)
                    int t = (b_subtile >> (j * 2 + i)) & 1;
                    if (block == 2) t *= 2;

                    // Skip the subtile if not used
                    if (t == 0) continue;

                    // Get the coordinates on the chipset
                    quarters[j][i][0] = 1;
                    quarters[j][i][1] = 4 + t;
                }
            }
        }

        //Blit
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                int x = quarters[j][i][0] * 16 + i * 8;
                int y = quarters[j][i][1] * 16 + j * 8;
                dst.blit(dX * 16 + i * 8, dY * 16 + j * 8, src, x, y, 8, 8);
            }
        }
    }
}

void dumpMap(const std::string &gamePath, const std::string &mapPath, const std::string &encoding, int id, const std::vector<Bitmap> &chipsets)
{
    //Make output dir
    std::string outPath = gamePath + OUT_DIR_NAME PATH_SEPARATOR + mapPath;
    Util::mkdir(outPath);

    //Load map
    std::ostringstream ss;
    ss << gamePath << "Map" << std::setw(4) << std::setfill('0') << id << ".lmu";
    std::auto_ptr<RPG::Map> map = LMU_Reader::Load(unicodeCompatPath(ss.str()), encoding);
    if (map.get() == NULL)
        throw std::runtime_error("could not load map file \"" + ss.str() + "\": " + LcfReader::GetError());

    if (map->chipset_id > 0) {
        //Get chipset reference
        const Bitmap &chipset = chipsets[map->chipset_id - 1];

        //Blit to bitmap
        if (!chipset.empty()) {
            //Create output image objects
            Bitmap lowerLayer(map->width * 16, map->height * 16);
            Bitmap upperLayer(map->width * 16, map->height * 16);

            for (int y = 0; y < map->height; ++y) {
                for (int x = 0; x < map->width; ++x) {
                    drawTile(lowerLayer, chipset, x, y, map->lower_layer[y * map->width + x]);
                    drawTile(upperLayer, chipset, x, y, map->upper_layer[y * map->width + x]);
                }
            }

            //Save to file
            lowerLayer.writeToPng(outPath + "lower.png", true, chipset.getPalette());
            upperLayer.writeToPng(outPath + "upper.png", true, chipset.getPalette());

            //Create composite image
            lowerLayer.blit(0, 0, upperLayer, 0, 0, upperLayer.getWidth(), upperLayer.getHeight());
            lowerLayer.writeToPng(outPath + "composite.png", true, chipset.getPalette());
        }
    }
}

int unimain(const std::vector<std::string> &args)
{
    if (args.size() < 1) {
        std::cerr << "usage: mapdump game_path [encoding]" << std::endl;
        return 1;
    }

    //Get path, make sure it ends with a path separator
    std::string gamePath = Util::sanitizeDirPath(args[0]);

    try {
        //Get LMT, LDB, and ChipSet file paths
#if defined OS_W32
        std::string lmtPath = gamePath + "RPG_RT.lmt";
        std::string ldbPath = gamePath + "RPG_RT.ldb";
        std::string chipsetPath = gamePath + "ChipSet" PATH_SEPARATOR;
#elif defined OS_UNIX
        std::string lmtPath, ldbPath, chipsetPath;
        {
            std::vector<std::string> files = Util::listFiles(gamePath);
            for (unsigned int i = 0; i < files.size(); ++i) {
                std::string lower = Util::toLower(files[i]);
                if (lower == "rpg_rt.lmt")
                    lmtPath = gamePath + files[i];
                else if (lower == "rpg_rt.ldb")
                    ldbPath = gamePath + files[i];
                else if (lower == "chipset")
                    chipsetPath = gamePath + files[i] + PATH_SEPARATOR;
                if (!lmtPath.empty() && !ldbPath.empty() && !chipsetPath.empty())
                    break;
            }
        }
        if (lmtPath.empty() || ldbPath.empty() || chipsetPath.empty()) {
            std::cerr << "error: could not find RPG_RT.lmt, RPG_RT.ldb, and ChipSet" << std::endl;
            return 1;
        }
#endif

        //Get encoding; try to detect if none specified
        std::string encoding;
        if (args.size() < 2) {
            encoding = ReaderUtil::DetectEncoding(unicodeCompatPath(ldbPath));
        } else {
            encoding = args[1];
        }

        //Load lmt/ldb tree
        if (!LMT_Reader::Load(unicodeCompatPath(lmtPath), encoding)) {
            std::cerr << "error: could not load RPG_RT.lmt: " << LcfReader::GetError();
            return 1;
        }
        if (!LDB_Reader::Load(unicodeCompatPath(ldbPath), encoding)) {
            std::cerr << "error: could not load RPG_RT.ldb: " << LcfReader::GetError();
            return 1;
        }

        if (!Util::dirExists(chipsetPath)) {
            std::cerr << "error: ChipSet directory does not exist" << std::endl;
            return 1;
        }

        //List files in chipsetPath, cache lowercase
        std::vector<std::string> chipsetFilenames = Util::listFiles(chipsetPath);
        std::vector<std::string> chipsetFilenamesLower;
        chipsetFilenamesLower.reserve(chipsetFilenames.size());
        for (unsigned int i = 0; i < chipsetFilenames.size(); ++i)
            chipsetFilenamesLower.push_back(Util::toLower(chipsetFilenames[i]));

        //Load chipset bitmaps
        std::vector<Bitmap> chipsets;
        chipsets.resize(Data::chipsets.size());
        for (unsigned int i = 0; i < chipsets.size(); ++i) {
            if (Data::chipsets[i].chipset_name.empty())
                continue;

            static const char *const exts[] = {
                ".xyz", ".png", ".bmp"
            };

            unsigned int j = 0;
            for (; j < ARRAY_SIZE(exts); ++j) {
                std::string lowername = Util::toLower(Data::chipsets[i].chipset_name) + exts[j];
                std::vector<std::string>::iterator index = std::find(chipsetFilenamesLower.begin(),
                                                                     chipsetFilenamesLower.end(),
                                                                     lowername);
                if (index != chipsetFilenamesLower.end()) {
                    chipsets[i] = Bitmap(chipsetPath + chipsetFilenames[index - chipsetFilenamesLower.begin()]);
                    break;
                }
            }
            if (j == ARRAY_SIZE(exts)) {
                std::cerr << "warning: " << Data::chipsets[i].chipset_name << ": could not find file" << std::endl;
            }
        }

        //For each map...
        Util::mkdir(gamePath + OUT_DIR_NAME);
        for (unsigned int i = 1; i < Data::treemap.tree_order.size(); ++i) {
            int id = Data::treemap.tree_order[i];
            dumpMap(gamePath, getMapPath(id), encoding, id, chipsets);
        }
    } catch (std::runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

