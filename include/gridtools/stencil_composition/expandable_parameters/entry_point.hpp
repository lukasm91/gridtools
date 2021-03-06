/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "../../common/defs.hpp"
#include "../../common/functional.hpp"
#include "../../common/tuple_util.hpp"
#include "../../meta.hpp"
#include "../arg.hpp"
#include "../backend.hpp"
#include "../caches/cache_definitions.hpp"
#include "../caches/define_caches.hpp"
#include "../esf_metafunctions.hpp"
#include "../mss.hpp"
#include "expand_factor.hpp"

namespace gridtools {
    namespace intermediate_expand_impl_ {
        template <class Plh, class = void>
        struct is_expandable : std::false_type {};

        template <class Plh>
        struct is_expandable<Plh,
            std::enable_if_t<meta::is_instantiation_of<std::vector, typename Plh::data_store_t>::value>>
            : std::true_type {};

        // The purpose of this tag is to uniquely identify the args for each index in the unrolling of the
        // expandable parameters (0...ExpandFactor).
        template <class I, class Tag>
        struct expand_tag;

        template <class, class = void>
        struct is_expanded : std::false_type {};

        template <class Plh>
        struct is_expanded<Plh, std::enable_if_t<meta::is_instantiation_of<expand_tag, typename Plh::tag_t>::value>>
            : std::true_type {};

        namespace lazy {
            template <class...>
            struct convert_plh;

            template <class I, class Plh>
            struct convert_plh<I, Plh> {
                using type = Plh;
            };

            template <class I, class ID, class DataStore, class Location>
            struct convert_plh<I, plh<ID, std::vector<DataStore>, Location>> {
                using type = plh<expand_tag<I, ID>, DataStore, Location>;
            };

            template <class I, class ID, class Data, class Location>
            struct convert_plh<I, tmp_plh<ID, std::vector<Data>, Location>> {
                using type = tmp_plh<expand_tag<I, ID>, Data, Location>;
            };

            template <class I, class Cache>
            struct convert_cache;

            template <class I, cache_type CacheType, class Plh, cache_io_policy cacheIOPolicy>
            struct convert_cache<I, detail::cache_impl<CacheType, Plh, cacheIOPolicy>> {
                using type = detail::cache_impl<CacheType, typename convert_plh<I, Plh>::type, cacheIOPolicy>;
            };
        }; // namespace lazy
        GT_META_DELEGATE_TO_LAZY(convert_plh, class... Ts, Ts...);

        template <class IndexAndCache>
        using convert_cache_f =
            typename lazy::convert_cache<meta::first<IndexAndCache>, meta::second<IndexAndCache>>::type;

        template <class Esf>
        struct convert_esf {
            template <class I>
            using apply = esf_replace_args<Esf,
                meta::transform<meta::curry<convert_plh, I>::template apply, typename Esf::args_t>>;
        };

        template <class ExpandFactor>
        struct expand_esf_f {
            template <class Esf>
            using apply = meta::transform<convert_esf<Esf>::template apply, meta::make_indices<ExpandFactor>>;
        };

        namespace lazy {
            template <class...>
            struct convert_mss;

            template <class ExpandFactor, class ExecutionEngine, class Esfs, class Caches>
            struct convert_mss<ExpandFactor, mss_descriptor<ExecutionEngine, Esfs, Caches>> {
                using esfs_t = meta::flatten<meta::transform<expand_esf_f<ExpandFactor>::template apply, Esfs>>;

                using indices_and_caches_t = meta::cartesian_product<meta::make_indices<ExpandFactor>, Caches>;
                using caches_t = meta::dedup<meta::transform<convert_cache_f, indices_and_caches_t>>;

                using type = mss_descriptor<ExecutionEngine, esfs_t, caches_t>;
            };
        } // namespace lazy
        GT_META_DELEGATE_TO_LAZY(convert_mss, class... Ts, Ts...);

        template <class ExpandFactor, class Msses, class Converter = meta::curry<convert_mss, ExpandFactor>>
        using convert_msses = meta::transform<Converter::template apply, Msses>;

