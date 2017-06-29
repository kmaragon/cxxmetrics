#include <gtest/gtest.h>
#include <execinfo.h>
#include <pool.hpp>

using namespace cxxmetrics::internal;
using namespace std;

TEST(pool_tests, allocates_and_frees_objects)
{
    pool<string> p;

    auto sptr1 = p.allocate();
    *sptr1 = "This is a test";

    string *sp1addr = &(*sptr1);

    auto sptr2 = p.allocate();
    *sptr2 = "Another test";

    sptr1 = nullptr;

    auto sptr3 = p.allocate();
    *sptr3 = "Last";

    ASSERT_EQ(&(*sptr3), sp1addr);
}
