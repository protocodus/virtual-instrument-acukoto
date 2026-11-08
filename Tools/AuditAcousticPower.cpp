// Offline per-mode acoustic-power audit. Protocol and units are in
// AcousticPowerBudget.h. This executable alone builds AcukotoEngine.cpp with
// ACUKOTO_RADIATION_DIAGNOSTICS, so it observes the production design grid;
// regular builds contain no hook or patch storage. It changes no synthesis.
//
// Default: both factory instruments, modes 20-1200 Hz, equal-area angular
// quadrature at 16x32 and 32x64 points. --polar sets the finer grid. Report
// free-field sphere and production floor-image upper-hemisphere power, and
// the relative refinement difference. The latter is a convergence indicator,
// not a rigorous error bound; >1% is flagged and should be refined. This is a
// single-mode diagnostic, not a complete multimode radiation-impedance matrix.
// No microphone directivity, output/normalizing gain, room or limiter enters.
//
// A ratio >1 is deliberately NON-GATING: the current 299 Hz mode exceeds its
// whole mechanical loss budget. Do not conceal that existing model gap by
// normalizing the report, capping radiation, or automatically changing Q.
// Exit failure is reserved for invalid inputs/nonfinite or inconsistent data.
//
// Example:
// AcukotoAcousticPower --min-hz 280 --max-hz 560 --polar 64 --csv power.csv --json power.json
#include "AcousticPowerBudget.h"
#include "DSP/AcukotoEngine.h"
#include "DSP/AcukotoPresets.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace
{
struct Options
{
    double sampleRate = 48000.0, minimum = 20.0, maximum = 1200.0;
    int polar = 32, preset = -1;
    std::string csv, json;
} options;
struct Row
{
    acukoto::diagnostics::RadiationMode mode;
    acoustic_power::Power free, floor;
    double freeRefinement, floorRefinement;
};
std::vector<Row> rows;
bool observing = false, valid = true;
int offered = 0;

bool number(const char* text, double& out)
{
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(value))
        return false;
    out = value;
    return true;
}
void csv(std::ostream& out)
{
    out << "instrument,mode,frequency_hz,q,sample_rate_hz,inverse_q_supports,inverse_q_box,"
           "inverse_q_walls,inverse_q_cavity,inverse_q_ji,mechanical_resistance,"
           "free_radiation_resistance,floor_radiation_resistance,free_power_over_loss,"
           "floor_power_over_loss,free_refinement_relative,floor_refinement_relative,"
           "quadrature_flag,budget_flag\n" << std::setprecision(12);
    for (const auto& r : rows)
    {
        out << '"' << r.mode.instrument << "\"," << r.mode.index << ',' << r.mode.frequency
            << ',' << r.mode.q << ',' << r.mode.sampleRate;
        for (double loss : r.mode.inverseQ)
            out << ',' << loss;
        out << ',' << r.free.mechanicalResistance << ',' << r.free.radiationResistance
            << ',' << r.floor.radiationResistance << ',' << r.free.radiationOverLoss
            << ',' << r.floor.radiationOverLoss << ',' << r.freeRefinement << ',' << r.floorRefinement
            << ',' << (std::max(r.freeRefinement, r.floorRefinement) > 0.01 ? "refine" : "within_1pct")
            << ',' << (std::max(r.free.radiationOverLoss, r.floor.radiationOverLoss) > 1.0
                           ? "exceeds_total_loss" : "within_total_loss") << '\n';
    }
}
void json(std::ostream& out)
{
    out << std::setprecision(12)
        << "{\n  \"schema\": 1,\n  \"scope\": \"isolated modes; no microphone, gain, room or limiter\",\n"
           "  \"interpretation\": \"ratio > 1 is a reported model inconsistency, not a numerical-instability claim; no acoustic loss is added\",\n"
           "  \"normalization\": \"mass-normalized modal RMS velocity; its squared units are kg m^2/s^2\",\n"
           "  \"resistance_units\": \"s^-1; multiply by squared modal RMS velocity for watts\",\n"
           "  \"angular_quadrature\": \"equal-area midpoint; relative coarse/fine difference is an indicator, not an error bound\",\n"
        << "  \"polar_steps\": " << options.polar << ",\n  \"azimuth_steps\": " << 2 * options.polar
        << ",\n  \"minimum_hz\": " << options.minimum << ",\n  \"maximum_hz\": " << options.maximum
        << ",\n  \"modes_offered\": " << offered << ",\n  \"modes_measured\": " << rows.size()
        << ",\n  \"modes\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        out << "    {\"instrument\": \"" << r.mode.instrument << "\", \"mode\": " << r.mode.index
            << ", \"frequency_hz\": " << r.mode.frequency << ", \"q\": " << r.mode.q
            << ", \"sample_rate_hz\": " << r.mode.sampleRate
            << ", \"inverse_q\": {\"supports\": " << r.mode.inverseQ[0] << ", \"box\": " << r.mode.inverseQ[1]
            << ", \"walls\": " << r.mode.inverseQ[2] << ", \"cavity\": " << r.mode.inverseQ[3]
            << ", \"ji\": " << r.mode.inverseQ[4] << '}'
            << ", \"mechanical_resistance\": " << r.free.mechanicalResistance
            << ", \"free_radiation_resistance\": " << r.free.radiationResistance
            << ", \"floor_radiation_resistance\": " << r.floor.radiationResistance
            << ", \"free_power_over_loss\": " << r.free.radiationOverLoss
            << ", \"floor_power_over_loss\": " << r.floor.radiationOverLoss
            << ", \"free_refinement_relative\": " << r.freeRefinement
            << ", \"floor_refinement_relative\": " << r.floorRefinement
            << ", \"quadrature_flag\": \"" << (std::max(r.freeRefinement, r.floorRefinement) > 0.01 ? "refine" : "within_1pct")
            << "\", \"budget_flag\": \"" << (std::max(r.free.radiationOverLoss, r.floor.radiationOverLoss) > 1.0
                                                       ? "exceeds_total_loss" : "within_total_loss") << "\"}"
            << (i + 1 == rows.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
}
}

namespace acukoto::diagnostics
{
void observeRadiationMode(const RadiationMode& mode, const RadiationPatch* patches, std::size_t count)
{
    if (!observing)
        return;
    ++offered;
    if (mode.frequency < options.minimum || mode.frequency > options.maximum)
        return;
    const double loss = std::accumulate(mode.inverseQ.begin(), mode.inverseQ.end(), 0.0);
    if (!(mode.q > 0.0) || !std::isfinite(loss) || std::abs(loss * mode.q - 1.0) > 1.0e-10
        || std::any_of(mode.inverseQ.begin(), mode.inverseQ.end(), [] (double x) { return x < 0.0; }))
    {
        valid = false;
        return;
    }
    const auto freeCoarse = acoustic_power::integrate(patches, count, mode.frequency, mode.q, options.polar / 2, false);
    const auto floorCoarse = acoustic_power::integrate(patches, count, mode.frequency, mode.q, options.polar / 2, true);
    const auto free = acoustic_power::integrate(patches, count, mode.frequency, mode.q, options.polar, false);
    const auto floor = acoustic_power::integrate(patches, count, mode.frequency, mode.q, options.polar, true);
    if (!std::isfinite(free.radiationOverLoss) || !std::isfinite(floor.radiationOverLoss))
    {
        valid = false;
        return;
    }
    const auto refinement = [] (double coarse, double fine)
    {
        return std::abs(coarse - fine) / std::max(std::abs(fine), std::numeric_limits<double>::min());
    };
    rows.push_back({ mode, free, floor,
                     refinement(freeCoarse.radiationResistance, free.radiationResistance),
                     refinement(floorCoarse.radiationResistance, floor.radiationResistance) });
}
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--help")
        {
            std::cout << "AcukotoAcousticPower [--preset 0|1] [--rate Hz] [--min-hz Hz] [--max-hz Hz]\n"
                         "                    [--polar even-N] [--csv path] [--json path]\n"
                         "Defaults: both presets, 48 kHz, modes 20-1200 Hz, polar 32.\n"
                         "Reports existing over-budget radiation without failing or changing it.\n";
            return 0;
        }
        if (++i >= argc)
        {
            std::cerr << "Missing value for " << argument << '\n';
            return 2;
        }
        if (argument == "--csv") options.csv = argv[i];
        else if (argument == "--json") options.json = argv[i];
        else
        {
            double value = 0.0;
            if (!number(argv[i], value)) { std::cerr << "Invalid number\n"; return 2; }
            if (argument == "--rate") options.sampleRate = value;
            else if (argument == "--min-hz") options.minimum = value;
            else if (argument == "--max-hz") options.maximum = value;
            else if (argument == "--polar" && value >= 4 && value <= 1024 && std::floor(value) == value)
                options.polar = static_cast<int>(value);
            else if (argument == "--preset" && value >= 0 && value < acukoto::presets.size() && std::floor(value) == value)
                options.preset = static_cast<int>(value);
            else { std::cerr << "Unknown option or invalid range: " << argument << '\n'; return 2; }
        }
    }
    if (options.sampleRate < 22050.0 || options.sampleRate > 192000.0 || options.minimum < 0.0
        || options.maximum < options.minimum || options.polar % 2 != 0
        || (!options.csv.empty() && options.csv == options.json))
    {
        std::cerr << "Invalid range, quadrature grid or duplicate output path\n";
        return 2;
    }
    acukoto::AcukotoEngine engine;
    engine.prepare(options.sampleRate);
    observing = true;
    for (std::size_t i = 0; i < acukoto::presets.size(); ++i)
        if (options.preset < 0 || options.preset == static_cast<int>(i))
            engine.setInstrument(*acukoto::presets[i]);
    observing = false;
    if (!valid || rows.empty())
    {
        std::cerr << "No modes in range or invalid physical diagnostic data\n";
        return 1;
    }
    if (options.csv.empty()) csv(std::cout);
    else
    {
        std::ofstream file(options.csv);
        csv(file);
        if (!file) { std::cerr << "Cannot write CSV\n"; return 2; }
    }
    if (!options.json.empty())
    {
        std::ofstream file(options.json);
        json(file);
        if (!file) { std::cerr << "Cannot write JSON\n"; return 2; }
    }
    const auto over = std::count_if(rows.begin(), rows.end(), [] (const Row& r)
    { return std::max(r.free.radiationOverLoss, r.floor.radiationOverLoss) > 1.0; });
    std::cerr << rows.size() << " modes measured; " << over
              << " exceed prescribed total loss (reported, not corrected).\n";
    return 0;
}
