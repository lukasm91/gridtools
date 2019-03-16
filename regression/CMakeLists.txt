set(GT_DUMP_DATA_FOLDER "/home/lukas/documents/work/gridtools4/build/regression/data")
set(GT_DUMP_GENERATE_DATA OFF)

function (gt_copy_target new_target old_target)
    set(CMAKE_PROPERTY_LIST
        BINARY_DIR
        C_EXTENSIONS
        C_STANDARD
        C_STANDARD_REQUIRED
        COMPILE_DEFINITIONS
        COMPILE_FEATURES
        COMPILE_FLAGS
        COMPILE_OPTIONS
        CUDA_SEPARABLE_COMPILATION
        CUDA_RESOLVE_DEVICE_SYMBOLS
        CUDA_EXTENSIONS
        CUDA_STANDARD
        CUDA_STANDARD_REQUIRED
        CXX_EXTENSIONS
        CXX_STANDARD
        CXX_STANDARD_REQUIRED
        DEFINE_SYMBOL
        ENABLE_EXPORTS
        EXCLUDE_FROM_ALL
        HAS_CXX
        INCLUDE_DIRECTORIES
        INSTALL_NAME_DIR
        INSTALL_RPATH
        INSTALL_RPATH_USE_LINK_PATH
        INTERFACE_AUTOUIC_OPTIONS
        INTERFACE_COMPILE_DEFINITIONS
        INTERFACE_COMPILE_FEATURES
        INTERFACE_COMPILE_OPTIONS
        INTERFACE_INCLUDE_DIRECTORIES
        INTERFACE_LINK_DEPENDS
        INTERFACE_LINK_DIRECTORIES
        INTERFACE_LINK_LIBRARIES
        INTERFACE_LINK_OPTIONS
        INTERFACE_POSITION_INDEPENDENT_CODE
        INTERFACE_SOURCES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        LABELS
        LINK_DEPENDS_NO_SHARED
        LINK_DEPENDS
        LINKER_LANGUAGE
        LINK_DIRECTORIES
        LINK_FLAGS
        LINK_INTERFACE_LIBRARIES
        LINK_INTERFACE_MULTIPLICITY
        LINK_LIBRARIES
        LINK_OPTIONS
        LINK_SEARCH_END_STATIC
        LINK_SEARCH_START_STATIC
        LINK_WHAT_YOU_USE
        POSITION_INDEPENDENT_CODE
        RULE_LAUNCH_COMPILE
        RULE_LAUNCH_CUSTOM
        RULE_LAUNCH_LINK
        SOURCE_DIR
        )

    # duplicate target
    get_target_property(target_sources ${old_target} SOURCES)
    add_library(${new_target} OBJECT ${target_sources} )

    foreach(prop ${CMAKE_PROPERTY_LIST})
        get_target_property(target_prop ${old_target} ${prop})
        if (NOT "${target_prop}" MATCHES "-NOTFOUND$")
            set_target_properties(${new_target} PROPERTIES ${prop} "${target_prop}")
        endif ()
    endforeach()
    # target_compile_options(${new_target}_COMPILED PRIVATE "-xc++-cpp-output")
endfunction()

