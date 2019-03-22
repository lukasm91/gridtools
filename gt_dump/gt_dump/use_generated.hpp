#pragma once

#include <gridtools/stencil-composition/accessor.hpp>
#include <gridtools/stencil-composition/make_computation.hpp>

#include "helpers.hpp"

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

    template <typename Id, typename Grid, typename ArgStoragePairs>
    struct generated_computation;

    namespace gt_gen_helpers {} // namespace gt_gen_helpers

    template <class Backend,
        class Id,
        class Grid,
        class Arg,
        class... Args,
        enable_if_t<is_grid<Grid>::value, int> = 0,
        class ArgsPair = decltype(split_args<is_arg_storage_pair>(
            std::forward<Arg>(std::declval<Arg>()), std::forward<Args>(std::declval<Args>())...)),
        class ArgStoragePairs = GT_META_CALL(_impl::decay_elements, typename ArgsPair::first_type)>
    generated_computation<Id, Grid, ArgStoragePairs> make_computation(
        Id &&, Grid const &grid, Arg &&arg, Args &&... args) {
        auto &&args_pair = split_args<is_arg_storage_pair>(std::forward<Arg>(arg), std::forward<Args>(args)...);
        return {grid, std::move(args_pair.first)};
    }

} // namespace gridtools
