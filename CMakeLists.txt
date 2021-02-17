cmake_minimum_required(VERSION 3.11.4)
project (lilotane)

find_package(MPI REQUIRED)

set(CMAKE_CXX_STANDARD 17)


if(NOT IPASIRSOLVER)
    set(IPASIRSOLVER "glucose4")
endif()
set(SOLVERLIB "lib/${IPASIRSOLVER}/libipasir${IPASIRSOLVER}.a")


# Build-specific compile options

add_definitions("-DIPASIRSOLVER=\"${IPASIRSOLVER}\"")
if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions("-DLILOTANE_VERSION=\"dbg-${IPASIRSOLVER}\"")
    set(MY_DEBUG_OPTIONS "-rdynamic -g -ggdb") 
else()
    add_definitions("-DLILOTANE_VERSION=\"rls-${IPASIRSOLVER}\"")
    set(MY_DEBUG_OPTIONS "-rdynamic -g -ggdb") 
    set(BASE_COMPILEFLAGS -O3 -flto)
endif()

if(LILOTANE_USE_ASAN)
    set(MY_DEBUG_OPTIONS "${MY_DEBUG_OPTIONS} -fno-omit-frame-pointer -fsanitize=address -static-libasan") 
endif()


# Libraries and includes

link_directories(lib lib/${IPASIRSOLVER})
set(BASE_LIBS ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS} m ipasir${IPASIRSOLVER} z pandaPIparser)
set(BASE_INCLUDES ${MPI_CXX_INCLUDE_PATH} src src/pandaPIparser/src)
file(READ lib/${IPASIRSOLVER}/LIBS SOLVERLIBS)


# Source files

set(BASE_SOURCES
    src/algo/arg_iterator.cpp src/algo/domination_resolver.cpp src/algo/fact_analysis.cpp src/algo/instantiator.cpp src/algo/network_traversal.cpp src/algo/planner.cpp src/algo/plan_writer.cpp src/algo/retroactive_pruning.cpp
    src/data/action.cpp src/data/htn_instance.cpp src/data/htn_op.cpp src/data/layer.cpp src/data/position.cpp src/data/reduction.cpp src/data/signature.cpp src/data/substitution.cpp
    src/sat/binary_amo.cpp src/sat/encoding.cpp src/sat/literal_tree.cpp src/sat/variable_domain.cpp
    src/util/log.cpp src/util/names.cpp src/util/params.cpp src/util/signal_manager.cpp src/util/timer.cpp
    src/main.cpp
)


# Executable

add_executable(lilotane ${BASE_SOURCES})
target_include_directories(lilotane PRIVATE ${BASE_INCLUDES})
target_compile_options(lilotane PRIVATE ${BASE_COMPILEFLAGS})
target_link_libraries(lilotane ${BASE_LIBS})


# Debug flags to find line numbers in stack traces etc.

add_definitions("${MY_DEBUG_OPTIONS}")
add_definitions("${SOLVERLIBS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MY_DEBUG_OPTIONS}")
