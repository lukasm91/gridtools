#pragma once

#include "generator.hpp"

#include <gridtools/stencil_composition/expandable_parameters/make_computation.hpp>

namespace gridtools {
    namespace gt_gen_helpers {
        template <size_t ExpandFactor,
            bool IsStateful,
            class Backend,
            class Grid,
            class BoundArgStoragePairs,
            class MssDescriptors>
        struct dump_intermediate<
            intermediate_expand<ExpandFactor, IsStateful, Backend, Grid, BoundArgStoragePairs, MssDescriptors>> {
            using non_expandable_bound_arg_storage_pairs_t = GT_META_CALL(
                meta::filter, (meta::not_<_impl::expand_detail::is_expandable>::apply, BoundArgStoragePairs));

            template <size_t N>
            using converted_intermediate = intermediate<IsStateful,
                Backend,
                Grid,
                non_expandable_bound_arg_storage_pairs_t,
                GT_META_CALL(_impl::expand_detail::converted_mss_descriptors, (N, MssDescriptors))>;

            using intermediate_expanded = converted_intermediate<ExpandFactor>;
            using intermediate_remainder = converted_intermediate<1>;

            void operator()(const std::string &filename) const {
                dump_intermediate<intermediate_expanded>{}(filename + "_expanded");
                dump_intermediate<intermediate_remainder>{}(filename + "_remainder");
            }
        };
    } // namespace gt_gen_helpers

    /// generator for intermediate
    template <class Backend, class Grid, size_t N, class Arg, class... Args, enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_expandable_computation(
        std::string const &name, expand_factor<N>, Grid const &grid, Arg &&arg, Args &&... args)
        GT_AUTO_RETURN(gt_gen_helpers::dump_and_return(
            make_expandable_computation<Backend>(
                expand_factor<N>{}, grid, std::forward<Arg>(arg), std::forward<Args>(args)...),
            name));

    template <class Backend, class Grid, size_t N, class Arg, class... Args, enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_expandable_computation(std::string &&name, expand_factor<N>, Grid const &grid, Arg &&arg, Args &&... args)
        GT_AUTO_RETURN(gt_gen_helpers::dump_and_return(
            make_expandable_computation<Backend>(
                expand_factor<N>{}, grid, std::forward<Arg>(arg), std::forward<Args>(args)...),
            name));
} // namespace gridtools
