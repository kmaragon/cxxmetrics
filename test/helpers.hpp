//
// Created by keef on 6/16/17.
//

#ifndef CXXMETRICS_HELPERS_HPP
#define CXXMETRICS_HELPERS_HPP

struct mock_clock
{
    mock_clock(unsigned &ref) : value_(ref)
    {
    }

    unsigned operator()() const
    {
        return value_;
    }

private:
    unsigned &value_;
};

#endif //CXXMETRICS_HELPERS_HPP
