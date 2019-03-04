/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cassert>
#include <cstdlib>
#include <iostream>

#include <boost/mpl/vector.hpp>

#include <gridtools/stencil-composition/stencil-composition.hpp>

namespace gt = gridtools;

#ifdef __CUDACC__
using target_t = gt::target::cuda;
using backend_t = gt::backend<target_t>;
#else
#ifdef GT_BACKEND_X86
using target_t = gt::target::x86;
#ifdef STRATEGY_NAIVE
using backend_t = gt::backend<target_t, gt::strategy::naive>;
#else
using backend_t = gt::backend<target_t>;
#endif
#else
using target_t = gt::target::mc;
using backend_t = gt::backend<target_t>;
#endif
#endif

using storage_info_t = gt::storage_traits<backend_t::backend_id_t>::storage_info_t<0, 3>;
using data_store_t = gt::storage_traits<backend_t::backend_id_t>::data_store_t<double, storage_info_t>;

// These are the stencil operators that compose the multistage stencil in this test
struct multiply_by_two {
    using data = gt::accessor<0, gt::intent::inout>;
    using param_list = gt::make_param_list<data>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval) {
        eval(data()) = 2 * eval(data());
    }
};
// this is to enforce in having an non-zero extent
struct pseudo_functor {
    using in = gt::accessor<0, gt::intent::in, gt::extent<-1, 1, -1, 1>>;
    using out = gt::accessor<1, gt::intent::inout>;

    using param_list = gt::make_param_list<in, out>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval) {}
};

bool verify(data_store_t const &data) {
    auto data_v = gt::make_host_view(data);

    bool success = true;
    for (int k = data_v.total_begin<2>(); k <= data_v.total_end<2>(); ++k) {
        for (int i = data_v.total_begin<0>(); i <= data_v.total_end<0>(); ++i) {
            for (int j = data_v.total_begin<1>(); j <= data_v.total_end<1>(); ++j) {
                if (data_v(i, j, k) != 2) {
                    std::cout << "error in " << i << ", " << j << ", " << k << ": "
                              << "data = " << data_v(i, j, k) << ", expected = " << 2 << std::endl;
                    success = false;
                }
            }
        }
    }
    return success;
}

int main(int argc, char **argv) {

    gt::uint_t d1, d2, d3;
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " dimx dimy dimz\n";
        return 1;
    } else {
        d1 = atoi(argv[1]);
        d2 = atoi(argv[2]);
        d3 = atoi(argv[3]);
    }

    // storage_info contains the information aboud sizes and layout of the storages to which it will be passed
    storage_info_t meta_data_{d1, d2, d3};

    // Definition of placeholders. The order does not have any semantics
    using p_ignore = gt::arg<0, data_store_t>;
    using p_data = gt::arg<1, data_store_t>;

    // Now we describe the itaration space. The first two dimensions
    // are described by halo_descriptors. In this case, since the
    // stencil has zero-exent, not particular care should be focused
    // on centering the iteration space to avoid put-of-bound
    // access. The third dimension is indicated with a simple size,
    // since there is not specific axis definition.
    auto grid = gt::make_grid(gt::halo_descriptor(1, 1, 1, d1 - 2, d1), gt::halo_descriptor(1, 1, 1, d2 - 2, d1), d3);

    data_store_t ignore{meta_data_, 0, "ignore"};
    data_store_t data{meta_data_, [](int i, int j, int k) { return 1; }, "data"};

    auto stencil = gt::make_computation<backend_t>(grid,
        p_ignore{} = ignore,
        p_data{} = data,
        gt::make_multistage(gt::execute::parallel{},
            gt::make_stage<multiply_by_two>(p_data{}), // will be calculated for extent<-1, 1, -1, 1>
            gt::make_stage<pseudo_functor>(p_data{}, p_ignore{})));

    stencil.run();

    data.sync();
    ignore.sync();

    bool success = verify(data);

    if (success) {
        std::cout << "Successful\n";
    } else {
        std::cout << "Failed\n";
    }

    return !success;
};
