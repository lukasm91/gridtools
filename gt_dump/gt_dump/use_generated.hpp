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

    template <typename Id, typename Grid, typename ArgStoragePairs>
    struct generated_computation;

    namespace _impl {
        template <typename T>
        struct get_tag;
        template <uint_t I>
        struct get_tag<_impl::arg_tag<I>> : std::integral_constant<uint_t, I> {};
        template <typename T>
        struct get_tag_of_plh;
        template <class Tag, typename DataStore, typename Location, bool Temporary>
        struct get_tag_of_plh<plh<Tag, DataStore, Location, Temporary>> : get_tag<Tag> {};
        template <typename T>
        struct is_temporary_plh;
        template <class Tag, typename DataStore, typename Location, bool Temporary>
        struct is_temporary_plh<plh<Tag, DataStore, Location, Temporary>> : std::integral_constant<bool, Temporary> {};

        template <typename T>
        using get_tag_of_plh_t = typename get_tag_of_plh<T>::type;
    } // namespace _impl

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
