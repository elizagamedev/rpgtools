#include <string>
#include <vector>
#include <stdexcept>

#include "bitmap.h"
#include "util.h"

int unimain(const std::vector<std::string> &args)
{
    if (args.size() == 0) {
        std::cerr << "usage: xyz files_to_convert..." << std::endl;
        return 1;
    }

    for (unsigned int i = 0; i < args.size(); ++i) {
        //Load the bitmap
        Bitmap bitmap;
        try {
            bitmap = Bitmap(args[i]);
        } catch (std::runtime_error &e) {
            std::cerr << "warning: " << e.what() << std::endl;
            continue;
        }
        if (bitmap.empty()) {
            std::cerr << "warning: " << args[i] << ": could not read file" << std::endl;
            continue;
        }

        //Write to file
        std::string filename = Util::sanitizePath(args[i]);
        std::string ext = Util::getExtension(filename);
        std::string outname = Util::getWithoutExtension(filename) + ".";
        if (ext == "xyz")
            bitmap.writeToPng(outname + "png", false);
        else
            bitmap.writeToXyz(outname + "xyz");
    }
    return 0;
}
