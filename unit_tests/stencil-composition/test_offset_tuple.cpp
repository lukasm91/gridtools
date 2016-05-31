#include "gtest/gtest.h"
#include <stencil-composition/stencil-composition.hpp>
#include <storage/storage.hpp>

using namespace gridtools;

TEST(offset_tuple, test_array_ctr) {
#if !defined(NDEBUG) && defined(CXX11_ENABLED)
    constexpr array<int_t, 4> pos{2,5,8,-6};
    constexpr offset_tuple<4,4> offsets(0, pos);

    GRIDTOOLS_STATIC_ASSERT((static_int<offsets.get<0>() >::value == -6), "Error");
    GRIDTOOLS_STATIC_ASSERT((static_int<offsets.get<1>() >::value == 8), "Error");
    GRIDTOOLS_STATIC_ASSERT((static_int<offsets.get<2>() >::value == 5), "Error");
    GRIDTOOLS_STATIC_ASSERT((static_int<offsets.get<3>() >::value == 2), "Error");

    ASSERT_TRUE(true);
#endif
    array<int_t, 4> pos{2,5,8,-6};
    offset_tuple<4,4> offsets(0, pos);

    ASSERT_TRUE((offsets.get<0>() == -6));
    ASSERT_TRUE((offsets.get<1>() == 8));
    ASSERT_TRUE((offsets.get<2>() == 5));
    ASSERT_TRUE((offsets.get<3>() == 2));
}
