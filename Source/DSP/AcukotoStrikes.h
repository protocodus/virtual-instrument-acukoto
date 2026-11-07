// What the twelve keys under the lowest string do: a hand strikes the koto's
// body (AcukotoEngine::strike). No koto has those strings, and players hit the
// body instead (Pornprasit 2010, section 2.9). Each key is one hit: a hand, a
// surface of the body and a place along it, and the key's velocity is the
// speed the hand arrives at. The keys further below play techniques
// (AcukotoEngine.cpp, TechniqueKey).
//
// Every figure is an estimate, not a measurement. The three hands are three
// defensible ones that the owner chose by ear (Docs/decisions.md, 2026-11-03).
// The palm is a hand and the forearm behind it; its Hertzian contact
// F = k d^1.5 lasts about 7 ms for a hand arriving at 1 m/s, with a peak force
// near 150 N. The knuckle has a third of its mass and four times its stiffness
// (a contact about a third as long), the rap a sixth of the mass and eighteen
// times the stiffness (about a sixth). The keys ascend from the palm's deep
// thumps to the rap's sharp knocks.
//
// The places run from 20% to 80% of the body from the head, where it answers
// best, and no hand strikes one surface at two places that mirror each other
// about the middle, where the body answers alike. `touch` is how hard the
// player's hand arrives there next to the others: it scales the arrival speed
// so that velocity 0.7 stands near a plucked note's level on every key, as the
// strings' touches do. It changes only through AcukotoLevels
// (Tools/MeasureLevels.cpp).

#pragma once

#include <array>

namespace acukoto
{

struct StrikeHand
{
    double mass;       // kg
    double stiffness;  // N/m^1.5
};

inline constexpr std::array<StrikeHand, 3> strikeHands {{
    { 0.3, 1.36e6 },   // palm
    { 0.1, 5.9e6 },    // knuckle
    { 0.05, 2.4e7 },   // rap
}};

// The top is the plate's far edge beside the first string, the near side is
// the wall by the player, the back is its middle.
enum class StrikeSurface
{
    Top,
    NearSide,
    Back
};

struct StrikeKey
{
    int hand;                // index into strikeHands
    StrikeSurface surface;
    double along;            // fraction of the body's length from the head
    double touch;
};

// The lowest string's note less twelve first: D2 to C#3 under a koto whose
// lowest string is D3.
inline constexpr std::array<StrikeKey, 12> strikeKeys {{
    { 0, StrikeSurface::Top, 0.28, 1.155 },
    { 0, StrikeSurface::Top, 0.62, 0.928 },
    { 0, StrikeSurface::NearSide, 0.45, 1.820 },
    { 0, StrikeSurface::Back, 0.35, 0.758 },
    { 1, StrikeSurface::Top, 0.22, 1.263 },
    { 1, StrikeSurface::Top, 0.55, 1.019 },
    { 1, StrikeSurface::NearSide, 0.33, 0.791 },
    { 1, StrikeSurface::Back, 0.58, 0.510 },
    { 2, StrikeSurface::Top, 0.40, 0.858 },
    { 2, StrikeSurface::Top, 0.76, 0.889 },
    { 2, StrikeSurface::NearSide, 0.72, 0.911 },
    { 2, StrikeSurface::Back, 0.22, 0.331 },
}};

} // namespace acukoto
