#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

#include <string>
#include <vector>

class Bitmap
{
public:
    /* constructors and destructors */
    Bitmap();
    Bitmap(const std::string &filename);
    Bitmap(unsigned int width, unsigned int height);

    //Accessors
    unsigned int getWidth() const { return width; }
    unsigned int getHeight() const { return height; }
    const std::vector<uint8_t> &getPixels() const { return pixels; }
    const std::vector<uint8_t> &getPalette() const { return palette; }

    //Blit!
    void blit(int mX, int mY, const Bitmap &other, int oX, int oY, int oW, int oH);

   	//Read from file
    void readFromXyz(const std::string &filename);
    void readFromPng(const std::string &filename);
    void readFromBmp(const std::string &filename);

    //Write to file
    void writeToPng(const std::string &filename, bool transparent, const std::vector<uint8_t> &palette) const;
    void writeToXyz(const std::string &filename, const std::vector<uint8_t> &palette) const;
    void writeToPng(const std::string &filename, bool transparent) const { writeToPng(filename, transparent, palette); }
    void writeToXyz(const std::string &filename) const { writeToXyz(filename, palette); }

    bool empty() const { return width == 0 || height == 0; }

    static bool isIndexed(const std::string &filename);

private:
    unsigned int width, height;
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> palette;
};

#endif // BITMAP_H

