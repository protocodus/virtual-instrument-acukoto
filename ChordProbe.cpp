// Plays key k with key k+d together (d=1,2,3,4) and reports which strings end up plucked/raised.
#include "AcukotoEngine.h"
#include "AcukotoPerformer.h"
#include "AcukotoPresets.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
int main(int argc, char** argv)
{
    int preset = std::atoi(argv[1]); int d = std::atoi(argv[2]);
    const int rate = 48000;
    auto engine = std::make_unique<acukoto::AcukotoEngine>();
    engine->prepare(rate); engine->setInstrument(*acukoto::presets[(std::size_t)preset]);
    auto c = acukoto::Performer::defaultControls(preset); c.room = 0; c.sympathy = 0;
    std::vector<float> l(rate/2), r(rate/2);
    for (int key = 50; key + d <= 108; ++key) {
        engine->reset(); engine->setControls(c);
        engine->process(l.data(), r.data(), 480);
        engine->noteOn(key, 0.7f); engine->noteOn(key + d, 0.7f);
        engine->process(l.data(), r.data(), rate/4);
        int plucked = 0; std::string which;
        for (int s = 0; s < engine->stringCount(); ++s) { auto a = engine->stringActivity(s); if (a.plucked) { ++plucked; which += std::to_string(s) + (a.raised ? "^" : "") + " "; } }
        std::printf("%d+%d: %d strings [%s]\n", key, key + d, plucked, which.c_str());
    }
}