function(gt_use_dumped_data target)
    target_compile_definitions(${target}  PRIVATE GT_DUMP_DATA_FOLDER="${GT_DUMP_DATA_FOLDER}")
    target_include_directories(${target}  PUBLIC ../gt_dump)
    target_link_libraries(${target} stdc++fs)

    if (GT_DUMP_GENERATE_DATA)
        target_compile_definitions(${target} PRIVATE GT_DUMP_GENERATE_DATA)
    else()
        get_target_property(target_sources ${target} SOURCES)
        set(new_sources)
        foreach(source ${target_sources})
            get_filename_component(source_path ${source} ABSOLUTE)
            get_filename_component(source_name_we ${source} NAME_WE)
            get_filename_component(source_name ${source} NAME)
            get_filename_component(source_name_ext ${source_path} EXT)
            get_filename_component(source_dir ${source_path} DIRECTORY)

            set(source_copy ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/gt_gen/${target}/${source_name})
            add_custom_command(OUTPUT ${source_copy}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/gt_gen
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/gt_gen/${target}
                COMMAND ${CMAKE_COMMAND} -E copy ${source_path} ${source_copy}
                DEPENDS ${source_path}
                COMMENT "Copy original code"
                )

            gt_copy_target(${target}_${source_name_we}_PREPROCESSED ${target})
            set_target_properties(${target}_${source_name_we}_PREPROCESSED PROPERTIES SOURCES "${source_copy}")
            target_compile_options(${target}_${source_name_we}_PREPROCESSED PRIVATE "-E")
            target_include_directories(${target}_${source_name_we}_PREPROCESSED PRIVATE ${source_dir})

            set(source_preprocessed ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/gt_gen/${target}/${source_name_we}_preprocessed${source_name_ext})
            add_custom_command(OUTPUT ${source_preprocessed}
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:${target}_${source_name_we}_PREPROCESSED> ${source_preprocessed}
                COMMAND python ${CMAKE_CURRENT_LIST_DIR}/generate_code.py ${source_preprocessed}
                DEPENDS ${target}_${source_name_we}_PREPROCESSED $<TARGET_OBJECTS:${target}_${source_name_we}_PREPROCESSED>
                COMMENT "Transform preprocessed file"
                )

            gt_copy_target(${target}_${source_name_we}_FINAL ${target})
            set_target_properties(${target}_${source_name_we}_FINAL PROPERTIES SOURCES "${source_preprocessed}")
            target_compile_options(${target}_${source_name_we}_FINAL PRIVATE "-xc++-cpp-output")
            target_include_directories(${target}_${source_name_we}_FINAL PRIVATE ${source_dir})

            target_link_libraries(${target} ${target}_${source_name_we}_FINAL)

        endforeach()

        set_target_properties(${target} PROPERTIES SOURCES "${new_sources}")
    endif()
endfunction()

