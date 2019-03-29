#pragma once

#include <gridtools/common/defs.hpp>
#include <gridtools/meta/at.hpp>
#include <gridtools/meta/first.hpp>
#include <gridtools/meta/st_contains.hpp>
#include <gridtools/meta/st_position.hpp>
#include <gridtools/meta/transform.hpp>
#include <gridtools/stencil_composition/arg.hpp>

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
            using DataType = typename BoundArgStoragePair::data_store_t::data_t;

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

        struct make_view_f {
            template <typename T>
            void operator()(T &&storage_pair) const {
                const auto &storage = storage_pair.m_value;
                if (storage.device_needs_update())
                    storage.sync();
                make_target_view(storage);
            }
        };
        struct sync_f {
            template <typename T>
            void operator()(T &&storage_pair) const {
                const auto &storage = storage_pair.m_value;
                storage.sync();
            }
        };

        template <typename LayoutMap>
        GT_META_DEFINE_ALIAS(si_array_t, gridtools::meta::id, (gridtools::array<int, LayoutMap::unmasked_length>));

        template <typename DataStore>
        auto get_ptr(DataStore const &data_store) GT_AUTO_RETURN((gridtools::advanced::get_raw_pointer_of(
            gridtools::make_target_view<gridtools::access_mode::read_only>(data_store))));

        template <typename T>
        struct AsIntegralConstants;
        template <template <int...> typename M, int... Is>
        struct AsIntegralConstants<M<Is...>> {
            using type = std::tuple<std::integral_constant<int, Is>...>;
        };

        template <typename LayoutMap, typename RetType = si_array_t<LayoutMap>>
        RetType extract_unmasked(gridtools::array<gridtools::uint_t, LayoutMap::masked_length> const &masked) {
            RetType ret;
            for (int read_pos = 0, write_pos = 0; read_pos < LayoutMap::masked_length; ++read_pos)
                if (LayoutMap::at(read_pos) != -1)
                    ret[write_pos++] = masked[read_pos];

            return ret;
        }
        template <typename DataStore, typename LayoutMap = typename DataStore::storage_info_t::layout_t>
        auto extract_stride(DataStore const &data_store)
            GT_AUTO_RETURN(extract_unmasked<LayoutMap>(data_store.info().strides()));

        template <typename DataStore, typename LayoutMap = typename DataStore::storage_info_t::layout_t>
        auto extract_dim(DataStore const &data_store)
            GT_AUTO_RETURN(extract_unmasked<LayoutMap>(data_store.info().total_lengths()));

        template <typename StorageInfo>
        GT_META_DEFINE_ALIAS(extract_layout_map, gridtools::meta::id, typename StorageInfo::layout_t);

        template <typename BoundData>
        struct computation_ptrs;
        template <typename... BoundPlaceholders, typename... BoundDataStores>
        struct computation_ptrs<std::tuple<gridtools::arg_storage_pair<BoundPlaceholders, BoundDataStores>...>> {
            using ptrs = std::tuple<typename BoundPlaceholders::data_store_t::data_t *...>;
            using storage_infos = GT_META_CALL(
                gridtools::meta::dedup, std::tuple<typename BoundPlaceholders::data_store_t::storage_info_t...>);
            using layout_maps = GT_META_CALL(gridtools::meta::transform, (extract_layout_map, storage_infos));
            using si_arrays = GT_META_CALL(gridtools::meta::transform, (si_array_t, layout_maps));

            using bound_arg_keys = GT_META_CALL(gridtools::meta::transform,
                (gridtools::gt_gen_helpers::get_tag_of_plh_t, std::tuple<BoundPlaceholders...>));

            ptrs ptrs_;
            si_arrays strides_;
            si_arrays dims_;

            struct assign_arg_f {
                ptrs &ptrs_;

                template <typename Placeholder,
                    typename Datastore,
                    typename PtrPosition = typename GT_META_CALL(
                        gridtools::meta::st_position, (std::tuple<BoundPlaceholders...>, Placeholder))::type>
                void operator()(gridtools::arg_storage_pair<Placeholder, Datastore> &&arg) const {
                    // normal accessor
                    std::get<PtrPosition::value>(ptrs_) = get_ptr(arg.m_value);
                }
            };
            struct assign_si_f {
                si_arrays &strides_;
                si_arrays &dims_;

                template <typename Placeholder,
                    typename Datastore,
                    typename StorageInfo = typename Placeholder::data_store_t::storage_info_t,
                    typename StrideArray = GT_META_CALL(si_array_t, typename StorageInfo::layout_t),
                    typename SIPosition = typename GT_META_CALL(
                        gridtools::meta::st_position, (storage_infos, StorageInfo))::type>
                void operator()(gridtools::arg_storage_pair<Placeholder, Datastore> &&arg) const {
                    // normal accessor
                    std::get<SIPosition::value>(strides_) = extract_stride(arg.m_value);
                    std::get<SIPosition::value>(dims_) = extract_dim(arg.m_value);
                }
            };

            void assign(std::tuple<gridtools::arg_storage_pair<BoundPlaceholders, BoundDataStores>...> &&args) {
                auto assign_args = gridtools::tuple_util::for_each(assign_arg_f{ptrs_});
                assign_args(std::move(args));

                auto assign_si = gridtools::tuple_util::for_each(assign_si_f{strides_, dims_});
                assign_si(std::move(args));
            }

            template <gridtools::uint_t ArgId,
                typename ArgPosition = GT_META_CALL(
                    gridtools::meta::st_position, (bound_arg_keys, std::integral_constant<gridtools::uint_t, ArgId>)),
                typename Placeholder =
                    GT_META_CALL(gridtools::meta::at, (std::tuple<BoundPlaceholders...>, ArgPosition)),
                typename StorageInfo = typename Placeholder::data_store_t::storage_info_t>
            struct si_position {
                using type = GT_META_CALL(gridtools::meta::st_position, (storage_infos, StorageInfo));
            };
            template <gridtools::uint_t ArgId>
            using si_position_t = typename si_position<ArgId>::type;

            template <gridtools::uint_t ArgId>
            using si_array = GT_META_CALL(gridtools::meta::at, (si_arrays, si_position_t<ArgId>));

            template <gridtools::uint_t ArgId>
            si_array<ArgId> get_stride() const {
                return std::get<si_position_t<ArgId>::value>(strides_);
            }

            template <gridtools::uint_t ArgId>
            si_array<ArgId> get_dim() const {
                return std::get<si_position_t<ArgId>::value>(dims_);
            }
        };
        // this is like the normal extent, but we use gridtools integral_constants because they have conversion
        // operators
        template <int IMinus = 0, int IPlus = 0, int JMinus = 0, int JPlus = 0, int KMinus = 0, int KPlus = 0, int...>
        struct extent {
            using type = extent;

            using iminus = gridtools::integral_constant<int, IMinus>;
            using iplus = gridtools::integral_constant<int, IPlus>;
            using jminus = gridtools::integral_constant<int, JMinus>;
            using jplus = gridtools::integral_constant<int, JPlus>;
            using kminus = gridtools::integral_constant<int, KMinus>;
            using kplus = gridtools::integral_constant<int, KPlus>;
        };
        static constexpr int block_i_size = 32;
        static constexpr int block_j_size = 8;

        template <typename Extent>
        struct get_blocked_i_padded_length {
            static constexpr const int alignment = 128 / sizeof(double);

            static constexpr const int full_block_i_size = block_i_size - Extent::iminus::value + Extent::iplus::value;
            static constexpr const int i_padded_length = (full_block_i_size + alignment - 1) / alignment * alignment;
            static constexpr const int value = i_padded_length;
        };

        template <typename Extent>
        struct get_blocked_j_padded_length {
            static constexpr const int j_padded_length = block_j_size - Extent::jminus::value + Extent::jplus::value;
            static constexpr const int value = j_padded_length;
        };

        template <typename Ptrs, typename ArgMap>
        struct eval {
            Ptrs &ptrs_;

            template <class Accessor, enable_if_t<Accessor::intent_v == intent::in, int> = 0>
            GT_FUNCTION_DEVICE auto operator()(Accessor const &arg) const
                noexcept GT_AUTO_RETURN(static_cast<const Ptrs &>(ptrs_).resolve(decltype(ArgMap{}(arg)){}, arg));

            template <class Accessor, enable_if_t<Accessor::intent_v == intent::inout, int> = 0>
            GT_FUNCTION_DEVICE auto operator()(Accessor const &arg) const
                noexcept GT_AUTO_RETURN(ptrs_.resolve(decltype(ArgMap{}(arg)){}, arg));

            template <uint_t I>
            GT_FUNCTION_DEVICE auto operator()(global_accessor<I> const &arg) const
                noexcept GT_AUTO_RETURN(ptrs_.resolve(decltype(ArgMap{}(arg)){}));

            template <class Op, class... Ts>
            GT_FUNCTION_DEVICE auto operator()(expr<Op, Ts...> const &arg) const
                noexcept GT_AUTO_RETURN(expressions::evaluation::value(*this, arg));
        };
    } // namespace gt_gen_helpers
} // namespace gridtools
