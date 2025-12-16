cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-rendering)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib)

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

set(COMMON_LIBRARIES
    hyperion-core
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Zc:preprocessor /MD /O2 /Zi /MP")
    add_definitions("/DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fpermissive")
endif()

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Hyperion Rendering Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

if(NOT IOS)
    set(RENDERING_BACKEND "Vulkan")

    find_package(Vulkan)
endif()

if (NOT DEFINED RENDERING_BACKEND)
    if (EXISTS "${CMAKE_SOURCE_DIR}/submodules/webgpu/CMakeLists.txt")
        set(RENDERING_BACKEND "WebGPU")
    elseif (Vulkan_FOUND)
        set(RENDERING_BACKEND "Vulkan")
    else()
        message(FATAL_ERROR "No rendering backend found")
    endif()
endif()

if (RENDERING_BACKEND STREQUAL "WebGPU")
    add_definitions(-DHYP_WEBGPU=1)

    # Build WebGPU as external project (only for VS and Xcode)
    if(MSVC) # OR CMAKE_GENERATOR STREQUAL "Xcode"
        ExternalProject_Add(webgpu_external
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/webgpu
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/webgpu_install
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            BUILD_ALWAYS FALSE
            INSTALL_COMMAND ""
            EXCLUDE_FROM_ALL TRUE
        )

        # Create imported target for WebGPU
        ExternalProject_Get_Property(webgpu_external BINARY_DIR)

        add_library(webgpu STATIC IMPORTED)
        add_dependencies(webgpu webgpu_external)

        if(WIN32)
            set_target_properties(webgpu PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/${CMAKE_BUILD_TYPE}/webgpu.lib
            )
        else()
            set_target_properties(webgpu PROPERTIES
                IMPORTED_LOCATION ${BINARY_DIR}/libwebgpu.a
            )
        endif()

        include_directories(submodules/webgpu/include)
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} webgpu)
    else()
        # For other generators (Makefiles, Ninja, etc.), use add_subdirectory
        add_subdirectory(submodules/webgpu EXCLUDE_FROM_ALL)

        include_directories(submodules/webgpu/include)
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} webgpu)
    endif()

    message(STATUS "Using experimental WebGPU rendering backend")
elseif (RENDERING_BACKEND STREQUAL "Vulkan")
    add_definitions(-DHYP_VULKAN=1)

    # expose HYP_VULKAN as a CMake variable so other parts of the build
    # can query whether Vulkan sources/features should be included
    set(HYP_VULKAN 1)

    message(STATUS "Found Vulkan at: ${Vulkan_INCLUDE_DIRS} ${Vulkan_LIBRARIES}")
else()
    message(FATAL_ERROR "No rendering backend found! Please install Vulkan or WebGPU.")
endif()

