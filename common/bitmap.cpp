#include "bitmap.h"

#include <stdexcept>
#include <algorithm>

#include <cstdlib>
#include <cstring>
#include <cassert>

#include <png.h>
#include <zlib.h>

#include "os.h"
#include "util.h"

#define ERROR_GENERIC "unknown read error"

static const char xyzMagicNumber[] = {'X', 'Y', 'Z', '1'};
static const char bmpMagicNumber[] = {'B', 'M'};

/* constructors and destructors */
Bitmap::Bitmap() :
    width(0), height(0)
{
}

Bitmap::Bitmap(const std::string &filename) :
    width(0), height(0)
{
    std::string ext = Util::getExtension(filename);
    if (ext == "xyz")
        readFromXyz(filename);
    else if (ext == "png")
        readFromPng(filename);
    else if (ext == "bmp")
        readFromBmp(filename);
    else
        throw std::runtime_error(filename + ": could not determine file type");
}

Bitmap::Bitmap(unsigned int width, unsigned int height) :
    width(width), height(height),
    pixels(width * height)
{
}

void Bitmap::readFromXyz(const std::string &filename)
{
    try {
        char magicNumber[sizeof(xyzMagicNumber)];
        uLongf size;

        ifstream file(filename.c_str());
        file.read(magicNumber, 4);

        if (std::memcmp(magicNumber, xyzMagicNumber, sizeof(magicNumber)))
            throw std::runtime_error("not a valid XYZ file");

        //Read the width and height
        width = height = 0;
        file.read(reinterpret_cast<char*>(&width), 2);
        file.read(reinterpret_cast<char*>(&height), 2);

        //Read the rest into a buffer
        file.seekg(0, file.end);
        size = static_cast<uLongf>(file.tellg()) - 8; //8 bytes for header
        if (size == 0)
            throw std::runtime_error("not a valid XYZ file");
        std::vector<Bytef> dataCompressed(size);
        file.seekg(8, file.beg);
        file.read(reinterpret_cast<char*>(dataCompressed.data()), dataCompressed.size());

        //Uncompress!
        size = 256 * 3 + width * height;
        std::vector<Bytef> data(size);
        if (uncompress(data.data(), &size, dataCompressed.data(), dataCompressed.size()) != Z_OK)
            throw std::runtime_error("zlib error");
        if (size != data.size())
            throw std::runtime_error("uncompressed image data too small");

        //Copy data to the correct buffers
        palette = std::vector<uint8_t>(256 * 3);
        std::copy(data.begin(), data.begin() + 256 * 3, palette.begin());
        pixels = std::vector<uint8_t>(width * height);
        std::copy(data.begin() + 256 * 3, data.begin() + 256 * 3 + width * height, pixels.begin());
    } catch (ifstream::failure &e) {
        throw std::runtime_error(ERROR_GENERIC);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void Bitmap::readFromPng(const std::string &filename)
{
    std::string error;

    png_structp png = NULL;
    png_infop info = NULL;
    png_bytepp rows = NULL;

    FILE *file = Util::fopen(filename, U("rb"));
    if (!file) {
        error = "could not open file";
        goto end;
    }

    //Check the header
    png_byte header[8];
    if (fread(header, 8, 1, file) != 1) {
        error = "could not validate as PNG";
        goto end;
    }
    if(png_sig_cmp(header, 0, 8) != 0) {
        error = "not a valid PNG file";
        goto end;
    }

    //Create
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        error = ERROR_GENERIC;
        goto end;
    }
    info = png_create_info_struct(png);
    if (info == NULL) {
        error = ERROR_GENERIC;
        goto end;
    }

    if (setjmp(png_jmpbuf(png))) {
        error = ERROR_GENERIC;
        goto end;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_png(png, info, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING, NULL);

    //Basic info
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    if (width == 0 || height == 0) {
        error = "invalid image dimensions";
        goto end;
    }

    //Get rows
    rows = png_get_rows(png, info);

    //Check the colortype
    switch(png_get_color_type(png, info)) {
    case PNG_COLOR_TYPE_PALETTE: {
        //Get palette
        uint8_t *pngPalette;
        int nPalette;
        png_get_PLTE(png, info, reinterpret_cast<png_colorpp>(&pngPalette), &nPalette);

        //Populate palette
        palette = std::vector<uint8_t>(nPalette * 3);
        std::copy(pngPalette, pngPalette + nPalette * 3, palette.begin());

        //Populate pixels
        pixels = std::vector<uint8_t>(width * height);
        for (unsigned int y = 0; y < height; ++y)
            std::copy(rows[y], rows[y] + width, pixels.begin() + y * width);
    } break;
    default:
        error = "PNG not indexed";
        goto end;
    }
end:
    if (png != NULL || info != NULL) {
        png_free_data(png, info, PNG_FREE_ALL, -1);
        png_destroy_read_struct(&png, &info, NULL);
    }
    if (file)
        fclose(file);

    if (!error.empty())
        throw std::runtime_error(filename + ": " + error);
}

void Bitmap::readFromBmp(const std::string &filename)
{
    try {
        char magicNumber[sizeof(bmpMagicNumber)];
        int32_t temp;

        ifstream file(filename.c_str());

        //Read and verify magic number
        file.read(magicNumber, sizeof(magicNumber));
        if (std::memcmp(magicNumber, bmpMagicNumber, sizeof(magicNumber)))
            throw std::runtime_error("not a valid BMP file");

        //Read: pixel data offset, palette offset
        file.seekg(10);
        uint32_t pixelOffset;
        file.read(reinterpret_cast<char*>(&pixelOffset), 4);
        uint32_t paletteOffset;
        file.read(reinterpret_cast<char*>(&paletteOffset), 4);
        paletteOffset += 14;

        //Read: width, height, pixel order; basic sanity checking
        width = 0;
        file.read(reinterpret_cast<char*>(&width), 4);
        file.read(reinterpret_cast<char*>(&temp), 4);
        if (width == 0 || temp == 0)
            throw std::runtime_error("invalid image dimensions");
        bool topDown = temp < 0;
        height = abs(temp);

        //More sanity checking
        temp = 0;
        file.read(reinterpret_cast<char*>(&temp), 2);
        if (temp != 1)
            throw std::runtime_error("number of BMP planes is not 1");
        file.read(reinterpret_cast<char*>(&temp), 2);
        if (temp != 8)
            throw std::runtime_error("BMP is not 8-bit");
        file.read(reinterpret_cast<char*>(&temp), 4);
        if (temp != 0)
            throw std::runtime_error("BMP is compressed");

        //Read palette info
        file.seekg(14 + 32);
        uint32_t nPalette;
        file.read(reinterpret_cast<char*>(&nPalette), 4);
        if (nPalette > 256)
            throw std::runtime_error("BMP header specifies more than 256 colors");
        if (nPalette == 0)
            nPalette = 256;

        //Read palette
        std::vector<uint8_t> bmpPalette(nPalette * 4);
        file.seekg(paletteOffset);
        file.read(reinterpret_cast<char*>(bmpPalette.data()), bmpPalette.size());

        //Read pixels
        std::vector<uint8_t> bmpPixels(width * height);
        file.seekg(pixelOffset);
        file.read(reinterpret_cast<char*>(bmpPixels.data()), bmpPixels.size());

        //Populate palette
        palette = std::vector<uint8_t>(nPalette * 3);
        for (unsigned int i = 0; i < nPalette; ++i) {
            palette[i * 3 + 0] = bmpPalette[i * 4 + 2];
            palette[i * 3 + 1] = bmpPalette[i * 4 + 1];
            palette[i * 3 + 2] = bmpPalette[i * 4 + 0];
        }

        //Populate pixels
        pixels = std::vector<uint8_t>(width * height);
        for (unsigned int y = 0; y < height; ++y) {
            int offset = (topDown ? y : height - 1 - y) * width;
            std::copy(bmpPixels.begin() + offset, bmpPixels.begin() + offset + width, pixels.begin() + y * width);
        }
    } catch (ifstream::failure &e) {
        throw std::runtime_error(filename + ": " + ERROR_GENERIC);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void Bitmap::blit(int mX, int mY, const Bitmap &other, int oX, int oY, int oW, int oH)
{
    for (int y = 0; y < oH; ++y) {
        for (int x = 0; x < oW; ++x) {
            int oOffset = (oY + y) * other.width + oX + x;
            int mOffset = (mY + y) * width + mX + x;
            if (other.pixels[oOffset] != 0)
                pixels[mOffset] = other.pixels[oOffset];
        }
    }
}

void Bitmap::writeToXyz(const std::string &filename, const std::vector<uint8_t> &palette) const
{
    try {
        uLongf size;

        ofstream file(filename.c_str());
        file.write(xyzMagicNumber, 4);

        //Write the width and height
        file.write(reinterpret_cast<const char*>(&width), 2);
        file.write(reinterpret_cast<const char*>(&height), 2);

        //Create a buffer to compress
        std::vector<Bytef> data(256*3 + width * height);
        std::copy(palette.begin(), palette.end(), data.begin());
        std::fill(data.begin() + palette.size(), data.begin() + 256 * 3, 0);
        std::copy(pixels.begin(), pixels.end(), data.begin() + 256 * 3);

        //Create a compressed buffer, compress it
        size = compressBound(256 * 3 + width * height);
        std::vector<Bytef> dataCompressed(size);
        if (compress(dataCompressed.data(), &size, data.data(), data.size()) != Z_OK)
            throw std::runtime_error("zlib error");

        //Write to file
        file.write(reinterpret_cast<const char*>(dataCompressed.data()), size);
    } catch (ifstream::failure &e) {
        throw std::runtime_error(ERROR_GENERIC);
    } catch (std::runtime_error &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void Bitmap::writeToPng(const std::string &filename, bool transparent, const std::vector<uint8_t> &palette) const
{
    volatile bool failed = false;

    //Transparent color index
    png_byte trans = 0;

    png_structp png = NULL;
    png_infop info = NULL;

    //Open file for writing
    FILE *file = Util::fopen(filename, U("wb"));
    if (!file) {
        failed = true;
        goto end;
    }

    //Initialize write structure
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        failed = true;
        goto end;
    }

    //Initialize info structure
    info = png_create_info_struct(png);
    if (info == NULL) {
        failed = true;
        goto end;
    }

    //Setup Exception handling
    if (setjmp(png_jmpbuf(png))) {
        failed = true;
        goto end;
    }

    png_init_io(png, file);

    //Write header
    png_set_IHDR(png, info, width, height,
                 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_PLTE(png, info, reinterpret_cast<const png_color*>(palette.data()), palette.size() / 3);
    if (transparent)
        png_set_tRNS(png, info, &trans, 1, NULL);
    png_write_info(png, info);

    //Write image data
    for (unsigned int y = 0; y < height; ++y)
        png_write_row(png, pixels.data() + y * width);

    //End
    png_write_end(png, info);

end:
    if (png || info) {
        png_free_data(png, info, PNG_FREE_ALL, -1);
        png_destroy_write_struct(&png, &info);
    }
    if (file)
        fclose(file);
    if (failed)
        throw std::runtime_error("unknown error while writing PNG");
}

bool Bitmap::isIndexed(const std::string &filename)
{
    std::string ext = Util::getExtension(filename);
    if (ext == "png") {
        volatile bool success = true;

        png_structp png = NULL;
        png_infop info = NULL;

        int width, height;

        FILE *file = Util::fopen(filename, U("rb"));
        if (!file) {
            success = false;
            goto end;
        }

        //Check the header
        png_byte header[8];
        if (fread(header, 8, 1, file) != 1) {
            success = false;
            goto end;
        }
        if(png_sig_cmp(header, 0, 8) != 0) {
            success = false;
            goto end;
        }

        //Create
        png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png == NULL) {
            success = false;
            goto end;
        }
        info = png_create_info_struct(png);
        if (info == NULL) {
            success = false;
            goto end;
        }

        if (setjmp(png_jmpbuf(png))) {
            success = false;
            goto end;
        }

        png_init_io(png, file);
        png_set_sig_bytes(png, 8);
        png_read_info(png, info);

        //Basic info
        width = png_get_image_width(png, info);
        height = png_get_image_height(png, info);
        if (width == 0 || height == 0) {
            success = false;
            goto end;
        }

        if (png_get_color_type(png, info) != PNG_COLOR_TYPE_PALETTE) {
            success = false;
            goto end;
        }
    end:
        if (png != NULL || info != NULL) {
            png_free_data(png, info, PNG_FREE_ALL, -1);
            png_destroy_read_struct(&png, &info, NULL);
        }
        if (file)
            fclose(file);
        return success;
    } else if (ext == "bmp") {
        try {
            char magicNumber[sizeof(bmpMagicNumber)];

            ifstream file(filename.c_str());

            //Read and verify magic number
            file.read(magicNumber, sizeof(magicNumber));
            if (std::memcmp(magicNumber, bmpMagicNumber, sizeof(magicNumber)))
                return false;

            //Read: width, height, pixel order; basic sanity checking
            file.seekg(18);
            int32_t width = 0, height = 0;
            file.read(reinterpret_cast<char*>(&width), 4);
            file.read(reinterpret_cast<char*>(&height), 4);
            if (width == 0 || height == 0)
                return false;

            //More sanity checking
            int32_t temp = 0;
            file.read(reinterpret_cast<char*>(&temp), 2);
            if (temp != 1)
                return false;
            file.read(reinterpret_cast<char*>(&temp), 2);
            if (temp != 8)
                return false;
            file.read(reinterpret_cast<char*>(&temp), 4);
            if (temp != 0)
                return false;
            return true;
        } catch (ifstream::failure &e) {
            return false;
        }
    }

    return false;
}
