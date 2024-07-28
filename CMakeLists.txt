#
# modem-monitor: A WWAN modem monitoring and control daemon
# Copyright (C) 2024, Tyler J. Stachecki
#
# This file is subject to the terms and conditions defined in
# 'LICENSE', which is part of this source code package.
#

cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
enable_language(C)
project(modem-monitor C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CheckCCompilerFlag)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(Netlink REQUIRED)
find_library(MATH_LIBRARY m REQUIRED)
find_package(Systemd REQUIRED)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
#  Set defaults for things that were not prespecified.
# -----------------------------------------------------------------------------
if (NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Project build type" FORCE)
else ()
  set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Project build type" FORCE)
endif ()

# -----------------------------------------------------------------------------
#  Define functions for configuring compilers (checking flags, features, etc.)
# -----------------------------------------------------------------------------
function (ENABLE_C_FLAG_IF_SUPPORTED flag flag_description)
  string(FIND "${CMAKE_C_FLAGS}" "${flag}" flag_already_set)

  if (flag_already_set EQUAL -1)
    CHECK_C_COMPILER_FLAG("-Werror ${flag}" COMPILER_SUPPORTS_${flag_description})

    if (${COMPILER_SUPPORTS_${flag_description}})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
    endif ()
  endif ()
endfunction ()

function (ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG flag flag_description)
  string(FIND "${CMAKE_C_FLAGS}" "${flag}" flag_already_set)

  if (flag_already_set EQUAL -1)
    CHECK_C_COMPILER_FLAG("-Werror ${flag}" COMPILER_SUPPORTS_${flag_description})

    if (${COMPILER_SUPPORTS_${flag_description}})
      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${flag}" PARENT_SCOPE)
    endif ()
  endif ()
endfunction ()

function (ENABLE_C_FLAG_IF_SUPPORTED_AND_NOT_DEBUG flag flag_description)
  string(FIND "${CMAKE_C_FLAGS}" "${flag}" flag_already_set)

  if (flag_already_set EQUAL -1)
    CHECK_C_COMPILER_FLAG("-Werror ${flag}" COMPILER_SUPPORTS_${flag_description})

    if (${COMPILER_SUPPORTS_${flag_description}})
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${flag}" PARENT_SCOPE)
      set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${flag}" PARENT_SCOPE)
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${flag}" PARENT_SCOPE)
    endif ()
  endif ()
endfunction ()

function (ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE flag flag_description)
  string(FIND "${CMAKE_C_FLAGS}" "${flag}" flag_already_set)

  if (flag_already_set EQUAL -1)
    CHECK_C_COMPILER_FLAG("-Werror ${flag}" COMPILER_SUPPORTS_${flag_description})

    if (${COMPILER_SUPPORTS_${flag_description}})
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${flag}" PARENT_SCOPE)
      set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${flag}" PARENT_SCOPE)
    endif ()
  endif ()
endfunction ()

# -----------------------------------------------------------------------------
#  Configure Clang/GCC/ICC if one of them is being used.
# -----------------------------------------------------------------------------
if (CMAKE_C_COMPILER_ID MATCHES GNU OR
    CMAKE_C_COMPILER_ID MATCHES Clang OR
    CMAKE_C_COMPILER_ID MATCHES Intel)

  # Enable useful warnings if we have them.
  ENABLE_C_FLAG_IF_SUPPORTED(-fdiagnostics-color=auto fdiagnostics_color_auto)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wall Wall)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wextra Wextra)
  ENABLE_C_FLAG_IF_SUPPORTED(-std=c99 std_c99)

  ENABLE_C_FLAG_IF_SUPPORTED(-Waggregate-return Waggregate_return)
  ENABLE_C_FLAG_IF_SUPPORTED(-Warray-bounds Warray_bounds)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wbad-function-cast Wbad_function_cast)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wcast-align Wcast_align)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wcast-align=strict Wcast_align_strict)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wcast-qual Wcast_qual)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wconversion Wconversion)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wdate-time Wdate_time)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wdisabled-optimization Wdisabled_optimization)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wdouble-promotion Wdouble_promotion)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wduplicated-cond Wduplicated_cond)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wfloat-equal Wfloat_equal)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wformat=2 Wformat_2)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wformat-overflow Wformat_overflow)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wformat-signedness Wformat_signedness)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wformat-truncation Wformat_truncation)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wif-not-aligned Wif_not_aligned)

  # ICC gives bogus inline warnings. It seems like it doesn't want to
  # inline any function that contains a shared library call? No thanks.
  if (NOT CMAKE_C_COMPILER_ID MATCHES Intel)
    ENABLE_C_FLAG_IF_SUPPORTED(-Winline Winline)
  endif ()

  ENABLE_C_FLAG_IF_SUPPORTED(-Winit-self Winit_self)
  ENABLE_C_FLAG_IF_SUPPORTED(-Winvalid-pch Winvalid_pch)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wlogical-op Wlogical_op)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wlogical-not-parentheses Wlogical_not_parentheses)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmissing-attributes Wmissing_attributes)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmissing-declarations Wmissing_declarations)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmissing-include-dirs Wmissing_include_dirs)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmissing-prototypes Wmissing_prototypes)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmisleading-indentation Wmisleading_indentation)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmultichar Wmultichar)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wmultistatement-macros Wmultistatement_macros)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wnested-externs Wnested_externs)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wno-unused-parameter Wno_unused_parameter)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wnull-dreference Wnull_dereference)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wold-style-declaration Wold_style_declaration)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wold-style-definition Wold_style_definition)
  ENABLE_C_FLAG_IF_SUPPORTED(-Woverlength-strings Woverlength_strings)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wpacked Wpacked)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wpacked-not-aligned Wpacked_not_aligned)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wpointer-arith Wpointer_arith)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wredundant-decls Wredundant_decls)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wreorder Wreorder)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wrestrict Wrestrict)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wshadow Wshadow)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wshift-overflow=2 Wshift_overflow_2)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wsizeof-array-argument Wsizeof_array_argument)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wsizeof-pointer-div Wsizeof_pointer_div)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wstrict-overflow=5 Wstrict_overflow_5)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wstrict-prototypes Wstrict_prototypes)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wstringop-truncation Wstringop_truncation)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wsuggest-attribute=format Wsuggest_attribute_format)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wsuggest-attribute=pure Wsuggest_attribute_pure)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wsuggest-attribute=noreturn Wsuggest_attribute_noreturn)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wswitch-bool Wswitch_bool)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wswitch-default Wswitch_default)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wtraditional Wtraditional)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wundef Wundef)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wunreachable-code Wunreachable_code)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wunsuffixed-float-constants Wunsuffixed_float_constants)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wwrite-strings Wwrite_strings)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wvariadic-macros Wvariadic_macros)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wvector-operation-performance Wvector_operation_performance)
  ENABLE_C_FLAG_IF_SUPPORTED(-Wwrite-strings Wwrite_strings)

  # Set the default symbol visibility to hidden as long as we're not debug.
  # If we're building a shared object, we'll flag exported symbols manually.
  ENABLE_C_FLAG_IF_SUPPORTED(-fvisibility=hidden fvisibility_hidden)

  # Enable sanitizers on debug builds.
  ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG(-fsanitize=address fsanitize_address)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG(-fsanitize=thread fsanitize_thread)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG(-fsanitize=signed-integer-overflow fsanitize_signed_integer_overflow)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG(-fsanitize=undefined fsanitize_undefined)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_DEBUG(-ftrapv ftrapv)

  # Perform extra optimizations as long as we're not debug.
  ENABLE_C_FLAG_IF_SUPPORTED_AND_NOT_DEBUG(-funsafe-loop-optimizations funsafe_loop_optimizations)

  # Enable some options that go against what the standard says.
  # We don't need the precision or redundancy provided.
  ENABLE_C_FLAG_IF_SUPPORTED_AND_NOT_DEBUG(-ffast-math  ffast_math)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_NOT_DEBUG(-fmerge-all-constants fmerge_all_constants)

  # Only do full-link time optimization on release builds.
  ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-flto flto)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-ipo ipo)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-fdata-sections fdata_sections)
  ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-ffunction-sections ffunction_sections)

  # Only attempt to enable fat LTO objects if -flto or -ipo was set.
  string(FIND "${CMAKE_C_FLAGS_RELEASE}" "-flto" flto_set)
  string(FIND "${CMAKE_C_FLAGS_RELEASE}" "-ipo" ipo_set)

  if ((NOT ${flto_set} EQUAL -1) OR (NOT ${ipo_set} EQUAL -1))
    ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-ffat-lto-objects ffat_lto_objects)
  endif ()

  # ICC will spew lots of notices to the console when this is enabled, so
  # disable those remarks. We'll print the notice ourself once using CMake...
  if (CMAKE_C_COMPILER_ID MATCHES Intel)
    ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-wd11000,11006 wd11000_11006_disable_redundant_ipo_remarks)
  endif ()

  # Optionally generate optimization reports when using ICC.
  if (CMAKE_C_COMPILER_ID MATCHES Intel)
    option(ENABLE_OPTIMIZATION_REPORTS "Enable generation of ICC optimization reports" OFF)

    if (ENABLE_OPTIMIZATION_REPORTS)
      message(STATUS "Optimization reports are generated in *.optrpt files in the output location")

      ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-wd10397 wd10397_disable_report_output_notice)
      ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-qopt-report=4 qopt_report_4)

      if (${COMPILER_SUPPORTS_ipo})
        ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE("-qopt-report-phase ipo" qopt_report_phase_ipo)
        ENABLE_C_FLAG_IF_SUPPORTED_AND_RELEASE(-wd10346 wd10346_disable_ipo_report_output_notice)
      endif ()
    endif ()
  endif ()

  # This flag MUST appear last in the check list!
  #
  # While this flag is generally useful for development, it causes massive
  # headaches for package maintainers due to the fact that unforseen problems
  # can bork package builds. One such example is: when using Clang without
  # FLTO, Clang will throw a warning at link time about CFLAGS not being used
  # (which makes sense - there's no link time code generation - so the flags
  # don't matter).
  #
  # The tl;dr: We need to gate this flag with a CMake option. Developers and
  # build bots should enable it... but it definitely shouldn't be a default.
  option(TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

  if (TREAT_WARNINGS_AS_ERRORS)
    ENABLE_C_FLAG_IF_SUPPORTED(-Werror Werror)
  endif ()

  # Release builds should use -O2, not -O3.
  # -O3 just results in lots of code bloat.
  string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

  # Strip binaries that don't need symbols.
  if (CMAKE_BUILD_TYPE MATCHES Release OR
      CMAKE_BUILD_TYPE MATCHES MinSizeRel)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
  endif ()

  # Provide buffer overflow detection where available.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")

  # Don't ever link with -rdynamic. We do not need it,
  # and it increases the size of the resulting binary.
  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

  # If using clang, make sure unused arguments during compilation don't
  # trigger an error if we're building with TREAT_WARNINGS_AS_ERRORS.
  if (CMAKE_C_COMPILER_ID MATCHES Clang)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Qunused-arguments")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Qunused-arguments")
  endif ()

  # Poke the compiler to determine archiecture.
  # XXX: Test this segment with clang and ICC.
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_VARIABLE MM_HOST_ARCH)

  string(REGEX MATCH "([a-zA-Z0-9_])+" MM_HOST_ARCH ${MM_HOST_ARCH})
