#pragma once

#include <gridtools/stencil_composition/accessor.hpp>
#include <gridtools/stencil_composition/make_computation.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/core/demangle.hpp>
#include <experimental/filesystem>
#include <fstream>

#include "interface.pb.h"

#ifndef GT_DUMP_GENERATED_CODE
// clang-format off
#define GT_DUMP_GENERATED_CODE(name) <gt_dump/dummy.hpp> \
// clang-format on
#endif

#ifndef GT_DUMP_IDENTIFIER
#define GT_DUMP_IDENTIFIER(name) \
    (std::string(BOOST_PP_STRINGIZE(GT_DUMP_DATA_FOLDER)) + "/" + boost::replace_all_copy(std::string(__BASE_FILE__ "__" #name), "/", "_"))
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

        struct add_param_f {
            gt_gen::Stage& stage;

            template <uint_t ID>
            void operator()(global_accessor<ID>) const {
                auto accessor = stage.add_accessors();

                accessor->mutable_normal_accessor()->set_id(ID);
                accessor->mutable_normal_accessor()->set_intent(gt_gen::NormalAccessor_Intent_READ_ONLY);
                accessor->mutable_normal_accessor()->set_dimension(3);

                gt_gen::Extent *extent = accessor->mutable_normal_accessor()->mutable_extent();
                extent->set_iminus(0);
                extent->set_iplus(0);
                extent->set_jminus(0);
                extent->set_jplus(0);
                extent->set_kminus(0);
                extent->set_kplus(0);

                // accessor->mutable_global_accessor()->set_id(ID);
            }
            template <uint_t ID, intent Intent, typename Extent, size_t Dimension>
            void operator()(accessor<ID, Intent, Extent, Dimension>) const {
                auto accessor = stage.add_accessors();

                accessor->mutable_normal_accessor()->set_id(ID);
                accessor->mutable_normal_accessor()->set_intent(Intent == intent::inout
                                                                    ? gt_gen::NormalAccessor_Intent_READ_WRITE
                                                                    : gt_gen::NormalAccessor_Intent_READ_ONLY);
                accessor->mutable_normal_accessor()->set_dimension(Dimension);

                gt_gen::Extent *extent = accessor->mutable_normal_accessor()->mutable_extent();
                extent->set_iminus(Extent::iminus::value);
                extent->set_iplus(Extent::iplus::value);
                extent->set_jminus(Extent::jminus::value);
                extent->set_jplus(Extent::jplus::value);
                extent->set_kminus(Extent::kminus::value);
                extent->set_kplus(Extent::kplus::value);
            }
        };
        struct add_arg_f {
            gt_gen::Multistage::IndependentStage *stage;
            gt_gen::Computation *computation;
            template <class Tag, typename DataStoreType, typename LocationType, bool Temporary>
            void operator()(plh<Tag, DataStoreType, LocationType, Temporary>) const {
                using plh = plh<Tag, DataStoreType, LocationType, Temporary>;

                using layout_map = typename DataStoreType::storage_info_t::layout_t;
                /*
                if (layout_map::unmasked_length == 0) {
                    (*computation->mutable_global_params())[get_tag<Tag>::value].set_type(
                        boost::core::demangle(typeid(typename DataStoreType::data_t).name()));
                    auto arg = stage->add_args();
                    arg->set_id(get_tag<Tag>::value);
                    arg->set_arg_type(gt_gen::Multistage::GLOBAL);
                    return;
                }
                */

                auto arg = stage->add_args();
                arg->set_id(get_tag<Tag>::value);
                arg->set_arg_type(Temporary ? gt_gen::Multistage::TEMPORARY : gt_gen::Multistage::NORMAL);

                if (Temporary) {
                    if (not computation->mutable_temporaries()->count(get_tag<Tag>::value)) {
                        (*computation->mutable_temporaries())[get_tag<Tag>::value].set_type(
                            boost::core::demangle(typeid(typename DataStoreType::data_t).name()));

                        for (int i = 0; i < layout_map::masked_length; ++i)
                            (*computation->mutable_temporaries())[get_tag<Tag>::value].add_selector(
                                layout_map{}.at(i) == -1);
                    }
                } else {
                    (*computation->mutable_fields()->mutable_args())[get_tag<Tag>::value].set_type(
                        boost::core::demangle(typeid(typename DataStoreType::data_t).name()));
                    (*computation->mutable_fields()->mutable_args())[get_tag<Tag>::value].set_kind(
                        DataStoreType::storage_info_t::id);

                    if (not computation->mutable_fields()->mutable_kinds()->count(DataStoreType::storage_info_t::id))
                        for (int i = 0; i < layout_map::masked_length; ++i)
                            (*computation->mutable_fields()->mutable_kinds())[DataStoreType::storage_info_t::id]
                                .add_layout(layout_map{}.at(i));
                }
            }
        };
        template <class Functor, class DefaultInterval>
        struct find_esf_interval {
            template <class Index, class = int>
            struct deduce_interval {
                using type = int; // none
            };
            template <class Index>
            struct deduce_interval<Index,
                enable_if_t<!_impl::is_interval_overload_defined<Functor, Index>::value && has_apply<Functor>::value,
                    int>> {
                using type = std::pair<DefaultInterval, std::false_type>;
            };
            template <class Index>
            struct deduce_interval<Index,
                enable_if_t<_impl::is_interval_overload_defined<Functor, Index>::value, int>> {
                using interval = typename _impl::find_interval<Functor, Index>::type;
                using type = std::pair<interval, std::true_type>;
            };

            template <class Index>
            using apply = typename deduce_interval<Index>::type;
        };
        template <typename EsfFunction, typename Axis>
        struct find_intervals {
            // produce the list of all level_indices that the give interval has
            using from_index_t = GT_META_CALL(level_to_index, typename Axis::FromLevel);
            using to_index_t = GT_META_CALL(level_to_index, typename Axis::ToLevel);
            using nums_t = GT_META_CALL(meta::make_indices_c, to_index_t::value - from_index_t::value + 1);
            using indices_t = GT_META_CALL(
                meta::transform, (_impl::make_level_index<from_index_t>::template apply, nums_t));

            using type = GT_META_CALL(meta::dedup,
                (GT_META_CALL(meta::transform, (find_esf_interval<EsfFunction, Axis>::template apply, indices_t))));
        };
        struct add_interval_f {
            gt_gen::Stage& stage;

            void operator()(int) const {}
            template <typename IntervalPair>
            void operator()(IntervalPair) const {

                auto interval = stage.add_intervals();
                interval->mutable_interval()->mutable_begin()->set_splitter(
                    IntervalPair::first_type::FromLevel::splitter);
                interval->mutable_interval()->mutable_begin()->set_offset(IntervalPair::first_type::FromLevel::offset);

                interval->mutable_interval()->mutable_end()->set_splitter(IntervalPair::first_type::ToLevel::splitter);
                interval->mutable_interval()->mutable_end()->set_offset(IntervalPair::first_type::ToLevel::offset);

                if (IntervalPair::second_type::value)
                    interval->set_overload(gt_gen::StageInterval::INTERVAL);
                else
                    interval->set_overload(gt_gen::StageInterval::NO_INTERVAL);
            }
        };
        template <typename Axis>
        struct add_independent_esf_sequence_f {
            gt_gen::Multistage::DependentStageList *dependent_stages;
            gt_gen::Computation *computation;
            template <typename Esf>
            void operator()() const {
                auto stage_name = boost::core::demangle(typeid(typename Esf::esf_function).name());
                auto independent_stage = dependent_stages->add_independent_stages();
                independent_stage->set_name(stage_name);
                for_each<typename Esf::args_t>(add_arg_f{independent_stage, computation});

                gt_gen::Stage stage;
                for_each<typename find_intervals<typename Esf::esf_function, Axis>::type>(
                    add_interval_f{stage});

                for_each<copy_into_variadic<typename Esf::esf_function::param_list, std::tuple<>>>(
                    add_param_f{stage});

                computation->mutable_stages()->insert({stage_name, stage});
            }
        };
        template <typename Axis>
        struct add_dependent_esf_sequence_f {
            gt_gen::Multistage *multistage;
            gt_gen::Computation *computation;
            template <typename Esf, enable_if_t<is_independent<Esf>::value, int> = 0>
            void operator()() const {
                auto dependent_stages = multistage->add_dependent_stages();
                for_each_type<typename Esf::esf_list>(
                    add_independent_esf_sequence_f<Axis>{dependent_stages, computation});
            }
            template <typename Esf, enable_if_t<!is_independent<Esf>::value, int> = 0>
            void operator()() const {
                auto dependent_stages = multistage->add_dependent_stages();
                add_independent_esf_sequence_f<Axis>{dependent_stages, computation}.template operator()<Esf>();
            }
        };
        struct add_cache_sequence_f {
            gt_gen::Multistage *multistage;
            template <typename Cache>
            void operator()() const {
                if (is_ij_cache<Cache>::value) {
                    auto ij_cache = multistage->add_ij_caches();
                    ij_cache->set_id(get_tag_of_plh<cache_parameter<Cache>>::value);
                    ij_cache->set_temporary(is_temporary_plh<cache_parameter<Cache>>::value);
                    ij_cache->set_type(
                        boost::core::demangle(typeid(typename cache_parameter<Cache>::data_store_t::data_t).name()));

                } else if (is_k_cache<Cache>::value) {
                    auto k_cache = multistage->add_k_caches();
                    k_cache->set_id(get_tag_of_plh<cache_parameter<Cache>>::value);
                    k_cache->set_temporary(is_temporary_plh<cache_parameter<Cache>>::value);
                    k_cache->set_fill(is_filling_cache<Cache>::value);
                    k_cache->set_flush(is_flushing_cache<Cache>::value);
                    k_cache->set_type(
                        boost::core::demangle(typeid(typename cache_parameter<Cache>::data_store_t::data_t).name()));
                } else {
                    throw std::runtime_error("Did not recognize cache");
                }
            }
        };
        template <typename Axis>
        struct add_mss_f {
            gt_gen::Computation *computation;
            template <class Mss>
            void operator()() const {
                auto mss = computation->mutable_multistages()->Add();
                if (execute::is_forward<typename Mss::execution_engine_t>::value)
                    mss->set_policy(gt_gen::Multistage::FORWARD);
                else if (execute::is_backward<typename Mss::execution_engine_t>::value)
                    mss->set_policy(gt_gen::Multistage::BACKWARD);
                else
                    mss->set_policy(gt_gen::Multistage::PARALLEL);

                for_each_type<typename Mss::esf_sequence_t>(add_dependent_esf_sequence_f<Axis>{mss, computation});
                for_each_type<typename Mss::cache_sequence_t>(add_cache_sequence_f{mss});
            }
        };

        template <typename Intermediate>
        struct dump_intermediate;
        template <bool IsStateful, class Backend, class Grid, class ArgStoragePairs, class Msses>
        struct dump_intermediate<intermediate<IsStateful, Backend, Grid, ArgStoragePairs, Msses>> {
            void operator()(const std::string &filename) const {
                std::experimental::filesystem::create_directory(
                    std::experimental::filesystem::path(filename).parent_path());
                std::ofstream of(filename, std::ios::out | std::ios::binary | std::ios::trunc);
                gt_gen::Computation computation;
                // the default axis is exclusive while all other intervals are inclusive -> modify
                for_each_type<Msses>(add_mss_f<typename Grid::axis_type::template modify<0, -1>>{&computation});
                computation.set_positional(IsStateful);
                computation.set_offset_limit(Grid::axis_type::offset_limit);

                computation.SerializeToOstream(&of);
                of.close();
            }
        };

        template <typename Intermediate>
        void dump(const std::string &filename) {
            dump_intermediate<Intermediate>{}(filename);
        }

        template <typename Intermediate>
        Intermediate dump_and_return(Intermediate &&intermediate, const std::string &name) {
            dump<Intermediate>(name);
            return std::move(intermediate);
        }
    } // namespace gt_gen_helpers

    /// generator for intermediate
    template <class Backend, class Grid, class Arg, class... Args, enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_computation(std::string const &name, Grid const &grid, Arg &&arg, Args &&... args) GT_AUTO_RETURN(
        gt_gen_helpers::dump_and_return(make_computation<Backend>(grid, std::forward<Arg>(arg), std::forward<Args>(args)...), name));
    template <class Backend, class Grid, class Arg, class... Args, enable_if_t<is_grid<Grid>::value, int> = 0>
    auto make_computation(std::string &&name, Grid const &grid, Arg &&arg, Args &&... args) GT_AUTO_RETURN(
        gt_gen_helpers::dump_and_return(make_computation<Backend>(grid, std::forward<Arg>(arg), std::forward<Args>(args)...), name));

} // namespace gridtools
