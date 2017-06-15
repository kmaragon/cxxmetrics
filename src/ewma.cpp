#include "ewma.hpp"

using namespace cxxmetrics;
using namespace std::chrono;

ewma::ewma(steady_clock::duration window, steady_clock::duration interval) noexcept :
    ewma_(window, interval)
{
}

void ewma::mark(int64_t value) noexcept
{
    ewma_.mark(value);
}

double ewma::rate() const noexcept
{
    return ewma_.rate();
}

double ewma::rate() noexcept
{
    return ewma_.rate();
}

bool ewma::compare_exchange(double expectedrate, double rate)
{
    return ewma_.compare_exchange(expectedrate, rate);
}