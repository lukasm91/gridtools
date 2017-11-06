/*
  GridTools Libraries

  Copyright (c) 2017, ETH Zurich and MeteoSwiss
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  For information: http://eth-cscs.github.io/gridtools/
*/
#include "gtest/gtest.h"
#include "stencil-composition/stencil-composition.hpp"
#include "kcache_fixture.hpp"
#include "tools/verifier.hpp"

using namespace gridtools;
using namespace enumtype;

struct shift_acc_forward_flush {

    typedef accessor< 0, enumtype::in, extent<> > in;
    typedef accessor< 1, enumtype::inout, extent< 0, 0, 0, 0, -1, 0 > > out;

    typedef boost::mpl::vector< in, out > arg_list;

    template < typename Evaluation >
    GT_FUNCTION static void Do(Evaluation &eval, kminimum) {
        eval(out()) = eval(in());
    }

    template < typename Evaluation >
    GT_FUNCTION static void Do(Evaluation &eval, kbody_high) {
        eval(out()) = eval(out(0, 0, -1)) + eval(in());
    }
};

struct shift_acc_backward_flush {

    typedef accessor< 0, enumtype::in, extent<> > in;
    typedef accessor< 1, enumtype::inout, extent< 0, 0, 0, 0, 0, 1 > > out;

    typedef boost::mpl::vector< in, out > arg_list;

    template < typename Evaluation >
    GT_FUNCTION static void Do(Evaluation &eval, kmaximum) {
        eval(out()) = eval(in());
    }

    template < typename Evaluation >
    GT_FUNCTION static void Do(Evaluation &eval, kbody_low) {
        eval(out()) = eval(out(0, 0, 1)) + eval(in());
    }
};

TEST_F(kcachef, flush_forward) {

    for (uint_t i = 0; i < m_d1; ++i) {
        for (uint_t j = 0; j < m_d2; ++j) {
            m_refv(i, j, 0) = m_inv(i, j, 0);
            for (uint_t k = 1; k < m_d3; ++k) {
                m_refv(i, j, k) = m_refv(i, j, k - 1) + m_inv(i, j, k);
            }
        }
    }

    typedef arg< 0, storage_t > p_in;
    typedef arg< 1, storage_t > p_out;

    typedef boost::mpl::vector< p_in, p_out > accessor_list;
    aggregator_type< accessor_list > domain((p_out() = m_out), (p_in() = m_in));

    auto kcache_stencil =
        make_computation< BACKEND >(domain,
            m_grid,
            make_multistage // mss_descriptor
            (execute< forward >(),
                                        define_caches(cache< K, cache_io_policy::flush, kfull >(p_out())),
                                        make_stage< shift_acc_forward_flush >(p_in() // esf_descriptor
                                            ,
                                            p_out())));

    kcache_stencil->ready();

    kcache_stencil->steady();

    kcache_stencil->run();

    m_out.sync();
    m_out.reactivate_host_write_views();

#if FLOAT_PRECISION == 4
    verifier verif(1e-6);
#else
    verifier verif(1e-10);
#endif
    array< array< uint_t, 2 >, 3 > halos{{{0, 0}, {0, 0}, {0, 0}}};

    ASSERT_TRUE(verif.verify(m_grid, m_ref, m_out, halos));

    kcache_stencil->finalize();
}

TEST_F(kcachef, flush_backward) {

    for (uint_t i = 0; i < m_d1; ++i) {
        for (uint_t j = 0; j < m_d2; ++j) {
            m_inv(i, j, m_d3 - 1) = i + j + m_d3 - 1;
            m_refv(i, j, m_d3 - 1) = m_inv(i, j, m_d3 - 1);
            for (int_t k = m_d3 - 2; k >= 0; --k) {
                m_inv(i, j, k) = i + j + k;
                m_refv(i, j, k) = m_refv(i, j, k + 1) + m_inv(i, j, k);
            }
        }
    }

    typedef arg< 0, storage_t > p_in;
    typedef arg< 1, storage_t > p_out;

    typedef boost::mpl::vector< p_in, p_out > accessor_list;
    aggregator_type< accessor_list > domain((p_out() = m_out), (p_in() = m_in));

    auto kcache_stencil =
        make_computation< BACKEND >(domain,
            m_grid,
            make_multistage // mss_descriptor
            (execute< backward >(),
                                        define_caches(cache< K, cache_io_policy::flush, kfull >(p_out())),
                                        make_stage< shift_acc_backward_flush >(p_in() // esf_descriptor
                                            ,
                                            p_out())));

    kcache_stencil->ready();

    kcache_stencil->steady();

    kcache_stencil->run();

    m_out.sync();
    m_out.reactivate_host_write_views();

#if FLOAT_PRECISION == 4
    verifier verif(1e-6);
#else
    verifier verif(1e-10);
#endif
    array< array< uint_t, 2 >, 3 > halos{{{0, 0}, {0, 0}, {0, 0}}};

    ASSERT_TRUE(verif.verify(m_grid, m_ref, m_out, halos));

    kcache_stencil->finalize();
}