if(IOS)
    find_package(SDL2 REQUIRED)

    if(SDL2_FOUND)
        set(HYP_SDL 1)
    endif()

    if (${CMAKE_OSX_SYSROOT} STREQUAL "iphonesimulator")
        set(IOS_SIMULATOR true)
        set(MVK_PACKAGE_NAME "ios-arm64_x86_64-simulator")
    else()
        set(MVK_PACKAGE_NAME "ios-arm64")
    endif()

    # ensure VULKAN_SDK is set
    if(NOT DEFINED ENV{VULKAN_SDK})
        message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please set it to the path of your Vulkan SDK.")
    else()
        message(STATUS "VULKAN_SDK environment variable is set to: $ENV{VULKAN_SDK}")
    endif()

    set(CMAKE_SYSTEM_NAME iOS)
    set(NO_CMAKE_SYSTEM_PATH TRUE)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
    set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")

    set(VULKAN_SDK $ENV{VULKAN_SDK})
    set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/include")
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "${VULKAN_SDK}/../MoltenVK/static/MoltenVK.xcframework/${MVK_PACKAGE_NAME}/libMoltenVK.a")
    set(Vulkan_LIBRARIES "${VULKAN_SDK}/../MoltenVK/static/MoltenVK.xcframework/${MVK_PACKAGE_NAME}/libMoltenVK.a")
    set(Vulkan_FOUND TRUE)

    set(SDL2_INCLUDE_DIR "${THIRD_PARTY_INCLUDE_DIR}/SDL")
    set(SDL2_INCLUDE_DIRS "${THIRD_PARTY_INCLUDE_DIR}/SDL")
    set(SDL2_LIBRARIES "${THIRD_PARTY_LIBRARY_DIR}/libSDL2.a")
    set(SDL2_LIBRARY "${THIRD_PARTY_LIBRARY_DIR}/libSDL2.a")
    set(SDL2_FOUND TRUE)

    find_library(UIKIT_LIB UIKit)
    find_library(COREMEDIA_LIB CoreMedia)
    find_library(METAL_LIB Metal)

    # These are all SDL requirements - we should remove the SDL dependency for iOS.
    find_library(IOSURFACE_LIB IOSurface)
    find_library(AVFOUNDATION_LIB AVFoundation)
    find_library(GAMECONTROLLER_LIB GameController)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(COREHAPTICS_LIB CoreHaptics)
    find_library(COREGRAPHICS_LIB CoreGraphics)
    find_library(COREMOTION_LIB CoreMotion)
    find_library(OPENGLES_LIB OpenGLES)

    set(COMMON_LIBRARIES
        ${UIKIT_LIB}
        ${COREMEDIA_LIB}
        ${METAL_LIB}

        # Begin SDL stuff
        ${IOSURFACE_LIB}
        ${AVFOUNDATION_LIB}
        ${GAMECONTROLLER_LIB}
        ${AUDIOTOOLBOX_LIB}
        ${QUARTZCORE_LIB}
        ${COREHAPTICS_LIB}
        ${COREGRAPHICS_LIB}
        ${COREMOTION_LIB}
        ${OPENGLES_LIB}
    )
elseif(HYP_PLATFORM_NAME STREQUAL "Linux")
    # we still use sdl on Linux (for now)
    find_package(SDL2 REQUIRED)

    if(SDL2_FOUND)
        set(HYP_SDL 1)
    endif()
endif()

add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

include_directories(
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/rendering
)

