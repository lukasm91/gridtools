#include "gtest/gtest.h"
#include <stencil-composition/make_computation.hpp>

TEST(storage_info, test_interface) {
    typedef gridtools::layout_map<0,1,2,3,4> layout_t;
#ifdef CXX11_ENABLED
    constexpr gridtools::meta_storage_base<0,layout_t,false> meta_{11, 12, 13, 14, 15};
    GRIDTOOLS_STATIC_ASSERT((meta_.dims<0>()==11), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.dims<1>()==12), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.dims<2>()==13), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.dims<3>()==14), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.dims<4>()==15), "error");

    GRIDTOOLS_STATIC_ASSERT((meta_.strides(4)==15), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides(3)==15*14), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides(2)==15*14*13), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides(1)==15*14*13*12), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides(0)==15*14*13*12*11), "error");

    GRIDTOOLS_STATIC_ASSERT((meta_.strides<4>()==1), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides<3>()==15), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides<2>()==15*14), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides<1>()==15*14*13), "error");
    GRIDTOOLS_STATIC_ASSERT((meta_.strides<0>()==15*14*13*12), "error");
#else
    gridtools::meta_storage_base<0,layout_t,false> meta_(11, 12, 13, 14, 15);
    ASSERT_TRUE((meta_.dims<0>()==11));
    ASSERT_TRUE((meta_.dims<1>()==12));
    ASSERT_TRUE((meta_.dims<2>()==13));
    ASSERT_TRUE((meta_.dims<3>()==14));
    ASSERT_TRUE((meta_.dims<4>()==15));

    ASSERT_TRUE((meta_.strides(4)==15));
    ASSERT_TRUE((meta_.strides(3)==15*14));
    ASSERT_TRUE((meta_.strides(2)==15*14*13));
    ASSERT_TRUE((meta_.strides(1)==15*14*13*12));
    ASSERT_TRUE((meta_.strides(0)==15*14*13*12*11));

    ASSERT_TRUE((meta_.strides<4>(meta_.strides())==1));
    ASSERT_TRUE((meta_.strides<3>(meta_.strides())==15));
    ASSERT_TRUE((meta_.strides<2>(meta_.strides())==15*14));
    ASSERT_TRUE((meta_.strides<1>(meta_.strides())==15*14*13));
    ASSERT_TRUE((meta_.strides<0>(meta_.strides())==15*14*13*12));
#endif
}
