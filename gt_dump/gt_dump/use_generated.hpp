#pragma once

#include <gridtools/stencil-composition/accessor.hpp>
#include <gridtools/stencil-composition/make_computation.hpp>

#ifndef GT_DUMP_GENERATED_CODE
// clang-format off
#define GT_DUMP_GENERATED_CODE(name) BOOST_PP_STRINGIZE(BOOST_PP_CAT(BOOST_PP_CAT(GT_DUMP_PREFIX, __), name))
// clang-format on
#endif

#ifndef GT_DUMP_IDENTIFIER
#define GT_DUMP_IDENTIFIER(name) \
    std::integral_constant<long int, BOOST_PP_CAT(GT_DUMP_IDENTIFIER_, name)> {}
#endif

namespace gridtools {

    template <typename>
    struct generated_computation;

    template <class Backend, class Id, class Grid, class Arg, class... Args, enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_computation(Id &&, Grid const &grid, Arg &&arg, Args &&... args)
        GT_AUTO_RETURN((generated_computation<Id>{}));

} // namespace gridtools
