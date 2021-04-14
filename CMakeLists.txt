cmake_minimum_required(VERSION 3.11.4)
project (lilotane)

set(CMAKE_CXX_STANDARD 17)


if(NOT IPASIRSOLVER)
    set(IPASIRSOLVER "glucose4")
endif()
if(NOT IPASIRDIR)
    set(IPASIRDIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
endif()

# Build-specific compile options

set(MY_DEFINITIONS IPASIRSOLVER=\"${IPASIRSOLVER}\")
if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    set(MY_DEFINITIONS ${MY_DEFINITIONS} LILOTANE_VERSION=\"dbg-${IPASIRSOLVER}\")
    set(MY_DEBUG_OPTIONS "-rdynamic -g -ggdb") 
else()
    set(MY_DEFINITIONS ${MY_DEFINITIONS} LILOTANE_VERSION=\"rls-${IPASIRSOLVER}\")
    set(MY_DEBUG_OPTIONS "-rdynamic -g -ggdb") 
    #set(BASE_COMPILEFLAGS -flto)
endif()

if(LILOTANE_USE_ASAN)
    set(MY_DEBUG_OPTIONS "${MY_DEBUG_OPTIONS} -fno-omit-frame-pointer -fsanitize=address -static-libasan") 
endif()


# Libraries and includes

link_directories(lib ${IPASIRDIR}/${IPASIRSOLVER} build)
set(BASE_LIBS ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS} m z pandaPIparser)
set(BASE_INCLUDES ${MPI_CXX_INCLUDE_PATH} src src/pandaPIparser/src)
if(EXISTS ${IPASIRDIR}/${IPASIRSOLVER}/LIBS)
    message(STATUS "${IPASIRDIR}/${IPASIRSOLVER}/LIBS exists")
    file(READ ${IPASIRDIR}/${IPASIRSOLVER}/LIBS SOLVERLIBS)
else()
    message(STATUS "${IPASIRDIR}/${IPASIRSOLVER}/LIBS does not exist")
    # Assume that dummy functions are required
    set(SOLVERLIBS "-ldummyfuncs")
endif()

# Source files (without main.cpp)

set(BASE_SOURCES
    src/algo/arg_iterator.cpp src/algo/domination_resolver.cpp src/algo/fact_analysis.cpp src/algo/instantiator.cpp src/algo/network_traversal.cpp src/algo/planner.cpp src/algo/plan_writer.cpp src/algo/retroactive_pruning.cpp
    src/data/action.cpp src/data/htn_instance.cpp src/data/htn_op.cpp src/data/layer.cpp src/data/position.cpp src/data/reduction.cpp src/data/signature.cpp src/data/substitution.cpp
    src/sat/binary_amo.cpp src/sat/encoding.cpp src/sat/literal_tree.cpp src/sat/plan_optimizer.cpp src/sat/variable_domain.cpp
    src/util/log.cpp src/util/names.cpp src/util/params.cpp src/util/random.cpp src/util/signal_manager.cpp src/util/timer.cpp
)


# Base library for Lilotane (liblotane)

add_library(lotane STATIC ${BASE_SOURCES})
target_include_directories(lotane PRIVATE ${BASE_INCLUDES})
target_compile_options(lotane PRIVATE ${BASE_COMPILEFLAGS})


# Executable

add_executable(lilotane src/main.cpp)
target_include_directories(lilotane PRIVATE ${BASE_INCLUDES})
target_compile_options(lilotane PRIVATE ${BASE_COMPILEFLAGS})
target_compile_definitions(lilotane PRIVATE ${MY_DEFINITIONS})

if("${SOLVERLIBS}" MATCHES ".*dummyfuncs.*")
    # "dummyfuncs" library for unimplemented extra IPASIR functions
    add_library(dummyfuncs
        STATIC lib/dummyfuncs/dummyfuncs.cpp
    )
endif()
if("${SOLVERLIBS}" MATCHES ".*[A-Za-z].*")
    target_link_libraries(lilotane lotane ${BASE_LIBS} ipasir${IPASIRSOLVER} ${SOLVERLIBS})
else()
    target_link_libraries(lilotane lotane ${BASE_LIBS} ipasir${IPASIRSOLVER})
endif()


# PandaPIparser

add_custom_target(parser cd ../src/ && bash fetch_and_build_parser.sh)
add_dependencies(lotane parser)


# Solver library

add_custom_target(solverlib cd .. && cd ${IPASIRDIR}/${IPASIRSOLVER}/ && [ ! -f fetch_and_build.sh ] || bash fetch_and_build.sh)
add_dependencies(lilotane solverlib)


# Global debug flags

add_definitions("${MY_DEBUG_OPTIONS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MY_DEBUG_OPTIONS}")



enable_testing()

add_executable(test_arg_iterator src/test/test_arg_iterator.cpp)
target_include_directories(test_arg_iterator PRIVATE ${BASE_INCLUDES})
target_compile_options(test_arg_iterator PRIVATE ${BASE_COMPILEFLAGS})
target_link_libraries(test_arg_iterator ${BASE_LIBS} lotane)
add_test(NAME test_arg_iterator COMMAND test_arg_iterator)

