#ifndef CXXMETRICS_METRIC_PATH_HPP
#define CXXMETRICS_METRIC_PATH_HPP

#include <vector>
#include <string>

namespace cxxmetrics
{

class metric_path;

template<std::size_t Size>
metric_path operator/(const char (&pstr)[Size], const metric_path& other);

class metric_path
{
    std::vector<std::string> paths_;

    metric_path(std::vector<std::string>&& paths) :
            paths_(std::move(paths))
    { }

    template<std::size_t Size>
    friend metric_path operator/(const char (&pstr)[Size], const metric_path& other);

public:
    template<std::size_t Size>
    constexpr metric_path(const char (&pstr)[Size])
    {
        if (Size > 1)
            paths_.emplace_back(std::string(pstr, Size-1));
    }

    metric_path(const char* pstr, std::size_t len = 0)
    {
        if (pstr && *pstr)
        {
            if (len)
                paths_.emplace_back(pstr, len);
            else
                paths_.emplace_back(pstr);
        }
    }

    metric_path(std::string pstr)
    {
        if (!pstr.empty())
            paths_.emplace_back(std::move(pstr));
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

    metric_path operator/(const metric_path& other) const
    {
        if (paths_.empty())
            return other;
        if (other.paths_.empty())
            return *this;

        auto paths = paths_;
        paths.insert(paths.end(), other.paths_.begin(), other.paths_.end());
        return metric_path(std::move(paths));
    }
};

inline std::string metric_path::join(const std::string &delim) const
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

inline bool metric_path::operator==(const cxxmetrics::metric_path &other) const
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

inline bool metric_path::operator!=(const cxxmetrics::metric_path &other) const
{
    return !(operator==(other));
}

template<std::size_t Size>
inline metric_path operator/(const char (&pstr)[Size], const metric_path& other)
{
    std::vector<std::string> paths;
    paths.reserve(other.paths_.size() + 1);

    paths.emplace_back(pstr);
    paths.insert(paths.end(), other.paths_.begin(), other.paths_.end());

    return metric_path(std::move(paths));
}

inline metric_path operator""_m(const char* metric, std::size_t len)
{
    return metric_path(metric, len);
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