        struct get_size_f {
            size_t &m_res;
            template <class T>
            void operator()(std::vector<T> const &val) const {
                if (m_res == (size_t)-1)
                    m_res = val.size();
                else
                    assert(m_res == val.size());
            }
            template <class T>
            void operator()(T const &) const {}
        };

        template <class DataStoreMap>
        size_t get_expandable_size(DataStoreMap const &src) {
            size_t res = (size_t)-1;
            tuple_util::for_each(get_size_f{res}, src);
            return res == (size_t)-1 ? 1 : res;
        }

        template <class ExpandFactor, class Item, bool = is_expandable<meta::first<Item>>::value>
        struct expand_map_item {
            using type = meta::list<Item>;
        };

        template <class ExpandFactor, class Item>
        struct expand_map_item<ExpandFactor, Item, true> {
            using raw_vector_t = meta::second<Item>;

            static constexpr bool need_const =
                !std::is_reference<raw_vector_t>::value || std::is_const<std::remove_reference_t<raw_vector_t>>::value;

            using data_store_t = typename std::decay_t<raw_vector_t>::value_type;

            using data_store_ref_t = std::add_lvalue_reference_t<
                std::conditional_t<need_const, std::add_const_t<data_store_t>, data_store_t>>;

            template <class I>
            using convert_item = meta::list<convert_plh<I, meta::first<Item>>, data_store_ref_t>;

            using type = meta::transform<convert_item, meta::make_indices<ExpandFactor>>;
        };

        template <class ExpandFactor>
        struct expand_map_item_f {
            template <class Item>
            using apply = typename expand_map_item<ExpandFactor, Item>::type;
        };

        template <class Plh, class = void>
        struct convert_data_store_f {
            template <class DataStoreMap>
            decltype(auto) operator()(size_t offset, DataStoreMap const &map) const {
                using tag_t = typename Plh::tag_t;
                using index_t = meta::first<tag_t>;
                using src_plh_t =
                    plh<meta::second<tag_t>, std::vector<typename Plh::data_store_t>, typename Plh::location_t>;
                return at_key<src_plh_t>(map)[offset + index_t::value];
            }
        };

        template <class Plh>
        struct convert_data_store_f<Plh, std::enable_if_t<!is_expanded<Plh>::value>> {
            template <class DataStoreMap>
            decltype(auto) operator()(size_t offset, DataStoreMap const &map) const {
                return at_key<Plh>(map);
            }
        };

        template <class ExpandFactor, class DataStoreMap>
        auto convert_data_store_map(size_t offset, DataStoreMap const &data_store_map) {
            using res_t = hymap::from_meta_map<meta::flatten<
                meta::transform<expand_map_item_f<ExpandFactor>::template apply, hymap::to_meta_map<DataStoreMap>>>>;
            using generators_t = meta::transform<convert_data_store_f, get_keys<res_t>>;
            return tuple_util::generate<generators_t, res_t>(offset, data_store_map);
        }

        template <class ExpandFactor, class Backend, class IsStateful, class Msses>
        struct expandable_entry_point_f {
            template <class Factor>
            using converted_entry_point = backend_entry_point_f<Backend, IsStateful, convert_msses<Factor, Msses>>;

            template <class Grid, class DataStores>
            void operator()(Grid const &grid, DataStores data_stores) const {
                size_t size = get_expandable_size(data_stores);
                size_t offset = 0;
                for (; size - offset >= ExpandFactor::value; offset += ExpandFactor::value)
                    converted_entry_point<ExpandFactor>()(
                        grid, convert_data_store_map<ExpandFactor>(offset, data_stores));
                for (; offset < size; ++offset)
                    converted_entry_point<expand_factor<1>>()(
                        grid, convert_data_store_map<expand_factor<1>>(offset, data_stores));
            }
        };
    } // namespace intermediate_expand_impl_

    using intermediate_expand_impl_::expandable_entry_point_f;
} // namespace gridtools
