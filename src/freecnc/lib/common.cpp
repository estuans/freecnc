#include <iostream>
#include <string>
#include <cstdio>
#include <cctype>
#include <boost/algorithm/string.hpp>

#include "../freecnc.h"
#include "inifile.h"

using std::cout;
using boost::to_lower;

int mapscaleq = -1;

namespace p {
    ActionEventQueue* aequeue = 0;
    CnCMap* ccmap = 0;
    UnitAndStructurePool* uspool = 0;
    PlayerPool* ppool = 0;
    WeaponsPool* weappool = 0;
    Dispatcher::Dispatcher* dispatcher = 0;
    map<string, shared_ptr<INIFile> > settings;
}

// We pass by value because we could copy anyway
shared_ptr<INIFile> GetConfig(string name) {
    to_lower(name);
    map<string, shared_ptr<INIFile> >::const_iterator it = p::settings.find(name);
    if (p::settings.end() == it) {
        shared_ptr<File> f = game.vfs.open(name);
        if (!f) {
            throw INIFileNotFound("INIFile: Unable to open" + name);
        }
        shared_ptr<INIFile> ret(new INIFile(f));
        p::settings[name] = ret;
        return ret;
    }
    return it->second;
}

// Client only
namespace pc {
    SoundEngine* sfxeng = 0;
    GraphicsEngine* gfxeng = 0;
    MessagePool* msg = 0;
    std::vector<SHPImage *>* imagepool = 0;
    ImageCache* imgcache = 0;
    Sidebar* sidebar = 0;
    Cursor* cursor = 0;
    Input* input = 0;
}

// Server only
namespace ps {
}

// Server only
std::vector<char*> splitList(char* line, char delim)
{
    std::vector<char*> retval;
    char* tmp;
    unsigned int i,i2;
    tmp = NULL;
    if (line != NULL) {
        tmp = new char[16];
        memset(tmp,0,16);
        for (i=0,i2=0;line[i]!=0x0;++i) {
            if ( (i2>=16) || (tmp != NULL && (line[i] == delim)) ) {
                retval.push_back(tmp);
                tmp = new char[16];
                memset(tmp,0,16);
                i2 = 0;
            } else {
                tmp[i2] = line[i];
                ++i2;
            }
        }
        retval.push_back(tmp);
    }
    return retval;
}

/// change "foo123" to "foo"
char* stripNumbers(const char* src)
{
    char* dest;
    unsigned short i;
    for (i=0;i<strlen(src);++i) {
        if (src[i] <= '9') {
            break;
        }
    }
    dest = new char[i+1];
    strncpy(dest,src,i);
    dest[i] = 0;
    return dest;
}
