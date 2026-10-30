// Acukoto: an experimental physically modelled koto, forked from Aculyre
// 0.6.0's lyre engine.
//
// Every string is open (one pitch per string, no frets). Each is two pairs
// of stiff-string waveguides, one per polarisation: the speaking length from
// the fixed bridge (ryukaku), taken as rigid, to its own movable bridge (ji)
// standing on the top, and the rest of the string from the ji to the far
// fixed bridge (unkaku). Each ji stands on its feet on springs: it heaves on
// the top beneath it (the string's motion normal to the top) and rocks
// across it (the motion parallel to it), so every string drives the body
// under its own ji and every string, played or idle, is coupled to every
// other through it. The body is built as the koto is: a hollow paulownia box
// that bends and twists as a whole, a top and a back with modes of their own
// between its walls, and the air in its long cavity breathing through two
// holes in the back; all of it radiates into a small room.
//
// The left hand's press behind a ji (oshide) raises a string's tension and
// so its pitch: press() sets it per string, in semitones, over a glide.
//
// This is a proof of concept, not a product: setup calls allocate nothing
// but are not real-time safe; process(), the note calls and setControls()
// are.

#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace acukoto
{

inline constexpr int maxStrings = 13;
inline constexpr int maxModes = 400;

struct StringMaterial
{
    const char* name;
    double density;       // kg/m^3
    // Young's modulus at the string's stress sigma (Pa), E' = youngModulus
    // + modulusPerStress * sigma: polymer strings stiffen under tension.
    double youngModulus;  // Pa
    double modulusPerStress;
    // Loss angle constant over the audio band: dislocation in steel
    // (constantLossSection).
    double constantLoss;
    // Loss modulus E'' of the bending stiffness, EI(1 + i E''/E')
    // (bendingLossSection).
    double lossModulus;   // Pa
    // Air damping by Woodhouse and Lynch-Aird's fit to measured polymer and
    // gut strings rather than Valette's law.
    bool measuredAirDamping;
    // A wound string's dry friction between its turns, a constant loss angle
    // on its tension, and its bending loss factor in place of the plain
    // wire's E''/E' (AcukotoEngine::setInstrument).
    double windingFriction { 0.0 };
    double woundBendingLoss { 0.0 };
};

struct StringSpec
{
    int midiNote;
    double length;    // speaking length, ryukaku to the ji, m (the rest runs on to the unkaku)
    double diameter;  // m
    // The player's force on this string as a share of the instrument's
    // pluckForce: an even touch by the microphones and by the piezo
    // (AcukotoPresets.h).
    double touch { 1.0 };
    double piezoTouch { 1.0 };
    // A wound string: its maker's unit weight (kg/m), and the core diameter
    // (m) that sets its bending stiffness and carries its tension, the wrap
    // adding mass but almost no stiffness. Zero for plain wire.
    double woundMass { 0.0 };
    double coreDiameter { 0.0 };
    // How the player tunes it by ear: cents from the equal-tempered note,
    // and one player's scatter about that (a standard deviation). Each
    // instrument draws its own scatter once, from its name.
    double cents { 0.0 };
    double centsSpread { 0.0 };
};

// An orthotropic wood: L along the grain, R radial, T tangential to the
// growth rings.
struct WoodSpec
{
    double density;  // kg/m^3
    double eL, eR, eT;  // Young's moduli, Pa
    double gLR, gLT;    // shear moduli, Pa
    double nuLT;        // Poisson's ratio
};

// A hole in the back, as the round hole of the same area.
struct SoundHole
{
    double position;  // its centre's distance from the head end, m
    double radius;    // m
};

// The body as a koto is built, x along it from the head (the ryukaku's end):
// a box whose top is the arched outside of the log, grain along the length,
// on two side walls, closed by a back board with its grain across and two
// holes, solid at both ends.
struct BodySpec
{
    double length;        // head to tail, m
    double headWidth;     // m
    double tailWidth;     // m
    double endDepth;      // back to the top's crown, at the ends, m
    double middleDepth;   // and halfway along, m
    double crown;         // the top's rise across its width, m
    double topThickness;  // m
    double sideThickness; // m
    double backThickness; // m
    WoodSpec wood;
    double cavityStart;   // the hollow between the solid ends, from the head, m
    double cavityEnd;     // m
    double headMass;      // each solid end with what is fitted to it, kg
    double tailMass;
    std::array<SoundHole, 2> holes;
    double ryukaku;       // the ryukaku's distance from the head end, m
    double stringSpan;    // ryukaku to unkaku, m
    double stringSpacing; // between neighbouring strings across the top, m
    double bridgeHeight;  // a ji's height, the string's above the top, m
    double bridgeFeet;    // the spread of its two feet across the top, m
    double bridgeMass;    // one ji, kg
    // A ji's resonances on its feet with the body held still: heaving on
    // them, and rocking across the top (at the string a third of its mass).
    double bridgeHeaveHz;
    double bridgeRockHz;
    // The back's height over the floor the koto is played on, at the head
    // (raised on its two legs) and at the tail (resting on the floor), m.
    double headClearance;
    double tailClearance;
};

enum class Technique
{
    Finger,
    Nail,
    Plectrum
};

struct InstrumentSpec
{
    const char* name;
    StringMaterial material;
    int stringCount;
    std::array<StringSpec, maxStrings> strings;
    BodySpec body;
    // How the instrument is played by default; setInstrument() starts its
    // PlayingControls from these.
    Technique technique;
    double pluckDistance;  // m from the ryukaku along the string
    double normalShare;    // share of the pluck's energy normal to the top
    double pluckForce;     // N held before release at full velocity
    double room;           // room send, 0-1
    double outputGain;     // digital full scale per pascal at the microphones
    double piezoGain;      // digital full scale per newton on the piezo
};

// What a player and the room change while the instrument sounds. Every
// field takes effect in real time: the plucking fields at the next pluck,
// the rest at once.
struct PlayingControls
{
    Technique technique { Technique::Finger };
    // Where the tsume meets the string, m from the ryukaku: a koto player's
    // hand plays at one place by the ryukaku, whatever the string's length.
    double pluckDistance { 0.05 };
    double normalShare { 0.25 };    // share of the pluck's energy normal to the top
    // How a string sounds on after its key is let go: 0, the playing hand
    // stops it (an amplitude time constant of 30 ms); 1, it rings until it
    // dies away by itself.
    double release { 0.0 };
    // A palm resting across every string, 0-1.
    double mute { 0.0 };
    // 1, strings nobody has plucked ring in sympathy; 0, a resting hand
    // keeps them still.
    double sympathy { 1.0 };
    double room { 0.35 };       // room send, 0-1
    double width { 1.0 };       // stereo width, 0 (mono) to 2
    double volume { 1.0 };      // gain on the instrument's outputGain
    double pitchBend { 0.0 };   // semitones, on every string
    // Oshide follows the notes played: a note one or two semitones above a
    // string's own, which no string is tuned to, presses the nearest string
    // below up to it before the pluck, and its key-up lets the press go.
    bool oshide { true };
};

class AcukotoEngine
{
public:
    AcukotoEngine();
    ~AcukotoEngine();

    // Setup: designs the strings, the body and the room. Not real-time.
    void prepare(double sampleRate);
    void setInstrument(const InstrumentSpec& instrument);
    void reset() noexcept;

    // The string tuned to midiNote plays (with oshide, also a string one or
    // two semitones below, pressed up to it); a note no string can sound is
    // ignored (a koto has no frets). With oshide, a note played legato that
    // a held, sounding string reaches by its press alone moves the press
    // without a pluck (ato-oshi, or letting the press go).
    void noteOn(int midiNote, float velocity) noexcept;
    // The playing hand damps the string (PlayingControls::release), unless
    // sustain holds it.
    void noteOff(int midiNote) noexcept;
    void setSustain(bool down) noexcept;
    // The left hand presses string (0-12) behind its ji, raising it by
    // semitones (0-3), reached over `seconds` (an exponential glide of that
    // time constant). Pressing a sounding string bends its note (ato-oshi).
    void press(int string, double semitones, double seconds) noexcept;
    // Plucks a string by its number, whatever its note.
    void pluckString(int string, float velocity) noexcept;
    void setControls(const PlayingControls& controls) noexcept;
    [[nodiscard]] const PlayingControls& controls() const noexcept { return controls_; }
    // Ablation for listening: the hard pluck's pitch glide.
    void setTensionModulation(bool enabled) noexcept { tensionModulation_ = enabled; }

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] const InstrumentSpec& instrument() const noexcept;
    // The string whose own (unpressed) note is midiNote, or -1.
    [[nodiscard]] int stringForNote(int midiNote) const noexcept;
    // The string's smoothed mean squared wave velocity, (m/s)^2: an observer.
    [[nodiscard]] double stringEnergy(int string) const noexcept;
    struct StringActivity
    {
        double energy;  // as stringEnergy()
        bool held;      // its key is down or sustain holds it
        bool plucked;   // played, and still sounding from that pluck
        bool raised;    // the left hand presses it
    };
    [[nodiscard]] StringActivity stringActivity(int string) const noexcept;
    // Asleep: everything has died away below -170 dBFS, so process() writes
    // silence without running the strings, the body or the room until the
    // next pluck.
    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] int stringCount() const noexcept { return stringCount_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    struct StringReport
    {
        int midiNote;
        double frequency;    // designed open pitch, Hz
        double tension;      // N
        double impedance;    // kg/s
        double inharmonicity;
        double stress;       // Pa
        double intrinsicT60; // s, fundamental, string losses alone
        double dispersion;   // first-order allpass coefficient
        int dispersionStages;
        int dispersionPairs;    // second-order sections fitted past the 12th partial
        double dispersionCents; // worst partial below 5 kHz (to H12 at most where
                                // first-order sections alone fit) against the stiff-string law
    };
    [[nodiscard]] StringReport stringReport(int string) const noexcept;

    // The last pluck as drawn around the playing controls.
    struct PluckReport
    {
        double position;     // share of the speaking length from the ji
        double force;        // N, held before the release
        double normalShare;  // share of the pluck's energy normal to the top
        double release;      // s, the held force's unloading time
    };
    [[nodiscard]] PluckReport lastPluck() const noexcept;

    struct ModeReport
    {
        double frequency;
        double q;
        double heave;       // the middle string's ji's participation, unit modal mass
        double monopole;    // net volume out of the body per unit modal displacement, m^3
        double airShare;    // kinetic energy share of the air in the cavity and holes
        double bodyShare;   // of the box bending and twisting as a whole
        double wallShare;   // of the walls' own modes (the box's section)
        double bridgeShare; // of the ji on their feet
    };
    [[nodiscard]] int modeCount() const noexcept;
    [[nodiscard]] ModeReport modeReport(int mode) const noexcept;
    // Driving-point mobility of a string's ji normal to the top, m/s/N, the
    // other strings held still.
    [[nodiscard]] double bridgeMobilityMagnitude(double frequency, int string) const noexcept;

private:
    struct State;
    void applyPitch(int string, bool raised) noexcept;
    void pluck(int index, int midiNote, float velocity) noexcept;
    void updateDamping(int string) noexcept;
    void sleep() noexcept;

    std::unique_ptr<State> state_;
    PlayingControls controls_ {};
    double sampleRate_ { 48000.0 };
    int stringCount_ { 0 };
    bool tensionModulation_ { true };
};

} // namespace acukoto
