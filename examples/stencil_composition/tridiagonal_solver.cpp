/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gridtools/stencil_composition/stencil_composition.hpp>
#include <gridtools/stencil_composition/stencil_functions.hpp>

#include <iostream>

/*
  @file This file shows an implementation of the Thomas algorithm, done using stencil operations.

  Important convention: the linear system as usual is represented with 4 vectors: the main diagonal
  (diag), the upper and lower first diagonals (sup and inf respectively), and the right hand side
  (rhs). Note that the dimensions and the memory layout are, for an NxN system
  rank(diag)=N       [xxxxxxxxxxxxxxxxxxxxxxxx]
  rank(inf)=N-1      [0xxxxxxxxxxxxxxxxxxxxxxx]
  rank(sup)=N-1      [xxxxxxxxxxxxxxxxxxxxxxx0]
  rank(rhs)=N        [xxxxxxxxxxxxxxxxxxxxxxxx]
  where x denotes any number and 0 denotes the padding, a dummy value which is not used in
  the algorithm. This choice corresponds to having the same vector index for each row of the matrix.
 */

namespace gt = gridtools;

using backend_t = gt::backend::mc;

using axis_t = gt::axis<2>;
using full_t = axis_t::full_interval;

struct some_function {
    using out = gt::inout_accessor<0>;
    using in = gt::in_accessor<1, gt::extent<-2, 0, 0, 0>>;

    using param_list = gt::make_param_list<out, in>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, axis_t::get_interval<1>) {
        eval(out()) = eval(in(-2, 0, 0)) + eval(in(0, 0, 0));
    }
    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, axis_t::get_interval<0>::modify<1, 0>) {
        eval(out()) = 0.5 * eval(in(-2, 0, 0)) + eval(in(0, 0, 0));
    }
};

struct fwd_stage_1 {
    using out = gt::inout_accessor<0>;
    using mask = gt::in_accessor<1>;
    using in = gt::in_accessor<2, gt::extent<-1, 1, 0, 0>>;

    using param_list = gt::make_param_list<out, mask, in>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, axis_t::get_interval<1>) {
        if (eval(mask()))
            eval(out()) = 0.5 * (eval(in(1, 0, 0)) + eval(in(-1, 0, 0)));
        else
            eval(out()) = gt::call<some_function, axis_t::get_interval<1>>::at<1, 0, 0>::with(eval, in());
    }

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, axis_t::get_interval<0>::modify<1, 0>) {
        if (eval(mask()))
            eval(out()) = 2 * 0.5 * (eval(in(1, 0, 0)) + eval(in(-1, 0, 0)));
        else
            // note that this is equivalent to the syntax on line 65
            gt::call_proc<some_function, axis_t::get_interval<0>::modify<1, 0>>::at<1, 0, 0>::with(eval, out(), in());
    }

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, full_t::first_level) {
        if (eval(mask()))
            eval(out()) = 4 * 0.5 * (eval(in(1, 0, 0)) + eval(in(-1, 0, 0)));
        else
            eval(out()) = 4 * eval(in(0, 0, 0));
    }
};

struct fwd_stage_2 {
    using out = gt::inout_accessor<0, gt::extent<0, 0, 0, 0, 0, 1>>;
    using in = gt::in_accessor<1>;

    using param_list = gt::make_param_list<out, in>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, full_t::modify<0, -1>) {
        eval(out()) = eval(out(0, 0, -1)) + eval(in());
    }

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, full_t::last_level) {
        eval(out()) = 2 * eval(out(0, 0, -1));
    }
};

struct bwd_stage_1 {
    using out = gt::inout_accessor<0, gt::extent<0, 0, 0, 0, 0, 1>>;
    using global = gt::global_accessor<1>;

    using param_list = gt::make_param_list<out, global>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, full_t::last_level) {
        for (int i = 0; i < eval(global()).first; ++i) {
            eval(out()) = eval(out());
        }
    }
    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval, full_t::modify<0, -1>) {
        for (int i = 0; i < eval(global()).first; ++i) {
            eval(out()) = eval(out()) + eval(out(0, 0, 1)) + eval(global()).second;
        }
    }
};

int main() {
    unsigned int d1 = 10;
    unsigned int d2 = 10;
    unsigned int d3l = 6;
    unsigned int d3u = 6;

    using storage_tr = gt::storage_traits<backend_t>;

    using storage_info_t = storage_tr::storage_info_t<0, 3>;
    using masked_storage_info_t = storage_tr::special_storage_info_t<1, gridtools::selector<true, true, false>>;

    using storage_type = storage_tr::data_store_t<double, storage_info_t>;
    using masked_storage_type = storage_tr::data_store_t<bool, masked_storage_info_t>;
    using global_parameter_type = gt::global_parameter<backend_t, std::pair<int, float>>;

    auto out = storage_type{storage_info_t{d1, d2, d3u + d3l}};
    auto param = gt::make_global_parameter<backend_t, std::pair<int, float>>(std::make_pair(3, 1.2f));

    gt::arg<0, storage_type> p_in;
    gt::arg<1, masked_storage_type> p_mask;
    gt::arg<2, storage_type> p_out;
    gt::arg<3, global_parameter_type> p_param;

    gt::tmp_arg<4, storage_type> p_tmp;

    gt::halo_descriptor di{1, 2, 0, d1 - 1, d1};
    gt::halo_descriptor dj{1, 2, 0, d2 - 1, d2};

    auto grid = gt::make_grid(di, dj, axis_t{d3l, d3u});

    auto comp = gt::make_computation<backend_t>(grid,
        p_mask = masked_storage_type{masked_storage_info_t{d1, d2, d3u + d3l}, -1.},
        p_in = storage_type{storage_info_t{d1, d2, d3u + d3l}, -1.},
        p_out = out,
        p_param = param,
        gt::make_multistage(gt::execute::forward(),
            gt::make_stage<fwd_stage_1>(p_tmp, p_mask, p_in),
            gt::make_stage<fwd_stage_2>(p_out, p_in)),
        gt::make_multistage(gt::execute::backward(), gt::make_stage<bwd_stage_1>(p_out, p_param)));

    comp.run();

    out.sync();
}
