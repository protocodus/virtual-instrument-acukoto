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
    static double cdf(double u) noexcept { return 0.5 * std::erfc(-u / std::sqrt(2.0)); }
    double primitive(double x) const noexcept
    {
        constexpr double twoPi = 6.283185307179586476925286766559;
        const double u = (x - point_) / width_;
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
