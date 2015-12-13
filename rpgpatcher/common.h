#ifndef COMMON
#define COMMON

#include <vector>
#include <string>
#include <map>
#include <istream>
#include <sstream>
#include <iomanip>

class Lcf;

bool isAscii(const std::string &str);
std::string readline(std::istream &stream);

template <typename T>
const T *mapfind(const std::map<std::string, T> &map, const std::string &key)
{
    typename std::map<std::string, T>::const_iterator it = map.find(key);
    if (it == map.end())
        return NULL;
    return &it->second;
}

static inline std::string zeropad(int value)
{
    std::ostringstream ss;
    ss << std::setw(3) << std::setfill('0') << value;
    return ss.str();
}

struct EventText
{
    EventText(int type, int pos, int indent, const std::string &text) :
        type(type),
        pos(pos),
        indent(indent),
        text(text)
    {}

    int type;
    int pos;
    int indent;
    std::string text;
};

std::vector<EventText> parseEventText(Lcf &lcf);

#endif // COMMON

