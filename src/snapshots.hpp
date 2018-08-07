#ifndef CXXMETRICS_SNAPSHOTS_HPP
#define CXXMETRICS_SNAPSHOTS_HPP

#include <unordered_map>
#include "metric_value.hpp"
#include "time.hpp"

namespace cxxmetrics
{

/**
 * \brief a snapshot type that represents only a single value
 */
class value_snapshot
{
    metric_value value_;
public:
    /**
     * \brief constructor
     */
    value_snapshot(const metric_value& value) noexcept;

    /**
     * \brief Get the value in the snapshot
     */
    metric_value value() const;

    /**
     * \brief Comparison operators
     */
    bool operator==(const value_snapshot& other) const noexcept;
    bool operator==(const metric_value& value) const noexcept;
    bool operator!=(const value_snapshot& other) const noexcept;
    bool operator!=(const metric_value& value) const noexcept;
};

inline value_snapshot::value_snapshot(const metric_value &value) noexcept :
        value_(value)
{ }

inline metric_value value_snapshot::value() const {
    return value_;
}

inline bool value_snapshot::operator==(const value_snapshot &other) const noexcept
{
    return value_ == other.value_;
}

inline bool value_snapshot::operator==(const metric_value &value) const noexcept
{
    return value_ == value;
}

inline bool value_snapshot::operator!=(const value_snapshot &other) const noexcept
{
    return value_ == other.value_;
}

inline bool value_snapshot::operator!=(const metric_value &value) const noexcept
{
    return value_ == value;
}

class cumulative_value_snapshot : public value_snapshot
{
public:
    cumulative_value_snapshot(const metric_value& value) noexcept :
            value_snapshot(value)
    { }
};

class average_value_snapshot : public value_snapshot
{
    uint64_t samples_;

public:
    average_value_snapshot(const metric_value& value) noexcept :
            value_snapshot(value),
            samples_(1)
    { }
};

class meter_snapshot
{
    std::unordered_map<std::chrono::steady_clock::duration, metric_value> rates_;
public:
    meter_snapshot(std::unordered_map<std::chrono::steady_clock::duration, metric_value>&& rates) :
            rates_(std::move(rates))
    { }

    auto begin() const noexcept
    {
        return rates_.begin();
    }

    auto end() const noexcept
    {
        return rates_.end();
    }
};

class meter_with_mean_snapshot : public meter_snapshot, public value_snapshot
{
public:
    meter_with_mean_snapshot(const metric_value& mean, std::unordered_map<std::chrono::steady_clock::duration, metric_value>&& rates) :
            meter_snapshot(std::move(rates)),
            value_snapshot(mean)
    { }
};

}

#endif //CXXMETRICS_SNAPSHOTS_HPP
