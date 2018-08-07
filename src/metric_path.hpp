#ifndef CXXMETRICS_METRIC_PATH_HPP
#define CXXMETRICS_METRIC_PATH_HPP

#include <vector>
#include <string>

namespace cxxmetrics
{

class metric_path
{
    std::vector<std::string> paths_;
public:
    template<std::size_t Size>
    constexpr metric_path(const char (&pstr)[Size]) {

    }

    std::string join(const std::string& delim) const;

    auto begin() const
    {
        return paths_.begin();
    }

    auto end() const
    {
        return paths_.end();
    }

    bool operator==(const metric_path& other) const;
    bool operator!=(const metric_path& other) const;
};

std::string metric_path::join(const std::string &delim) const
{
    if (paths_.empty())
        return "";

    // probably faster to calculate the size and then build the result
    std::string result;
    std::size_t len = 0;
    for (auto& str : paths_)
        len += str.length() + delim.length();
    len -= delim.length(); // cut the last one

    result.reserve(len + 1);
    result += paths_[0];
    result += delim;
    for (std::size_t i = 1; i < paths_.size(); ++i)
    {
        result += paths_[i];
        result += delim;
    }

    return result;
}

bool metric_path::operator==(const cxxmetrics::metric_path &other) const
{
    if (paths_.size() != other.paths_.size())
        return false;

    for (std::size_t i = 0; i < paths_.size(); ++i)
    {
        if (paths_[i] != other.paths_[i])
            return false;
    }

    return true;
}

bool metric_path::operator!=(const cxxmetrics::metric_path &other) const
{
    return !(operator==(other));
}

}

namespace std
{
    template<>
    struct hash<cxxmetrics::metric_path>
    {
        std::size_t operator()(const cxxmetrics::metric_path& path) const
        {
            std::size_t result = 0;
            std::hash<string> h;
            for (const auto& str : path)
                result = (result * 397) ^ h(str);

            return result;
        }
    };
}

#endif //CXXMETRICS_METRIC_PATH_HPP
