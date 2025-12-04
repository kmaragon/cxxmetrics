#ifndef CXXMETRICS_METRIC_SOURCE_HPP
#define CXXMETRICS_METRIC_SOURCE_HPP

namespace cxxmetrics
{

class metric_source
{
public:


    virtual metric_value snapshot() const = 0;



    virtual ~metric_source() = default;
};

}

#endif //CXXMETRICS_METRIC_SOURCE_HPP
