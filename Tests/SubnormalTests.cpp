// Nothing the instrument computes may meet a subnormal number: a host need
// not set flush-to-zero or denormals-are-zero (Reason documents neither, and
// the Rack adapter sets neither), and without them one subnormal operand
// costs a hundred cycles or more. The engine keeps out of that range by
// sleeping while everything is still far above it, so this is a contract
// on every state that decays: a string stopped by the hand beside one that
// rings on, a long release into sleep, a soft note left to die away, and the
// seconds after a tuning switch.
//
// Checked through the sticky denormal-operand and underflow flags of MXCSR,
// so on x86-64 only: an arm64 core takes no such penalty, and its flags are
// not read here.
#include "DSP/AcukotoPerformer.h"
#include "DSP/AcukotoPresets.h"

#include <cmath>
#include <cstdio>
#include <memory>

#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#define ACUKOTO_HAS_MXCSR 1
#endif

namespace
{
constexpr int rate = 48000;
constexpr int block = 256;
int failures = 0;

void check(bool okay, const char* message, const char* scenario)
{
    if (!okay)
    {
        std::fprintf(stderr, "FAIL %s: %s\n", message, scenario);
        ++failures;
    }
}

// Renders `seconds`, returning the last block's peak.
double render(acukoto::Performer& performer, double seconds)
{
    float left[block], right[block];
    double peak = 0.0;
    for (int done = 0; done < static_cast<int>(seconds * rate); done += block)
    {
        performer.process(left, right, block);
        peak = 0.0;
        for (int i = 0; i < block; ++i)
            peak = std::fmax(peak, std::fmax(std::fabs(left[i]), std::fabs(right[i])));
    }
    return peak;
}

#if defined(ACUKOTO_HAS_MXCSR)
constexpr unsigned denormalOperand = 0x0002u, underflow = 0x0010u;
constexpr unsigned flushToZero = 0x8000u, denormalsAreZero = 0x0040u, stickyFlags = 0x003fu;
void clearFlags() { _mm_setcsr(_mm_getcsr() & ~(flushToZero | denormalsAreZero | stickyFlags)); }
void expectNone(const char* scenario)
{
    const unsigned flags = _mm_getcsr();
    check((flags & denormalOperand) == 0, "an operand was subnormal", scenario);
    check((flags & underflow) == 0, "a result underflowed", scenario);
    clearFlags();
}
#else
void clearFlags() {}
void expectNone(const char*) {}
#endif
}

int main()
{
    auto performer = std::make_unique<acukoto::Performer>();
    performer->prepare(static_cast<double>(rate), 1);
    performer->setInstrument(0);
    performer->setControls(acukoto::Performer::defaultControls(0));
    const auto& spec = *acukoto::presets[0];
    const auto note = [&] (int string) { return spec.strings[static_cast<std::size_t>(string)].midiNote; };

    clearFlags();
    // Every string plucked hard; twelve are stopped by the hand and the
    // lowest rings on beside them.
    for (int string = 0; string < spec.stringCount; ++string)
        performer->noteOn(note(string), 0.85f);
    render(*performer, 0.05);
    for (int string = 1; string < spec.stringCount; ++string)
        performer->noteOff(note(string));
    check(render(*performer, 12.0) > 0.0, "the held string stopped ringing", "stopped strings beside a ringing one");
    expectNone("stopped strings beside a ringing one");

    // The last string stopped too: everything decays until the engine sleeps.
    performer->noteOff(note(0));
    check(render(*performer, 8.0) == 0.0, "the instrument did not come to rest", "release into sleep");
    expectNone("release into sleep");

    // The highest string, softly, left alone: the shortest decay there is.
    performer->noteOn(note(spec.stringCount - 1), 0.2f);
    render(*performer, 10.0);
    performer->noteOff(note(spec.stringCount - 1));
    render(*performer, 4.0);
    expectNone("a soft high note left to die away");

    // The other tuning taken up while this one still rings.
    for (int string = 0; string < spec.stringCount; string += 3)
        performer->noteOn(note(string), 0.9f);
    render(*performer, 0.3);
    performer->setInstrument(1);
    performer->setControls(acukoto::Performer::defaultControls(1));
    render(*performer, 0.2);
    for (int string = 0; string < spec.stringCount; string += 3)
        performer->noteOff(note(string));
    const auto& other = *acukoto::presets[1];
    performer->noteOn(other.strings[4].midiNote, 0.6f);
    render(*performer, 6.0);
    performer->noteOff(other.strings[4].midiNote);
    check(render(*performer, 8.0) == 0.0, "the instrument did not come to rest", "after a tuning switch");
    expectNone("after a tuning switch");

#if defined(ACUKOTO_HAS_MXCSR)
    std::printf("subnormals: no subnormal operand and no underflow in 49 s of playing with flush-to-zero off\n");
#else
    std::printf("subnormals: rendered; the flags are read on x86-64 only\n");
#endif
    return failures == 0 ? 0 : 1;
}
