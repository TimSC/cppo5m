cmake_minimum_required(VERSION 3.13)
project(cppo5m VERSION 2.0.0 LANGUAGES C CXX)

# Use from another CMake project with either
#   add_subdirectory(cppo5m)                      or
#   find_package(cppo5m REQUIRED)                 after installing
# and then
#   target_link_libraries(yours PRIVATE cppo5m::cppo5m)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Top level builds get the tool, examples and tests; as a subdirectory only the library
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CPPO5M_TOP_LEVEL ON)
else()
	set(CPPO5M_TOP_LEVEL OFF)
endif()
option(CPPO5M_BUILD_TOOLS "Build o5mconvert" ${CPPO5M_TOP_LEVEL})
option(CPPO5M_BUILD_EXAMPLES "Build the example programs" ${CPPO5M_TOP_LEVEL})
option(CPPO5M_BUILD_TESTS "Build the tests" ${CPPO5M_TOP_LEVEL})
option(CPPO5M_PYTHON "Build PySink for use from a Python extension" OFF)

find_package(EXPAT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Protobuf REQUIRED)

set(CPPO5M_PUBLIC_HEADERS
	cppo5m.h model.h handler.h sink.h pysink.h decoder.h encoder.h o5m.h osmxml.h
	osmchangexml.h pbf.h filters.h io.h varint.h fixeddeque.h)

add_library(cppo5m
	model.cpp sink.cpp decoder.cpp varint.cpp o5m.cpp osmxml.cpp osmchangexml.cpp
	pbf.cpp filters.cpp io.cpp
	pbf/fileformat.pb.cc pbf/osmformat.pb.cc
	iso8601lib/iso8601.c)
add_library(cppo5m::cppo5m ALIAS cppo5m)
target_include_directories(cppo5m PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/cppo5m>)
target_link_libraries(cppo5m PUBLIC protobuf::libprotobuf PRIVATE EXPAT::EXPAT ZLIB::ZLIB)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set_source_files_properties(
		model.cpp sink.cpp decoder.cpp varint.cpp o5m.cpp osmxml.cpp osmchangexml.cpp pbf.cpp filters.cpp io.cpp
		PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-unused-parameter")
	# Generated protobuf sources produce warnings we cannot fix
	set_source_files_properties(pbf/fileformat.pb.cc pbf/osmformat.pb.cc PROPERTIES COMPILE_OPTIONS "-w")
endif()

if(CPPO5M_PYTHON)
	find_package(Python3 REQUIRED COMPONENTS Development)
	target_sources(cppo5m PRIVATE pysink.cpp)
	target_compile_definitions(cppo5m PUBLIC PYTHON_AWARE)
	target_link_libraries(cppo5m PUBLIC Python3::Module)
endif()

if(CPPO5M_BUILD_TOOLS)
	add_executable(o5mconvert tools/o5mconvert.cpp)
	target_link_libraries(o5mconvert PRIVATE cppo5m)
endif()

if(CPPO5M_BUILD_EXAMPLES)
	foreach(example example examplestream exampleosmchange)
		add_executable(${example} examples/${example}.cpp)
		target_link_libraries(${example} PRIVATE cppo5m)
	endforeach()
endif()

if(CPPO5M_BUILD_TESTS)
	enable_testing()
	add_executable(test_cppo5m tests/test_cppo5m.cpp)
	target_link_libraries(test_cppo5m PRIVATE cppo5m ZLIB::ZLIB)
	add_executable(test_fixeddeque tests/test_fixeddeque.cpp)
	# The tests read sample files by a path relative to the source directory
	add_test(NAME cppo5m COMMAND test_cppo5m WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(NAME fixeddeque COMMAND test_fixeddeque)
endif()

# ******* Install *******

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS cppo5m EXPORT cppo5mTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CPPO5M_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppo5m)
if(CPPO5M_BUILD_TOOLS)
	install(TARGETS o5mconvert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(EXPORT cppo5mTargets NAMESPACE cppo5m:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppo5m)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cppo5mConfig.cmake
"include(CMakeFindDependencyMacro)
find_dependency(Protobuf)
find_dependency(EXPAT)
find_dependency(ZLIB)
include(\"\${CMAKE_CURRENT_LIST_DIR}/cppo5mTargets.cmake\")
")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cppo5mConfigVersion.cmake
	COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cppo5mConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/cppo5mConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppo5m)