if (${NATIVE} AND NOT ${HYP_PLATFORM_NAME} STREQUAL "iOS")
    # if glslang folder exists, link with it.
    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/glslang/glslang/CMakeLists.txt")
        add_definitions(-DHYP_GLSLANG=1)
        add_definitions(-DHYP_GLSLANG_MODIFICATIONS=1)

        # Build glslang as external project to avoid cluttering solution (only for VS and Xcode)
        if(MSVC) # OR CMAKE_GENERATOR STREQUAL "Xcode"
            ExternalProject_Add(glslang_external
                SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/glslang
                CMAKE_ARGS
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/glslang_install
                    -DGLSLANG_ENABLE_INSTALL=OFF
                    -DGLSLANG_ENABLE_INSTALL_DEFAULT=OFF
                    -DGLSLANG_TESTS=OFF
                    -DENABLE_GLSLANG_BINARIES=OFF
                    -DENABLE_SPVREMAPPER=OFF
                    -DENABLE_SPIRV=ON
                    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                    -DCMAKE_CXX_FLAGS=-DHYP_GLSLANG_MODIFICATIONS=1
                BUILD_ALWAYS FALSE
                INSTALL_COMMAND ""
                EXCLUDE_FROM_ALL TRUE
            )

            # Create imported targets for all the libraries we need
            ExternalProject_Get_Property(glslang_external BINARY_DIR)

            add_library(glslang STATIC IMPORTED)
            add_library(SPIRV STATIC IMPORTED)
            add_library(glslang-default-resource-limits STATIC IMPORTED)
            add_library(SPIRV-Tools STATIC IMPORTED)
            add_library(SPIRV-Tools-opt STATIC IMPORTED)
            add_library(MachineIndependent STATIC IMPORTED)
            add_library(GenericCodeGen STATIC IMPORTED)
            add_library(OSDependent STATIC IMPORTED)

            add_dependencies(glslang glslang_external)
            add_dependencies(SPIRV glslang_external)
            add_dependencies(glslang-default-resource-limits glslang_external)
            add_dependencies(SPIRV-Tools glslang_external)
            add_dependencies(SPIRV-Tools-opt glslang_external)
            add_dependencies(MachineIndependent glslang_external)
            add_dependencies(GenericCodeGen glslang_external)
            add_dependencies(OSDependent glslang_external)

            # Set the imported library locations
            if(WIN32)
                set_target_properties(glslang PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/glslang.lib
                )
                set_target_properties(SPIRV PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/SPIRV/${CMAKE_BUILD_TYPE}/SPIRV.lib
                )
                set_target_properties(glslang-default-resource-limits PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/glslang-default-resource-limits.lib
                )
                set_target_properties(SPIRV-Tools PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/${CMAKE_BUILD_TYPE}/SPIRV-Tools.lib
                )
                set_target_properties(SPIRV-Tools-opt PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/opt/${CMAKE_BUILD_TYPE}/SPIRV-Tools-opt.lib
                )
                set_target_properties(MachineIndependent PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/MachineIndependent.lib
                )
                set_target_properties(GenericCodeGen PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/GenericCodeGen.lib
                )
                set_target_properties(OSDependent PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/OSDependent/Windows/${CMAKE_BUILD_TYPE}/OSDependent.lib
                )
            else()
                set_target_properties(glslang PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libglslang.a
                )
                set_target_properties(SPIRV PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/SPIRV/${CMAKE_BUILD_TYPE}/libSPIRV.a
                )
                set_target_properties(glslang-default-resource-limits PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libglslang-default-resource-limits.a
                )
                set_target_properties(SPIRV-Tools PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/${CMAKE_BUILD_TYPE}/libSPIRV-Tools.a
                )
                set_target_properties(SPIRV-Tools-opt PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/External/spirv-tools/source/opt/${CMAKE_BUILD_TYPE}/libSPIRV-Tools-opt.a
                )
                set_target_properties(MachineIndependent PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libMachineIndependent.a
                )
                set_target_properties(GenericCodeGen PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/${CMAKE_BUILD_TYPE}/libGenericCodeGen.a
                )
                set_target_properties(OSDependent PROPERTIES
                    IMPORTED_LOCATION ${BINARY_DIR}/glslang/OSDependent/Unix/${CMAKE_BUILD_TYPE}/libOSDependent.a
                )
            endif()

            include_directories(BEFORE ${CMAKE_SOURCE_DIR}/submodules/glslang)

            set(COMMON_LIBRARIES
                ${COMMON_LIBRARIES}
                glslang
                SPIRV
                glslang-default-resource-limits
                SPIRV-Tools
                SPIRV-Tools-opt
                MachineIndependent
                GenericCodeGen
                OSDependent
            )
        else()
            # For other generators (Makefiles, Ninja, etc.), use add_subdirectory
            set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
            set(GLSLANG_ENABLE_INSTALL_DEFAULT OFF CACHE BOOL "" FORCE)
            set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
            set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
            set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
            set(ENABLE_SPIRV ON CACHE BOOL "" FORCE)
            set(CMAKE_POSITION_INDEPENDENT_CODE ON)

            add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/glslang EXCLUDE_FROM_ALL)

            include_directories(BEFORE ${CMAKE_SOURCE_DIR}/submodules/glslang)

            set(COMMON_LIBRARIES
                ${COMMON_LIBRARIES}
                glslang
                SPIRV
                glslang-default-resource-limits
                SPIRV-Tools
                SPIRV-Tools-opt
                MachineIndependent
                GenericCodeGen
                OSDependent
            )
        endif()
    endif()
endif()

# render backend specific stuff
if(NOT DEFINED HYP_VULKAN)
    file(GLOB_RECURSE VULKAN_FILES "src/rendering/vulkan/**/*")
    foreach(VULKAN_FILE ${VULKAN_FILES})
        list(REMOVE_ITEM SOURCES ${VULKAN_FILE})
        list(REMOVE_ITEM HEADERS ${VULKAN_FILE})
    endforeach()
endif()

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${Vulkan_LIBRARIES}
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

target_link_libraries(${BUILD_NAME} PRIVATE ${COMMON_LIBRARIES})

if(TARGET glslang_external)
    add_dependencies(${BUILD_NAME} glslang_external)
endif()

# add definitions that should be visible to the main project as well
if(HYP_VULKAN)
    target_compile_definitions(${BUILD_NAME} PUBLIC HYP_VULKAN=1)
endif()
if(HYP_SDL)
    target_compile_definitions(${BUILD_NAME} PUBLIC HYP_SDL=1)
endif()
if(HYP_GLSLANG)
    target_compile_definitions(${BUILD_NAME} PUBLIC HYP_GLSLANG=1)
endif()