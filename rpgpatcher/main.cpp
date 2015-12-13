#include <iostream>
#include <string>
#include <stdexcept>

#include "os.h"
#include "util.h"
#include "patch.h"
#include "unpatch.h"

static inline void usage()
{
    std::cerr << "usage: rpgpatcher [-g game_dir output_file | patch_dir game_dir rtp_dir out_dir]" << std::endl;
}

int unimain(const std::vector<std::string> &args)
{
    if (args.size() < 3) {
        usage();
        return 1;
    }

    try {
        if (args[0] == "-g") {
            std::string gamePath = Util::sanitizeDirPath(args[1]);
            std::string outFilename = Util::sanitizePath(args[2]);
            unpatch(gamePath, outFilename);
        } else {
            if (args.size() < 4) {
                usage();
                return 1;
            }
            //Get paths
            std::string patchPath = Util::sanitizeDirPath(args[0]);
            std::string gamePath = Util::sanitizeDirPath(args[1]);
            std::string rtpPath = Util::sanitizeDirPath(args[2]);
            std::string outPath = Util::sanitizeDirPath(args[3]);

            //Create patch, apply
            Patch patch(patchPath, gamePath, rtpPath);
            patch.apply(outPath);
        }
    } catch (std::runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
    } catch (std::ios::failure &e) {
        std::cerr << "error: i/o error" << std::endl;
    }

    return 0;
}
