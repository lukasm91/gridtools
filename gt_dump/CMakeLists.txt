find_package(Protobuf REQUIRED)

protobuf_generate_python(INTERFACE_PY proto/interface.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/interface.proto)

add_library(gt_dump_interface ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(gt_dump_interface PRIVATE ${Protobuf_LIBRARIES})
get_filename_component(proto_header_dir ${PROTO_HDRS} DIRECTORY)
target_include_directories(gt_dump_interface PUBLIC ${proto_header_dir})

add_custom_target(gt_dump_python ALL
    DEPENDS ${INTERFACE_PY}
    )
set(INTERFACE_PY ${INTERFACE_PY} CACHE STRING "Path to protobuf python interface")

