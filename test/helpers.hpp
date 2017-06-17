//
// Created by keef on 6/16/17.
//

#ifndef CXXMETRICS_HELPERS_HPP
#define CXXMETRICS_HELPERS_HPP

struct mock_clock
{
    mock_clock(int &ref) : value_(ref)
    {
    }

    int operator()() const
    {
        return value_;
    }

private:
    int &value_;
};

#endif //CXXMETRICS_HELPERS_HPP