endif ()

# -----------------------------------------------------------------------------
#  Configure runtime architecture and calling convention to use.
# -----------------------------------------------------------------------------
set(MM_TARGET_ARCH ${MM_HOST_ARCH}
  CACHE STRING "Target architecture")
set_property(CACHE MM_TARGET_ARCH PROPERTY STRINGS "x86_64")

# -----------------------------------------------------------------------------
#  Configure the build to leverage SWI includes and libraries.
# -----------------------------------------------------------------------------
include_directories("${CMAKE_SOURCE_DIR}/swiinc/common"
                    "${CMAKE_SOURCE_DIR}/swiinc/qmi"
                    "${CMAKE_SOURCE_DIR}/swiinc/qmux")

add_library(common STATIC IMPORTED)
set_target_properties(common PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/swilib/libcommon.a"
)

add_library(mbim STATIC IMPORTED)
set_target_properties(mbim PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/swilib/liblite-mbim.a"
)

add_library(qmi STATIC IMPORTED)
set_target_properties(qmi PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/swilib/liblite-qmi.a"
)

add_library(qmux STATIC IMPORTED)
set_target_properties(qmux PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/swilib/liblite-qmux.a"
)

# -----------------------------------------------------------------------------
#  Collect application source files and link the binary.
# -----------------------------------------------------------------------------
add_compile_definitions(__USE_KERNEL_IPV6_DEFS)
include_directories(${LIBNL_INCLUDE_DIR} ${LIBSYSTEMD_INCLUDE_DIR})
include_directories("${CMAKE_SOURCE_DIR}/inc")

set(MM_SOURCES
  src/dms.c
  src/netlink.c
  src/main.c
  src/qmux.c
  src/run_helpers.c
  src/sdbus.c
  src/wds.c
)

add_executable(${CMAKE_PROJECT_NAME} ${MM_SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBNL_LIBRARIES}
                      ${LIBSYSTEMD_LIBRARIES} ${MATH_LIBRARY}
                      qmux mbim qmi Threads::Threads common)
