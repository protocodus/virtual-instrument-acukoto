#pragma once

#include <cmath>

namespace acukoto::detail
{
// Static displacement y/(F/T) of a fixed-end string under a Gaussian
// contact force of total F. Integrating -T y'' = F g(x-p) twice gives
// G(0) + x/L [G(L)-G(0)] - G(x), with G''=g. The denominator keeps the
// force on the string equal to F when a wide contact reaches an end.
// Unlike rounding only the triangle's kink, both endpoints stay fixed.
class PluckShape
{
public:
    PluckShape(double length, double point, double width, bool harmonic = false) noexcept
        : length_(length), point_(point), width_(width), harmonic_(harmonic)
    {
        if (width_ > 0.0)
        {
            atLeft_ = primitive(0.0);
            atRight_ = primitive(length_);
            forceShare_ = cdf((length_ - point_) / width_) - cdf(-point_ / width_);
        }
    }

    double operator()(double x) const noexcept
    {
        if (x <= 0.0 || x >= length_)
            return 0.0;
        const double held = displacement(x);
        // A midpoint touch removes modes with an antinode there (odd n).
        // sin(n pi (L-x)/L) = (-1)^(n+1) sin(n pi x/L), so this spatial
        // projection keeps even modes before sample-cell integration. A
        // half-period shift in a ceil(period)-sample scratch buffer is not
        // that projection when the period is fractional.
        return harmonic_ ? 0.5 * (held - displacement(length_ - x)) : held;
    }

private:
    static constexpr double farField = 8.5;
    // Past farField it is 0 or 1 to the last place of anything it is added
    // to here, and erfc is not asked for a value that underflows.
    static double cdf(double u) noexcept
    {
        if (u >= farField)
            return 1.0;
        if (u <= -farField)
            return 0.0;
        return 0.5 * std::erfc(-u / std::sqrt(2.0));
    }
    // Past 8.5 widths from the contact G is its asymptote: to the right the
    // Gaussian term is under a twentieth of u's last place and the erfc
    // within 2e-17 of 2, so the sum below is exactly u; to the left G is
    // under 1.2e-18 of the width, beneath the last place of the chord it is
    // taken from wherever the string is more than 0.004 widths from its
    // end. So only the few cells around the contact call erfc and exp, not
    // every cell of the period at each note-on (they are library calls in
    // Reason's build), and no far cell's exp underflows.
    double primitive(double x) const noexcept
    {
        constexpr double twoPi = 6.283185307179586476925286766559;
        const double u = (x - point_) / width_;
        if (u >= farField)
            return width_ * u;
        if (u <= -farField)
            return 0.0;
        return width_ * (u * cdf(u) + std::exp(-0.5 * u * u) / std::sqrt(twoPi));
    }
    double displacement(double x) const noexcept
    {
        if (width_ <= 0.0)
            return x <= point_ ? x * (length_ - point_) / length_
                               : point_ * (length_ - x) / length_;
        return (atLeft_ + x * (atRight_ - atLeft_) / length_ - primitive(x)) / forceShare_;
    }

    double length_, point_, width_;
    bool harmonic_;
    double atLeft_ { 0.0 }, atRight_ { 0.0 }, forceShare_ { 1.0 };
};
} // namespace acukoto::detail
