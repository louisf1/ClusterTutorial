# FindgRPC.cmake — locates gRPC via pkg-config and defines gRPC::grpc++
#
# Debian's libgrpc-dev (this system ships 1.30.2) only installs pkg-config
# files (grpc.pc, grpc++.pc); it does not provide a CMake config package
# (gRPCConfig.cmake), so plain find_package(gRPC) fails with "no package
# configuration file". This module bridges pkg-config's grpc++.pc into the
# gRPC::grpc++ imported target that CMakeLists.txt links against.

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_GRPCXX QUIET grpc++)

find_path(GRPC_INCLUDE_DIR
    NAMES grpcpp/grpcpp.h
    HINTS ${PC_GRPCXX_INCLUDE_DIRS}
)

find_library(GRPC_GRPCXX_LIBRARY
    NAMES grpc++
    HINTS ${PC_GRPCXX_LIBRARY_DIRS}
)
find_library(GRPC_GRPC_LIBRARY
    NAMES grpc
    HINTS ${PC_GRPCXX_LIBRARY_DIRS}
)
find_library(GRPC_GPR_LIBRARY
    NAMES gpr
    HINTS ${PC_GRPCXX_LIBRARY_DIRS}
)

# The protoc plugin used to generate *.grpc.pb.{h,cc} — ships in Debian's
# protobuf-compiler-grpc package (/usr/bin/grpc_cpp_plugin), consumed via
# $<TARGET_FILE:gRPC::grpc_cpp_plugin> by protobuf_generate(... LANGUAGE grpc ...).
find_program(GRPC_CPP_PLUGIN_EXECUTABLE
    NAMES grpc_cpp_plugin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gRPC
    REQUIRED_VARS GRPC_GRPCXX_LIBRARY GRPC_GRPC_LIBRARY GRPC_GPR_LIBRARY GRPC_INCLUDE_DIR GRPC_CPP_PLUGIN_EXECUTABLE
    VERSION_VAR PC_GRPCXX_VERSION
)

if(gRPC_FOUND AND NOT TARGET gRPC::grpc++)
    add_library(gRPC::grpc++ UNKNOWN IMPORTED)
    set_target_properties(gRPC::grpc++ PROPERTIES
        IMPORTED_LOCATION "${GRPC_GRPCXX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${GRPC_GRPC_LIBRARY};${GRPC_GPR_LIBRARY}"
    )
endif()

if(gRPC_FOUND AND NOT TARGET gRPC::grpc_cpp_plugin)
    add_executable(gRPC::grpc_cpp_plugin IMPORTED)
    set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES
        IMPORTED_LOCATION "${GRPC_CPP_PLUGIN_EXECUTABLE}"
    )
endif()

mark_as_advanced(GRPC_INCLUDE_DIR GRPC_GRPCXX_LIBRARY GRPC_GRPC_LIBRARY GRPC_GPR_LIBRARY GRPC_CPP_PLUGIN_EXECUTABLE)
