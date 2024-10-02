#include <catch2/catch_all.hpp>
#include <execinfo.h>
#include <cxxmetrics/pool.hpp>

using namespace cxxmetrics::internal;
using namespace std;

TEST_CASE("Pool allocates and frees objects")
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
