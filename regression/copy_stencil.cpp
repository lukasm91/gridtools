/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>

#include <gridtools/stencil_composition/stencil_composition.hpp>
#include <gridtools/tools/regression_fixture.hpp>

using namespace gridtools;

struct copy_functor {
    using in = in_accessor<0>;
    using out = inout_accessor<1>;

    using param_list = make_param_list<in, out>;

    template <typename Evaluation>
    GT_FUNCTION static void apply(Evaluation eval) {
        eval(out()) = 2 * eval(in());
    }
};

struct copy_stencil : regression_fixture<0> {
    storage_type in = make_storage([](int i, int j, int k) { return i + j + k; });
    storage_type out = make_storage(-1.);
};

#include GT_DUMP_GENERATED_CODE(test)

#define GT
#define GEN

#ifdef GEN
TEST_F(copy_stencil, test) {
    tmp_arg<0> p_tmp;
    auto comp = make_computation(GT_DUMP_IDENTIFIER(test),
        p_0 = in,
        p_1 = out,
        make_multistage(execute::parallel(),
            define_caches(cache<cache_type::ij, cache_io_policy::local>(p_tmp)),
            make_stage<copy_functor>(p_0, p_tmp),
            make_stage<copy_functor>(p_tmp, p_1)));

    comp.run();
    verify(make_storage([](int i, int j, int k) { return 4 * (i + j + k); }), out);
    benchmark(comp);
}
#endif
#ifdef GT
TEST_F(copy_stencil, test_gt) {
    tmp_arg<0> p_tmp;
    auto comp = make_computation(GT_DUMP_IDENTIFIER(NO_DUMP),
        p_0 = in,
        p_1 = out,
        make_multistage(execute::parallel(),
            define_caches(cache<cache_type::ij, cache_io_policy::local>(p_tmp)),
            make_stage<copy_functor>(p_0, p_tmp),
            make_stage<copy_functor>(p_tmp, p_1)));

    comp.run();
    verify(make_storage([](int i, int j, int k) { return 4 * (i + j + k); }), out);
    benchmark(comp);
}
#endif
