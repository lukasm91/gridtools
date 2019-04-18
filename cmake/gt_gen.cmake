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
    add_library(${new_target} OBJECT)

    foreach(prop ${CMAKE_PROPERTY_LIST})
        get_target_property(target_prop ${old_target} ${prop})
        if (NOT "${target_prop}" MATCHES "-NOTFOUND$")
            set_target_properties(${new_target} PROPERTIES ${prop} "${target_prop}")
        endif ()
    endforeach()
endfunction()

function(gt_use_dumped_data target)

    target_compile_definitions(${target}  PRIVATE GT_DUMP_DATA_FOLDER=${GT_DUMP_DATA_FOLDER})
    target_include_directories(${target}  PUBLIC ${GT_DUMP_SOURCE_DIR})
    target_link_libraries(${target} stdc++fs)

    if (TARGET gridtools)
        target_include_directories(gridtools INTERFACE 
            $<BUILD_INTERFACE:${GT_DUMP_SOURCE_DIR}>)
    else()
        target_include_directories(GridTools::gridtools INTERFACE 
            $<BUILD_INTERFACE:${GT_DUMP_SOURCE_DIR}>)
    endif()

    if (GT_DUMP_GENERATE_DATA)
        target_compile_definitions(${target} PRIVATE GT_DUMP_GENERATE_DATA)
        target_link_libraries(${target} GridTools::gt_dump_interface)
    else()
        get_target_property(target_sources ${target} SOURCES)
        foreach(source ${target_sources})
            get_filename_component(source_path ${source} ABSOLUTE)
            get_filename_component(source_name_we ${source} NAME_WE)
            string(REPLACE "/" "_" prefix ${source_path})

            gt_copy_target(${target}_${source_name_we} ${target})
            target_sources(${target}_${source_name_we} PRIVATE ${source})

            file(GLOB_RECURSE template_files CONFIGURE_DEPENDS ${GT_DUMP_SOURCE_DIR}/templates/*.j2 )

            file(GLOB generated_files RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS ${GT_DUMP_DATA_FOLDER}/${prefix}*)
            foreach(generated_file_source ${generated_files})
                get_filename_component(generated_file_name ${generated_file_source} NAME)
                get_filename_component(generated_file_source_path ${generated_file_source} ABSOLUTE)
                set(generated_file ${GT_DUMP_DATA_FOLDER}/generated/${generated_file_name})
                get_filename_component(interface_py_dir ${INTERFACE_PY} PATH)

                add_custom_command(OUTPUT ${generated_file}
                    COMMAND mkdir -p ${GT_DUMP_DATA_FOLDER}/generated/
                    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH="${interface_py_dir}"
                        python ${GT_DUMP_SOURCE_DIR}/generate_code.py ${generated_file_source_path} ${generated_file}
                    COMMENT "Generate ${generated_file}"
                    MAIN_DEPENDENCY ${generated_file_source}
                    DEPENDS
                    ${GT_DUMP_SOURCE_DIR}/generate_code.py
                        ${template_files}
                        ${generated_file_source_path}
                        gt_dump_python
                        ${GT_DUMP_SOURCE_DIR}/proto/interface.proto
                    )
                # we need to manually add this dependency because this include is dependent on a macro define which
                # CMake cannot resolve
                set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${generated_file})
            endforeach()

            target_compile_definitions(${target}_${source_name_we} PRIVATE GT_DUMP_PREFIX=${prefix})
            target_include_directories(${target}_${source_name_we} PRIVATE ${GT_DUMP_DATA_FOLDER}/generated)
            target_link_libraries(${target} ${target}_${source_name_we})

        endforeach()

        set_target_properties(${target} PROPERTIES SOURCES "")
    endif()
endfunction()
