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
/**
   @file
   @brief File containing the definition of the placeholders used to address the storage from whithin the functors.
   A placeholder is an implementation of the proxy design pattern for the storage class, i.e. it is a light object used
   in place of the storage when defining the high level computations,
   and it will be bound later on with a specific instantiation of a storage class.
*/

#pragma once

#include <iosfwd>
#include <utility>

#include "../common/defs.hpp"
#include "arg_fwd.hpp"
#include "arg_metafunctions.hpp"
#include "arg_metafunctions_fwd.hpp"
#include "../storage/storage-facility.hpp"

namespace gridtools {

    template < typename T >
    struct is_arg : boost::mpl::false_ {};

    template < uint_t I, typename DataStoreType, typename Location, bool Temporary >
    struct is_arg< arg< I, DataStoreType, Location, Temporary > > : boost::mpl::true_ {};

    /** @brief binding between the placeholder (\tparam ArgType) and the storage (\tparam DataStoreType)*/
    template < typename ArgType, typename DataStoreType >
    struct arg_storage_pair {

        GRIDTOOLS_STATIC_ASSERT(is_arg< ArgType >::value, GT_INTERNAL_ERROR);
        GRIDTOOLS_STATIC_ASSERT((boost::is_same< typename ArgType::data_store_t, DataStoreType >::type::value),
            "DataStoreType type not compatible with placeholder storage type, when associating placeholder to actual "
            "data store");

        arg_storage_pair() = default;
        arg_storage_pair(const DataStoreType &val) : m_value{val} {}
        arg_storage_pair(DataStoreType &&val) noexcept : m_value{std::move(val)} {}
        ~arg_storage_pair() = default;

        DataStoreType m_value;

        typedef ArgType arg_t;
        typedef DataStoreType data_store_t;
    };

    template < typename T >
    struct is_arg_storage_pair : boost::mpl::false_ {};

    template < typename ArgType, typename DataStoreType >
    struct is_arg_storage_pair< arg_storage_pair< ArgType, DataStoreType > > : boost::mpl::true_ {};

    template < typename T >
    struct is_tmp_arg : std::false_type {};

    template < uint_t I, typename DataStoreType, typename Location >
    struct is_tmp_arg< arg< I, DataStoreType, Location, true > > : std::true_type {};

    template < typename ArgType, typename DataStoreType >
    struct is_tmp_arg< arg_storage_pair< ArgType, DataStoreType > > : is_tmp_arg< ArgType > {};

    /**
     * Type to create placeholders for data fields.
     *
     * There is a specialization for the case in which T is a temporary.
     * The default version applies to all the storage classes (including
     * user-defined ones used via the global-accessor)
     *
     * @tparam I Integer index (unique) of the data field to identify it
     * @tparam DataStoreType The type of the storage used to store data
     * @tparam LocationType the location type of the storage of the placeholder
     * @tparam Temporary determines whether the placeholder holds a temporary or normal storage
     */
    template < uint_t I, typename DataStoreType, typename LocationType, bool Temporary >
    struct arg {
        GRIDTOOLS_STATIC_ASSERT((is_location_type< LocationType >::value),
            "The third template argument of a placeholder must be a location_type");
        typedef DataStoreType data_store_t;

        typedef LocationType location_t;
        typedef arg type;

        constexpr static bool is_temporary = Temporary;

        template < typename Arg >
        arg_storage_pair< arg, DataStoreType > operator=(Arg &&arg) {
            return {std::forward< Arg >(arg)};
        }

        static void info(std::ostream &out_s) {
#ifdef VERBOSE
            out_s << "Arg on real storage with index " << I;
#endif
        }
    };

    namespace _impl {

        // metafunction that replaces the ID of a storage_info type to the new value
        template < unsigned Id, typename T >
        struct tmp_storage_info;

        template < template < unsigned, typename, typename, typename > class StorageInfo,
            unsigned Id,
            unsigned OldId,
            typename Layout,
            typename Halo,
            typename Alignment >
        struct tmp_storage_info< Id, StorageInfo< OldId, Layout, Halo, Alignment > > {
            using type = StorageInfo< Id, Layout, Halo, Alignment >;
        };

        // replace the storage_info ID contained in a given storage with the new value
        template < unsigned Id, typename T >
        struct tmp_data_store;

        template < unsigned Id, typename Storage, typename StorageInfo >
        struct tmp_data_store< Id, data_store< Storage, StorageInfo > > {
            using type = data_store< Storage, typename tmp_storage_info< Id, StorageInfo >::type >;
        };

        template < unsigned Id, typename DataStore, unsigned... N >
        struct tmp_data_store< Id, data_store_field< DataStore, N... > > {
            using type = data_store_field< typename tmp_data_store< Id, DataStore >::type, N... >;
        };

        template < unsigned Id, typename DataStore >
        struct tmp_data_store< Id, std::vector< DataStore > > {
            using type = std::vector< typename tmp_data_store< Id, DataStore >::type >;
        };

        template < typename Location >
        struct tmp_storage_info_id;
        template < int_t I, ushort_t NColors >
        struct tmp_storage_info_id< location_type< I, NColors > > : std::integral_constant< unsigned, -NColors > {};
    }
    /** alias template that provides convenient tmp arg declaration.
     *
     *  Here we force tmp storages to share storage info type. To achieve this we substitute the storage info ID
     *  to one that is in the reserved range (close to max unsigned).
     *  TODO(anstaf): replace storage info IDs to tags to avoid having reserved range.
     */
    template < uint_t I, typename DataStoreType, typename Location = enumtype::default_location_type >
    using tmp_arg = arg< I,
        typename _impl::tmp_data_store< _impl::tmp_storage_info_id< Location >::value, DataStoreType >::type,
        Location,
        true >;

    template < typename T >
    struct arg_index;

    /** true in case of non temporary storage arg*/
    template < uint_t I, typename DataStoreType, typename Location, bool Temporary >
    struct arg_index< arg< I, DataStoreType, Location, Temporary > > : boost::mpl::integral_c< int, I > {};

    template < typename T >
    struct is_storage_arg : boost::mpl::false_ {};

    template < uint_t I, typename DataStoreType, typename Location, bool Temporary >
    struct is_storage_arg< arg< I, DataStoreType, Location, Temporary > > : is_storage< DataStoreType > {};

    /**
     * @struct arg_hods_data_field
     * metafunction that determines if an arg type is holding the storage type of a data field
     */
    template < typename Arg >
    struct arg_holds_data_field;

    template < uint_t I, typename DataStoreType, typename Location, bool Temporary >
    struct arg_holds_data_field< arg< I, DataStoreType, Location, Temporary > > : is_data_store_field< DataStoreType > {
    };

} // namespace gridtools