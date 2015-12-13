#include "common.h"

#include <stdexcept>
#include <cstring>

#include "util.h"

#include "lcf.h"

bool isAscii(const std::string &str)
{
    for (size_t i = 0; i < str.size(); ++i)
        if ((unsigned char)str[i] > 127)
            return false;
    return true;
}

std::string readline(std::istream &stream)
{
    for (;;) {
        std::string line;
        std::getline(stream, line);
        if (stream.eof())
            return std::string();
        if (!line.empty() && line[0] != '#')
            return line;
    }
}

std::vector<EventText> parseEventText(Lcf &lcf)
{
    //vgperson says there are stray message continuations sometimes;
    //make sure we have already begun reading a message before adding nth lines
    bool readingMsg = false;

    std::vector<EventText> messages;

    int pos = 0;

    while (lcf.bytesRemain()) {
        //Get event info
        int code = lcf.readInt();
        if (code == 0)
            continue;
        int indent = lcf.readInt();
        std::string string = lcf.readString();
        //dispose of args
        int argc = lcf.readInt();
        for (int i = 0; i < argc; ++i)
            lcf.readInt();
        ++pos;

        //Parse event!
        switch (code) {
        case 10110: //begin message
            readingMsg = true;
            messages.push_back(EventText(0, pos, indent, string + "\n"));
            break;
        case 20110: //continue message
            if (readingMsg) {
                messages.back().text.append(string);
                messages.back().text.append("\n");
            }
            break;
        case 10140: //show choice begin
            messages.push_back(EventText(1, pos, indent, std::string()));
            break;
        case 20140: //show choice option
            if (!string.empty()) {
                //find first message with same indent
                for (int i = (int)messages.size() - 1; i >= 0; --i) {
                    if (messages[i].type == 1 && messages[i].indent == indent) {
                        messages[i].text.append(string);
                        messages[i].text.append("\n");
                        break;
                    }
                }
            }
            break;
        case 20141: //show choice end
            //do nothing
            break;
        case 10610: //change hero name
            messages.push_back(EventText(2, pos, indent, string + "\n"));
            break;
        }

        //Reset readingMsg if this isn't a message code
        if (code != 10110 && code != 20110)
            readingMsg = false;
    }
    return messages;
}


