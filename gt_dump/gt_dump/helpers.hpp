#pragma once

#include <gridtools/common/defs.hpp>
#include <gridtools/meta/at.hpp>
#include <gridtools/meta/first.hpp>
#include <gridtools/meta/st_contains.hpp>
#include <gridtools/meta/st_position.hpp>
#include <gridtools/meta/transform.hpp>
#include <gridtools/stencil-composition/arg.hpp>

#ifndef __CUDACC__
#if not defined(WORKAROUND)
void __syncthreads() {}
struct {
    int x;
    int y;
    int z;
} blockIdx, threadIdx;
template <typename T>
T &__ldg(T *t) {
    return *t;
}
#endif
#endif

namespace gridtools {
    namespace gt_gen_helpers {
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

        template <typename BoundArgStoragePairs>
        GT_META_DEFINE_ALIAS(bound_args, meta::transform, (meta::first, BoundArgStoragePairs));

        template <typename BoundArgStoragePairs, typename BoundArgs = GT_META_CALL(bound_args, BoundArgStoragePairs)>
        GT_META_DEFINE_ALIAS(bound_arg_keys, meta::transform, (gt_gen_helpers::get_tag_of_plh_t, BoundArgs));

        template <uint_t ArgId,
            typename BoundArgStoragePairs,
            typename BoundArgKeys = GT_META_CALL(bound_arg_keys, BoundArgStoragePairs)>
        GT_META_DEFINE_ALIAS(
            is_static_bound_arg, meta::st_contains, (BoundArgKeys, std::integral_constant<uint_t, ArgId>));

        template <uint_t ArgId,
            typename BoundArgStoragePairs,
            typename BoundArgKeys = GT_META_CALL(bound_arg_keys, BoundArgStoragePairs)>
        GT_META_DEFINE_ALIAS(
            get_bound_arg_position, meta::st_position, (BoundArgKeys, std::integral_constant<uint_t, ArgId>));

        template <bool IsStaticBound, uint_t ArgId, typename BoundArgStoragePairs>
        struct get_bound_arg_result_type_helper;
        template <uint_t ArgId, typename BoundArgStoragePairs>
        struct get_bound_arg_result_type_helper<false, ArgId, BoundArgStoragePairs> {
            using type = void *;
        };
        template <uint_t ArgId, typename BoundArgStoragePairs>
        struct get_bound_arg_result_type_helper<true, ArgId, BoundArgStoragePairs> {
            using Position = GT_META_CALL(get_bound_arg_position, (ArgId, BoundArgStoragePairs));
            using BoundArgStoragePair = GT_META_CALL(meta::at, (BoundArgStoragePairs, Position));
            using DataType = typename BoundArgStoragePair::view_t::data_t;

            using type = DataType *;
        };

        template <uint_t ArgId,
            typename BoundArgStoragePairs,
            typename IsStaticBound = GT_META_CALL(is_static_bound_arg, (ArgId, BoundArgStoragePairs)),
            typename RetType =
                typename get_bound_arg_result_type_helper<IsStaticBound::value, ArgId, BoundArgStoragePairs>::type>
        GT_META_DEFINE_ALIAS(get_bound_arg_result_type, meta::id, RetType);

        template <uint_t ArgId, bool Temporary>
        struct arg_identifier {};
    } // namespace gt_gen_helpers
} // namespace gridtools