if( COMPONENT_STENCIL_COMPOSITION )

    if(GT_TESTS_ICOSAHEDRAL_GRID)
        add_subdirectory(icosahedral)
    else()

      set(SOURCES_PERFTEST
          horizontal_diffusion
          horizontal_diffusion_fused
          simple_hori_diff
          copy_stencil
          vertical_advection_dycore
          advection_pdbott_prepare_tracers
          )
      set(SOURCES
          ${SOURCES_PERFTEST}
          boundary-condition
          laplacian positional_stencil
          tridiagonal
          alignment
          extended_4D
          expandable_parameters
          expandable_parameters_single_kernel
          horizontal_diffusion_functions
          )

      # special target for executables which are used from performance benchmarks
      add_custom_target(perftests)

      if(GT_ENABLE_TARGET_X86)
        foreach(srcfile IN LISTS SOURCES)
          add_executable(${srcfile}_x86_naive   ${srcfile}.cpp)
          target_link_libraries(${srcfile}_x86_naive regression_main GridToolsTestX86)
          target_compile_definitions(${srcfile}_x86_naive PRIVATE GT_BACKEND_STRATEGY_NAIVE)

          add_executable(${srcfile}_x86_block   ${srcfile}.cpp)
          target_link_libraries(${srcfile}_x86_block regression_main GridToolsTestX86)

          gridtools_add_test(
              NAME tests.${srcfile}_x86_block_12_33_61
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_x86_block> 12 33 61
              LABELS regression_x86 target_x86
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )
          gridtools_add_test(
              NAME tests.${srcfile}_x86_block_23_11_43
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_x86_block> 23 11 43
              LABELS regression_x86 target_x86
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )
          gridtools_add_test(
              NAME tests.${srcfile}_x86_naive_12_33_61
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_x86_naive> 12 33 21
              LABELS regression_x86 target_x86
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )
          gridtools_add_test(
              NAME tests.${srcfile}_x86_naive_23_11_43
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_x86_naive> 23 11 43
              LABELS regression_x86 target_x86
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )

          if (srcfile IN_LIST SOURCES_PERFTEST)
              add_dependencies(perftests ${srcfile}_x86_block)
          endif()
        endforeach(srcfile)

        if( GT_USE_MPI )
            add_custom_mpi_x86_test(TARGET copy_stencil_parallel NPROC 4 SOURCES copy_stencil_parallel.cpp)

            ## The next executable is not made into a test since it does not validate.
            ## The numerics need to be fixed (this is a task to be undertaken in the future)
            add_executable(shallow_water_enhanced_x86  shallow_water_enhanced.cpp)
            target_link_libraries(shallow_water_enhanced_x86 gtest_main gcl GridToolsTestX86)
        endif()
      endif(GT_ENABLE_TARGET_X86)

      if(GT_ENABLE_TARGET_MC)
        foreach(srcfile IN LISTS SOURCES)
          add_executable(${srcfile}_mc   ${srcfile}.cpp)
          target_link_libraries(${srcfile}_mc regression_main GridToolsTestMC)

          gridtools_add_test(
              NAME tests.${srcfile}_mc_12_33_61
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_mc> 12 33 61
              LABELS regression_mc target_mc
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )
          gridtools_add_test(
              NAME tests.${srcfile}_mc_23_11_43
              SCRIPT ${TEST_SCRIPT}
              COMMAND $<TARGET_FILE:${srcfile}_mc> 23 11 43
              LABELS regression_mc target_mc
              ENVIRONMENT ${TEST_HOST_ENVIRONMENT}
              )
          if (srcfile IN_LIST SOURCES_PERFTEST)
            add_dependencies(perftests ${srcfile}_mc)
          endif()
        endforeach(srcfile)

        if( GT_USE_MPI )
            add_custom_mpi_mc_test(TARGET copy_stencil_parallel NPROC 4 SOURCES copy_stencil_parallel.cpp)

            ## The next executable is not made into a test since it does not validate.
            ## The numerics need to be fixed (this is a task to be undertaken in the future)
            add_executable(shallow_water_enhanced_mc  shallow_water_enhanced.cpp)
            target_link_libraries(shallow_water_enhanced_mc  gtest_main gcl GridToolsTestMC)
        endif()
    endif(GT_ENABLE_TARGET_MC)

    if(GT_ENABLE_TARGET_CUDA)

       foreach(srcfile IN LISTS SOURCES)
           add_executable( ${srcfile}_cuda ${srcfile}.cu)
           target_link_libraries(${srcfile}_cuda regression_main GridToolsTestCUDA)

           gridtools_add_test(
               NAME tests.${srcfile}_cuda_47_38_61
               SCRIPT ${TEST_SCRIPT}
               COMMAND $<TARGET_FILE:${srcfile}_cuda> 47 38 61
               LABELS regression_cuda target_cuda
               ENVIRONMENT ${TEST_CUDA_ENVIRONMENT}
               )
           gridtools_add_test(
               NAME tests.${srcfile}_cuda_63_38_10
               SCRIPT ${TEST_SCRIPT}
               COMMAND $<TARGET_FILE:${srcfile}_cuda> 63 38 10
               LABELS regression_cuda target_cuda
               ENVIRONMENT ${TEST_CUDA_ENVIRONMENT}
               )
           if (srcfile IN_LIST SOURCES_PERFTEST)
             add_dependencies(perftests ${srcfile}_cuda)
           endif()

           if (GT_CUDA_PTX_GENERATION)
               add_library( ${srcfile}_cuda_ptx OBJECT ${srcfile}.cu)
               target_link_libraries(${srcfile}_cuda_ptx regression_main GridToolsTestCUDA)
               set_target_properties(${srcfile}_cuda_ptx PROPERTIES CUDA_PTX_COMPILATION ON)
           endif()
       endforeach(srcfile)

       if( GT_USE_MPI )
           add_custom_mpi_gpu_test(TARGET copy_stencil_parallel NPROC 4 SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/copy_stencil_parallel.cu )

           ## The next executable is not made into a test since it does not validate.
           ## The numerics need to be fixed (this is a task to be undertaken in the future)
           add_executable(shallow_water_enhanced_cuda shallow_water_enhanced.cu)
           target_link_libraries(shallow_water_enhanced_cuda gtest_main gcl GridToolsTestCUDA)
        endif()
    endif(GT_ENABLE_TARGET_CUDA)

    if ( COMPONENT_C_BINDINGS )
        add_subdirectory( c_bindings )
    endif()

    if ( COMPONENT_GCL )
        add_subdirectory( communication )
    endif()

  endif(GT_TESTS_ICOSAHEDRAL_GRID)
endif( COMPONENT_STENCIL_COMPOSITION )


######

gt_use_dumped_data(vertical_advection_dycore_x86_block)
gt_use_dumped_data(vertical_advection_dycore_cuda)
gt_use_dumped_data(copy_stencil_x86_block)

