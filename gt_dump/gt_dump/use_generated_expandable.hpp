#pragma once

#include "use_generated.hpp"

#include <gridtools/stencil_composition/expandable_parameters/make_computation.hpp>

namespace gridtools {

    template <class Backend,
        class ID,
        class Grid,
        size_t N,
        class Arg,
        class... Args,
        enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_expandable_computation(ID &&id, expand_factor<N>, Grid const &grid, Arg &&arg, Args &&... args)
        GT_AUTO_RETURN(make_expandable_computation_orig<Backend>(
            std::forward<ID>(id), expand_factor<N>{}, grid, std::forward<Arg>(arg), std::forward<Args>(args)...));

} // namespace gridtools

