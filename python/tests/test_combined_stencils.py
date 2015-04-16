import unittest
import logging

import numpy as np

from nose.plugins.attrib import attr

from gridtools.stencil import MultiStageStencil, StencilInspector




class FluxI (MultiStageStencil):
    def kernel (self, out_fli, in_lapi):
        for p in self.get_interior_points (out_fli):
            out_fli[p] = in_lapi[p + (1,0,0)] - in_lapi[p]



class FluxJ (MultiStageStencil):
    def kernel (self, out_flj, in_lapj):
        for p in self.get_interior_points (out_flj):
            out_flj[p] = in_lapj[p + (0,1,0)] - in_lapj[p]



class Out (MultiStageStencil):
    def kernel (self, out_hr, in_wgt, in_fli, in_flj):
        for p in self.get_interior_points (out_hr):
            out_hr[p] = in_wgt[p] * ( in_fli[p + (-1,0,0)] - in_fli[p] +
                                      in_flj[p + (0,-1,0)] - in_flj[p] )   



class CombinedStencilTest (unittest.TestCase):
    def setUp (self):
        import os
        from tests.test_stencils import Copy, Laplace

        logging.basicConfig (level=logging.DEBUG)
        self.cur_dir = os.path.dirname (os.path.abspath (__file__))

        self.domain = (64, 64, 32)
        self.params = ('out_fli', 'in_data')
        self.temps  = ( )

        self.out_fli  = np.zeros (self.domain)
        self.in_wgt   = np.ones  (self.domain)
        self.in_data  = np.zeros (self.domain)
        for i in range (self.domain[0]):
            for j in range (self.domain[1]):
                for k in range (self.domain[2]):
                    self.in_data[i,j,k] = i**3 + j

        self.lap = Laplace ( )

        self.copy = Copy ( )

        self.fli = FluxI ( )
        self.flj = FluxJ ( )
        self.out = Out   ( )


    def test_single_combination (self, backend='python'):
        self.lap.set_halo  ( (1, 1, 1, 1) )

        combined = self.lap.build (output='out_data')
        combined.backend = backend
        combined.run (out_data=self.out_fli,
                      in_data=self.in_data)
        #
        # parameters correctly inferred
        #
        for p in combined.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_data', 'in_data'))
        #
        # results should be correct
        #
        expected = np.load ('%s/laplace_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))


    @attr(lang='c++')
    def test_single_combination_native (self):
        self.test_single_combination (backend='c++')


    def test_double_combination (self):
        self.lap.set_halo  ( (1, 1, 1, 1) )
        self.copy.set_halo ( (0, 0, 0, 0) )

        combined = self.copy.build (output='out_cpy',
                                    in_cpy=self.lap.build (output='out_data'))
        combined.backend = 'python'
        combined.run (out_cpy=self.out_fli,
                      in_data=self.in_data)
        #
        # parameters correctly inferred
        #
        for p in combined.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_cpy', 'in_data'))
        #
        # results should be correct
        #
        expected = np.load ('%s/laplace_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))


    def test_order_should_not_alter_results (self):
        self.lap.set_halo  ( (1, 1, 1, 1) )
        self.copy.set_halo ( (0, 0, 0, 0) )

        combined = self.copy.build (output='out_cpy',
                                    in_cpy=self.lap.build (output='out_data'))
        combined.backend = 'python'
        combined.run (out_cpy=self.out_fli,
                      in_data=self.in_data)
        #
        # results should be correct
        #
        expected = np.load ('%s/laplace_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))
        #
        # change the order of the combination
        #
        combined = self.lap.build (output='out_data',
                                   in_data=self.copy.build (output='out_cpy'))
        #
        # parameters correctly inferred
        #
        for p in combined.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_data', 'in_cpy'))
        combined.backend = 'python'
        combined.run (out_data=self.out_fli,
                      in_cpy=self.in_data)
        #
        # results should be correct
        #
        expected = np.load ('%s/laplace_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))


    def test_double_self_combination (self):
        self.in_data = np.random.rand (*self.domain)

        combined = self.copy.build (output='out_cpy',
                                    in_cpy=self.copy.build (output='out_cpy'))
        combined.backend = 'python'
        combined.run (out_cpy=self.out_fli,
                      in_cpy=self.in_data)
        #
        # parameters correctly inferred
        #
        for p in combined.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_cpy', 'in_cpy'))
        #
        # results should be correct
        #
        self.assertTrue (np.array_equal (self.out_fli,
                                         self.in_data))


    def test_triple_self_combination (self):
        self.in_data = np.random.rand (*self.domain)
        
        #
        # this one fails because of the depth of the built tree
        #
        combined = self.copy.build (output='out_cpy',
                                    in_cpy=self.copy.build (output='out_cpy',
                                                            in_cpy=self.copy.build (output='out_cpy')))
        combined.backend = 'python'
        combined.run (out_cpy=self.out_fli,
                      in_cpy=self.in_data)
        #
        # parameters correctly inferred
        #
        for p in combined.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_cpy', 'in_cpy'))
        #
        # results should be correct
        #
        self.assertTrue (np.array_equal (self.out_fli,
                                         self.in_data))


    def test_partial_combinations (self):
        self.lap.set_halo ( (1, 1, 1, 1) )
        self.fli.set_halo ( (1, 1, 1, 1) )
        self.flj.set_halo ( (1, 1, 1, 1) )
        self.out.set_halo ( (1, 1, 1, 1) )

        #
        # FluxI + Laplace
        #
        combo = self.fli.build (output='out_fli',
                                in_lapi=self.lap.build (output='out_data'))
        combo.backend = 'python'
        combo.run (out_fli=self.out_fli,
                   in_data=self.in_data)
        #
        # check parameters and results
        #
        for p in combo.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_fli', 'in_data'))
        expected = np.load ('%s/fluxi_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))
        #
        # FluxJ + Laplace
        #
        combo = self.flj.build (output='out_flj',
                                in_lapj=self.lap.build (output='out_data'))
        combo.backend = 'python'
        combo.run (out_flj=self.out_fli,
                   in_data=self.in_data)
        #
        # check parameters and results
        #
        for p in combo.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_flj', 'in_data'))
        expected = np.load ('%s/fluxj_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))
        #
        # Out + FluxI + FluxJ
        #
        combo = self.out.build (output='out_hr',
                                in_fli=self.fli.build (output='out_fli'),
                                in_flj=self.flj.build (output='out_flj'))
        combo.backend = 'python'
        combo.run (out_hr=self.out_fli,
                   in_wgt=self.in_wgt,
                   in_lapi=np.load ('%s/laplace_result.npy' % self.cur_dir),
                   in_lapj=np.load ('%s/laplace_result.npy' % self.cur_dir))
        #
        # check parameters and results
        #
        for p in combo.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_hr', 'in_wgt', 'in_lapi', 'in_lapj'))
        expected = np.load ('%s/horizontaldiffusion_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))


    def test_horizontal_diffusion_combination (self):
        self.lap.set_halo ( (1, 1, 1, 1) )
        self.fli.set_halo ( (1, 1, 1, 1) )
        self.flj.set_halo ( (1, 1, 1, 1) )
        self.out.set_halo ( (1, 1, 1, 1) )

        hor_dif = self.out.build (output='out_hr',
                                  in_fli=self.fli.build (output='out_fli',
                                                         in_lapi=self.lap.build (output='out_data')),
                                  in_flj=self.flj.build (output='out_flj',
                                                         in_lapj=self.lap.build (output='out_data')))
        hor_dif.backend = 'python'
        hor_dif.run (out_hr=self.out_fli,
                     in_wgt=self.in_wgt,
                     in_data=self.in_data)
        #
        # check parameters and results
        #
        for p in hor_dif.scope.get_parameters ( ):
            self.assertTrue (p.name in ('out_hr', 'in_wgt', 'in_data'))
        expected = np.load ('%s/horizontaldiffusion_result.npy' % self.cur_dir)
        self.assertTrue (np.array_equal (self.out_fli,
                                         expected))

