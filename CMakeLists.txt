cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME hyperion)

set(vcpkg_path "$ENV{VCPKG_INSTALLED_DIR}/../scripts/buildsystems/vcpkg.cmake")
project(${BUILD_NAME} CXX C)

set(RENDERING_BACKEND "Vulkan" CACHE STRING "Rendering backend to use")

# set HYP_PLATFORM_NAME
if(WIN32)
    set(HYP_PLATFORM_NAME "windows")
elseif(APPLE)
    set(HYP_PLATFORM_NAME "macos")
elseif(UNIX)
    set(HYP_PLATFORM_NAME "linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(NATIVE false CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
else()
    set(NATIVE true CACHE BOOL "Is the build occuring on a native platform? (Access to native libraries, filesystem etc.)")
endif()

if(MSVC)
    message(STATUS "Building on MSVC, checking toolchain is set (should be set to `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`)")

    if (DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using CMake toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(SEND_ERROR "CMake toolchain file not specified -- should be set to vcpkg cmake file! \
            If you don't have vcpkg set up, download vcpkg from Microsoft and set the `-DCMAKE_TOOLCHAIN_FILE` CMake argument to \
            `<vcpkg root path>/scripts/buildsystems/vcpkg.cmake`.")
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fsanitize=address")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(OpenGL_GL_PREFERENCE GLVND)

link_directories(${CMAKE_SOURCE_DIR}/lib /usr/local/lib ${VCPKG_INSTALLED_DIR}/x64-windows/lib "Z:/gstreamer/1.0/msvc_x86_64/lib")

find_package(PkgConfig)
pkg_check_modules(GST gstreamer-1.0>=1.4
                      gstreamer-video-1.0>=1.4
                      gstreamer-app-1.0>=1.4)

# # TODO: Add windows, linux mono support
# set(ENV{PKG_CONFIG_PATH} "/Library/Frameworks/Mono.framework/Versions/Current/lib/pkgconfig")
# pkg_check_modules(MONO mono-2)
                      
find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenAL)
find_package(Vulkan)

if (NOT DEFINED RENDERING_BACKEND)
    if (EXISTS "${CMAKE_SOURCE_DIR}/submodules/webgpu/CMakeLists.txt")
        set(RENDERING_BACKEND "WebGPU")
    elseif (Vulkan_FOUND)
        set(RENDERING_BACKEND "Vulkan")
    endif()
endif()

if (RENDERING_BACKEND STREQUAL "WebGPU")
    add_definitions(-DHYP_WEBGPU=1)

    include_directories(submodules/webgpu PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/webgpu/include")
    set(COMMON_LIBRARIES ${WGPU_RUNTIME_LIB})

    add_subdirectory(submodules/webgpu)

    message(STATUS "Using experimental WebGPU rendering backend")
elseif (RENDERING_BACKEND STREQUAL "Vulkan")
    add_definitions(-DHYP_VULKAN=1)

    message(STATUS "Using Vulkan rendering backend")
else()
    message(FATAL_ERROR "No rendering backend found! Please install Vulkan or WebGPU.")
endif()

if(${OpenAL_FOUND})
    add_definitions(-DHYP_OPENAL=1)
endif()

add_definitions(-DHYP_ROOT_DIR="${CMAKE_CURRENT_LIST_DIR}")
add_definitions(-DSDL_MAIN_HANDLED=1)
add_definitions(-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHYP_ROOT_DIR=\\\"${CMAKE_CURRENT_LIST_DIR}\\\"")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp" "src/*.inl")

file(GLOB_RECURSE CSHARP_CORE_SOURCES "src/dotnet/core/**.cs")
file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "src/dotnet/runtime/**.cs")
file(GLOB_RECURSE CSHARP_INTEROP_SOURCES "src/dotnet/interop/**.cs")

# # store sources that are in runtime folder in a separate list
# file(GLOB_RECURSE RUNTIME_SOURCES "src/runtime/*.cpp" "src/runtime/*.c")
# # remove sources that are in runtime folder
# list(FILTER SOURCES EXCLUDE REGEX ".*runtime/.*")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(Bullet_FOUND)
    add_definitions(-DHYP_BULLET=1)
endif()

if (${NATIVE})

    # if glslang folder exists, link with it.
    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/glslang/glslang/CMakeLists.txt")
        add_definitions(-DHYP_GLSLANG=1)

        include_directories(BEFORE submodules/glslang)
        add_subdirectory(submodules/glslang)

        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            glslang
            SPIRV
        )
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/zlib/CMakeLists.txt")
        add_definitions(-DHYP_ZLIB=1)

        include_directories(submodules/zlib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/zlib")
        add_subdirectory(submodules/zlib)
        
        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            zlibstatic
        )
    endif()


    if(EXISTS "${CMAKE_SOURCE_DIR}/submodules/libdatachannel/CMakeLists.txt")
        add_definitions(-DHYP_LIBDATACHANNEL=1)

        include_directories(submodules/libdatachannel PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libdatachannel/include")
        add_subdirectory(submodules/libdatachannel)
        
        set(COMMON_LIBRARIES
            ${COMMON_LIBRARIES}
            datachannel-static
        )
    endif()
endif()

if(GST_FOUND)
    add_definitions(-DHYP_GSTREAMER=1)
    add_definitions(-DHYP_GSTREAMER_BIN_DIR="Z:/gstreamer/1.0/msvc_x86_64/bin")

    include_directories(${GST_INCLUDE_DIRS})
    link_directories(${GST_LIBRARY_DIRS})

    # target_link_libraries(${BUILD_NAME} PRIVATE ${GST_LIBRARIES})

    message(STATUS "GStreamer found, linking to it: ${GST_LIBRARIES}\nInclude directories: ${GST_INCLUDE_DIRS}")

    
    set(COMMON_LIBRARIES
        ${COMMON_LIBRARIES}
        ${GST_LIBRARIES}
    )
endif()

# if dotnet headers exist, set HYP_DOTNET to 1
if(EXISTS "${CMAKE_SOURCE_DIR}/include/dotnetcore/hostfxr.h")
    # set CLI_CMAKE_PKG_RID

    # if(WIN32)
    #     set(CLI_CMAKE_PKG_RID "win-x64")
    # elseif(APPLE)
    #     set(CLI_CMAKE_PKG_RID "osx-x64")
    # elseif(UNIX)
    #     set(CLI_CMAKE_PKG_RID "linux-x64")
    # else()
    #     message(FATAL_ERROR "Unsupported platform for .NET core")
    # endif()

    # # set CLI_CMAKE_COMMIT_HASH
    # execute_process(
    #     COMMAND git rev-parse HEAD
    #     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/dotnet
    #     OUTPUT_VARIABLE CLI_CMAKE_COMMIT_HASH
    #     OUTPUT_STRIP_TRAILING_WHITESPACE
    # )

    # # set CLI_CMAKE_FALLBACK_OS
    # if(WIN32)
    #     set(CLI_CMAKE_FALLBACK_OS "win")
    # elseif(APPLE)
    #     set(CLI_CMAKE_FALLBACK_OS "osx")
    # elseif(UNIX)
    #     set(CLI_CMAKE_FALLBACK_OS "linux")
    # else()
    #     message(FATAL_ERROR "Unsupported platform for .NET core")
    # endif()

    # Run dotnet.sh to build the dotnet runtime

    # copy all .so and .a files to lib/<platform name>

    execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/submodules/dotnet/build.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/dotnet
    )


    file(GLOB DOTNET_LIBS "${CMAKE_SOURCE_DIR}/submodules/dotnet/runtime/artifacts/obj/dotnet-nethost/**/net9.0/**/output/*.dylib" "${CMAKE_SOURCE_DIR}/submodules/dotnet/runtime/artifacts/obj/dotnet-nethost/Debug/net9.0/**/output/*.so" "${CMAKE_SOURCE_DIR}/submodules/dotnet/runtime/artifacts/obj/dotnet-nethost/**/net9.0/**/output/*.a")

    foreach(DOTNET_LIB ${DOTNET_LIBS})
        message(STATUS "Copying ${DOTNET_LIB} to ${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}")
    
        file(COPY ${DOTNET_LIB} DESTINATION ${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME})
    endforeach()

    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/libnethost.a")
        message(STATUS ".NET core found, setting HYP_DOTNET to 1")

        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "${CMAKE_SOURCE_DIR}/lib/${HYP_PLATFORM_NAME}/libnethost.a")
        add_definitions(-DHYP_DOTNET=1)
    else()
        message(WARNING "Dotnet headers found, but libnethost.a not found.")
    endif()

    # add_custom_command(
    #     OUTPUT ${CMAKE_SOURCE_DIR}/submodules/dotnet/runtime/src/native/corehost/nethost/libnethost.so
    #     COMMAND ${CMAKE_SOURCE_DIR}/submodules/dotnet/build.sh
    #     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/dotnet
    # )

    # add_subdirectory(submodules/dotnet/runtime/src/native/corehost/nethost/)

    # Find the nethost library, which is required for .NET core
    # to find host fxr 
    # find_package(nethost REQUIRED)

endif()

# if aftermath lib exists on windows, link to it
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/windows/64/GFSDK_Aftermath_Lib.x64.lib")
        set(COMMON_LIBRARIES ${COMMON_LIBRARIES} windows/64/GFSDK_Aftermath_Lib.x64)
        add_definitions(-DHYP_AFTERMATH=1)
    endif()
endif()

set(HYPERION_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

include_directories(
    ${HYPERION_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

add_library(${BUILD_NAME} SHARED ${SOURCES} ${HEADERS})

set(COMMON_LIBRARIES
    ${COMMON_LIBRARIES}
    ${CMAKE_THREADS_LIBS_INIT}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${SDL2_LIBRARIES}
)

target_link_libraries(${BUILD_NAME} PUBLIC ${COMMON_LIBRARIES})

function(create_csharp_library output_name csharp_sources)
    # make a directory for the .csproj file
    set(CSHARP_PROJECT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${output_name})
    file(MAKE_DIRECTORY ${CSHARP_PROJECT_DIR})


    # generate .csproj file
    file(WRITE ${CSHARP_PROJECT_DIR}/${output_name}.csproj "<Project Sdk=\"Microsoft.NET.Sdk\">\n")

    
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  <PropertyGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Version>1.0.0</Version>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <OutputType>Library</OutputType>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <TargetFramework>net8.0</TargetFramework>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <ImplicitUsings>enable</ImplicitUsings>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Nullable>enable</Nullable>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  </PropertyGroup>\n")

    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  <ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />\n")
    foreach(CSHARP_SOURCE ${csharp_sources})
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <Compile Include=\"${CSHARP_SOURCE}\" />\n")
    endforeach()

    # if name is not equal to HyperionCore then add a reference to HyperionCore
    if(NOT ${output_name} STREQUAL "HyperionCore")
        file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "    <ProjectReference Include=\"../HyperionCore/HyperionCore.csproj\" />\n")
    endif()

    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "  </ItemGroup>\n")
    file(APPEND ${CSHARP_PROJECT_DIR}/${output_name}.csproj "</Project>\n")

    # build it
    execute_process(
        COMMAND dotnet build
        WORKING_DIRECTORY ${CSHARP_PROJECT_DIR}
    )

    # move the dll up to the build directory
    file(GLOB CSHARP_DLLS "${CSHARP_PROJECT_DIR}/bin/**/net8.0/*.dll")

    foreach(CSHARP_DLL ${CSHARP_DLLS})
        message(STATUS "Copying ${CSHARP_DLL} to ${CMAKE_CURRENT_BINARY_DIR}")
        file(COPY ${CSHARP_DLL} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
endfunction()

# for all csharp sources, compile them into a Class library
# use `dotnet` to compile them

if(CSHARP_CORE_SOURCES)
    # pass the sources to the function as a list
    create_csharp_library(HyperionCore "${CSHARP_CORE_SOURCES}")
endif()

if(CSHARP_RUNTIME_SOURCES)
    create_csharp_library(HyperionRuntime "${CSHARP_RUNTIME_SOURCES}")
endif()

if(CSHARP_INTEROP_SOURCES)
    create_csharp_library(HyperionInterop "${CSHARP_INTEROP_SOURCES}")
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
add_subdirectory(samples)
add_subdirectory(editor)