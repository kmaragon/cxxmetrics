#pragma once

#include "bits/time.hpp"

namespace cxxmetrics_literals {

constexpr cxxmetrics::period operator""_micro(cxxmetrics::period::value v) {
  return cxxmetrics::time::microseconds(v);
}

constexpr cxxmetrics::period operator""_msec(cxxmetrics::period::value v) {
  return cxxmetrics::time::milliseconds(v);
}

constexpr cxxmetrics::period operator""_sec(cxxmetrics::period::value v) {
  return cxxmetrics::time::seconds(v);
}

constexpr cxxmetrics::period operator""_min(cxxmetrics::period::value v) {
  return cxxmetrics::time::minutes(v);
}

constexpr cxxmetrics::period operator""_hour(cxxmetrics::period::value v) {
  return cxxmetrics::time::hours(v);
}

} // namespace cxxmetrics_literals
