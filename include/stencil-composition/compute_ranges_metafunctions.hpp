#pragma once

#ifdef STRUCTURED_GRIDS
    #include "stencil-composition/structured_grids/compute_ranges_metafunctions.hpp"
#else
    #include "stencil-composition/icosahedral_grids/compute_ranges_metafunctions.hpp"
#endif
