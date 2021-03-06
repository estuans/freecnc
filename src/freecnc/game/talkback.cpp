#include <cmath>

#include "../lib/inifile.h"
#include "../sound/sound_public.h"
#include "unitandstructurepool.h"
#include "talkback.h"

map<string, TalkbackType> Talkback::talktype;
bool Talkback::talktype_init;


Talkback::Talkback()
{
    if (!talktype_init) {
        talktype["report"] = TB_report;
        talktype["ack"] = TB_ack;
        talktype["die"] = TB_die;
        talktype["postkill"] = TB_postkill;
        talktype["attackunit"] = TB_atkun;
        talktype["attackstruct"] = TB_atkst;
        talktype_init = true;
    }
}

void Talkback::load(string talkback, shared_ptr<INIFile> tbini)
{
    unsigned int keynum;
    INIKey key;

    try {
        tbini->readKeyValue(talkback.c_str(), 0);
    } catch(int) {
        game.log << "Could not find talkback \"" << talkback << "\", reverting to default" << endl;
        talkback = "Generic";
    }

    try {
        for (keynum=0;;++keynum) {
            // Still TODO: stringify rest of code
            char* first;
            key=tbini->readKeyValue(talkback.c_str(), keynum);
            first = stripNumbers(key->first.c_str());
            if (strcasecmp(first,"include") == 0) {
                if (strcasecmp(key->second.c_str(),talkback.c_str()) != 0) {
                    merge(p::uspool->getTalkback(key->second.c_str()));
                } else {
                    game.log << "skipping self-referential include in \"" << talkback << "\"" << endl;
                }
            } else {
                if (strcasecmp(first,"delete") == 0) {
                    TalkbackType tbt = getTypeNum(key->second);
                    if (tbt == TB_invalid) {
                        continue;
                    }
                    talkstore[tbt].clear();
                    continue;
                }
                TalkbackType tbt = getTypeNum(first);
                if (tbt == TB_invalid) {
                    continue;
                }
                pc::sfxeng->LoadSound(key->second.c_str());
                talkstore[tbt].push_back(key->second);
            }
            delete[] first;
        }
    } catch(int) {}
}

const char* Talkback::getRandTalk(TalkbackType type)
{
    vector<string>& talk = getTypeVector(type);
    if (talk.empty()) {
        return NULL;
    }
    double sze = static_cast<double>(talk.size());
    int rnd = static_cast<int>(floor((sze*rand()/(RAND_MAX+1.0))));
    return talk[rnd].c_str();
}

vector<string>& Talkback::getTypeVector(TalkbackType type)
{
    if (type == TB_atkst) {
        if (talkstore[TB_atkst].empty()) {
            return talkstore[TB_ack];
        }
    } else if (type == TB_atkun) {
        if (talkstore[TB_atkun].empty()) {
            return talkstore[TB_ack];
        }
    }
    t_talkstore::iterator ret = talkstore.find(type);
    if (ret == talkstore.end()) {
        assert(0 && "Unknown talkback type");
    }
    return ret->second;
}

void Talkback::merge(shared_ptr<Talkback> mergee)
{
    typedef map<string, TalkbackType>::iterator TBI;
    TBI t = talktype.begin();
    TBI end = talktype.end();
    while (t != end) {
        vector<string>& src = mergee->talkstore[t->second];
        vector<string>& dest = talkstore[t->second];
        copy(src.begin(), src.end(), back_inserter(dest));
        ++t;
    }
}

TalkbackType Talkback::getTypeNum(string name)
{
    transform(name.begin(), name.end(), name.begin(), tolower);
    typedef map<string, TalkbackType>::const_iterator TBCI;
    TBCI tbtype = talktype.find(name);
    if (tbtype == talktype.end()) {
        game.log << "Unknown type: \"" << name << "\"" << endl;
        return TB_invalid;
    }
    return tbtype->second;
}
