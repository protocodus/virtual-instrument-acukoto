#pragma once

#include <array>
#include <cstddef>

// Setup-only observation hook for Tools/AuditAcousticPower.cpp. The regular
// DSP build does not define ACUKOTO_RADIATION_DIAGNOSTICS and neither stores
// these patches nor calls the observer. This is not an audio-thread API.
namespace acukoto::diagnostics
{
struct RadiationPatch
{
    double x, y, z;       // m, the actual source coordinates used by designBody
    double imageZ;        // m, that source's production rigid-floor image
    double area;          // m^2; a hole uses 1 and carries volume in its shape
    double displacement;  // surface shape per mass-normalized modal coordinate
};

struct RadiationMode
{
    const char* instrument;
    int index;
    double sampleRate;
    double frequency;
    double q;
    // Additive inverse-Q contributions in this order. These are the existing
    // model's total prescribed losses, not a measured partition of radiation.
    // They sum to 1/q; none is an extra acoustic-radiation-resistance term.
    std::array<double, 5> inverseQ; // supports, box, walls, cavity, ji
};

// Implemented by the diagnostic executable; pointers are valid only during
// the call. The engine passes the exact shapes before microphone directivity,
// finite-distance weighting, output gain, room and limiting.
void observeRadiationMode(const RadiationMode&, const RadiationPatch*, std::size_t);
} // namespace acukoto::diagnostics
