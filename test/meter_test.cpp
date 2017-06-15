#include <gtest/gtest.h>
#include <meter.hpp>
#include <thread>
#include <ctti/type_id.hpp>

using namespace std;
using namespace std::chrono_literals;
using namespace cxxmetrics;

TEST(meter_test, meter_test_type)
{
    using sorted = templates::static_sorter<templates::duration_collection<12, 1, 44, 3, 99, 57, 11, 101, rate_period::seconds<15>, 17, 12, 97, 1>>::type;
    using s = templates::static_uniq<sorted>::type;
    cout << ctti::type_id<s>().name().cppstring() << endl;
}
