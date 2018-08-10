#ifndef CXXMETRICS_TAG_COLLECTION_HPP
#define CXXMETRICS_TAG_COLLECTION_HPP

#include <unordered_map>
#include "metric_value.hpp"

namespace cxxmetrics
{

class tag_collection
{
    std::unordered_map<std::string, metric_value> tags_;
public:
    tag_collection() = default;
    tag_collection(const tag_collection&) = default;

    template<typename TPairCollection>
    tag_collection(const TPairCollection& pairs, int size = 0) : tags_(pairs.begin(), pairs.end(), size)
    { }

    tag_collection(std::initializer_list<typename std::unordered_map<std::string, metric_value>::value_type> tags) :
            tags_(tags)
    { }

    auto begin() const
    {
        return tags_.begin();
    }

    auto end() const
    {
        return tags_.end();
    }

    bool operator==(const tag_collection& other) const;
    bool operator!=(const tag_collection& other) const;
};

inline bool tag_collection::operator==(const cxxmetrics::tag_collection &other) const
{
    if (tags_.size() != other.tags_.size())
        return false;

    for (const auto& p : tags_)
    {
        auto f = other.tags_.find(p.first);
        if (f == other.tags_.end() || f->second != p.second)
            return false;
    }

    return true;
}

inline bool tag_collection::operator!=(const cxxmetrics::tag_collection &other) const
{
    return !operator==(other);
}

}

namespace std
{

template<>
struct hash<cxxmetrics::tag_collection>
{
    std::size_t operator()(const cxxmetrics::tag_collection& collection) const
    {
        std::size_t result = 0;
        std::hash<std::string> h;
        std::hash<cxxmetrics::metric_value> v;
        for (const auto& p : collection)
            result = (result * 397) ^ (h(p.first) ^ v(p.second));

        return result;
    }
};

}

#endif //CXXMETRICS_TAG_COLLECTION_HPP
