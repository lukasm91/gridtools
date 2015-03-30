#pragma once
#include <boost/mpl/for_each.hpp>
#include "../backend_traits_fwd.h"

/**@file
@brief type definitions and structures specific for the CUDA backend*/
namespace gridtools{

    /**forward declaration*/
    namespace _impl_cuda{
        template <typename Arguments>
        struct run_functor_cuda;
    }

    /**forward declaration*/
    template<typename T>
    struct hybrid_pointer;

    /** @brief traits struct defining the types which are specific to the CUDA backend*/
    template<>
    struct backend_traits_from_id< enumtype::Cuda >
    {

        template <typename T>
        struct pointer
        {
            typedef hybrid_pointer<T> type;
        };

        template <typename ValueType, typename Layout, bool Temp=false, short_t SpaceDim=1 >
        struct storage_traits
        {
            typedef storage< base_storage<typename pointer<ValueType>::type, Layout, Temp, SpaceDim> > storage_t;
        };

        template <typename Arguments>
        struct execute_traits
        {
            typedef _impl_cuda::run_functor_cuda<Arguments> run_functor_t;
        };

        //function alias (pre C++11)
        template<
            typename Sequence
            , typename F
            >
        inline static void for_each(F f)
            {
                boost::mpl::for_each<Sequence>(f);
            }

#ifdef __CUDACC__
        /**
           @brief assigns the two given values using the given thread Id whithin the block
        */
        template <uint_t Id>
        struct once_per_block {
            template<typename Left, typename Right>
            GT_FUNCTION
            static void assign(Left& l, Right const& r){
                if(threadIdx.x==Id)
                    {
                        l=r;
                    }
            }
        };
#endif

    };

}//namespace gridtools