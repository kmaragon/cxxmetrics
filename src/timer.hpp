#ifndef CXXMETRICS_TIMER_HPP
#define CXXMETRICS_TIMER_HPP

#include "histogram.hpp"
#include "meter.hpp"
#include "uniform_reservoir.hpp"

namespace cxxmetrics
{

template<typename TClock = std::chrono::system_clock, typename TReservoir = uniform_reservoir<typename TClock::duration, 1024>, period::value... TWindows>
class timer
{
    histogram<typename TClock::duration, TReservoir> histogram_;
    meter_with_mean<TWindows...> meter_;
    TClock clock_;

public:
    timer() = default;

    timer(TReservoir &&reservoir, TClock &&clock) :
            histogram_(std::forward<TReservoir>(reservoir)),
            clock_(std::forward<TReservoir>(clock)) {}

    timer(TReservoir &&reservoir) :
            histogram_(std::forward<TReservoir>(reservoir)) {}

    double mean() const noexcept override
    {
        return meter_.mean();
    }

    auto rates_collection() const noexcept override
    {
        return meter_.rates_collection();
    }

    template<period::value TAt>
    auto rate() const
    {
        return meter_.template rate<TAt>();
    }

    uint64_t count() const noexcept
    {
        return histogram_.count();
    }

    void update(const typename TClock::duration& duration) noexcept
    {
        if (duration)
        {
            histogram_.update(duration);
            meter_.mark();
        }
    }

    template<typename TRunnable, bool TIncludeExceptions = false>
    auto time(const TRunnable& runnable)
    {
        auto start_time = clock_.now();
        if (TIncludeExceptions)
        {
            try
            {
                runnable();
                update(clock_.now() - start_time);
            }
            catch (...)
            {
                update(clock_.now() - start_time);
                throw;
            }
        }
        else
        {
            runnable();
            update(clock_.now() - start_time);
        }
    }
};

}
#endif //CXXMETRICS_TIMER_HPP
