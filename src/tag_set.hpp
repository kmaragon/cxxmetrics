#ifndef CXXMETRICS_TAGSET_HPP
#define CXXMETRICS_TAGSET_HPP

#include <unordered_map>

namespace cxxmetrics
{

// TODO use std::variant when C++17 is fully supported in the major compilers
using tag_value = std::string;
using tag_set = std::unordered_map<std::string, tag_value>;

}

#endif //CXXMETRICS_TAGSET_HPP
