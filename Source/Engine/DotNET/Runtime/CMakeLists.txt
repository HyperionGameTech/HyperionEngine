cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW)

set(BUILD_NAME hyperion-dotnet-runtime)

project(${BUILD_NAME} CXX C)

message(STATUS "Building ${BUILD_NAME} module, base directory: ${CMAKE_SOURCE_DIR}")

link_directories(${CMAKE_THIRD_PARTY_LIBRARY_DIRECTORY})

set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to be used")

file(GLOB_RECURSE SOURCES "*.cpp" "*.cc" "*.c")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "${BUILD_NAME} Code" FILES ${SOURCES} ${HEADERS})

add_definitions(-DHYP_BUILD_LIBRARY=1)

include_directories(SYSTEM
    ${Vulkan_INCLUDE_DIRS}
    ${THIRD_PARTY_INCLUDE_DIR}
)
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Engine/DotNET
)

add_library(${BUILD_NAME} STATIC ${SOURCES} ${HEADERS})

target_compile_definitions(${BUILD_NAME} PRIVATE HYP_BUILD_ENGINE=1)

if(NOT "${${BUILD_NAME}_PCH}" STREQUAL "")
  target_precompile_headers(${BUILD_NAME} PRIVATE "${${BUILD_NAME}_PCH}")
endif()

hyp_link_core_library(${BUILD_NAME})

# writes <Assembly>.csproj (an AOT-compatible library with the type registry generator) and the target that builds it into the library
# output directory; REFERENCES are other generated projects' assembly names, DEPENDS the targets that build them
function(hyp_add_managed_runtime_assembly assembly_name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;REFERENCES;DEPENDS;PACKAGE_REFERENCES")

  set(project_dir "${CSHARP_PROJECTS_DIR}/${assembly_name}")
  set(csproj_path "${project_dir}/${assembly_name}.csproj")

  file(MAKE_DIRECTORY ${project_dir})

  file(WRITE ${csproj_path} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
  file(APPEND ${csproj_path} "  <PropertyGroup>\n")
  file(APPEND ${csproj_path} "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
  file(APPEND ${csproj_path} "    <OutputType>Library</OutputType>\n")
  file(APPEND ${csproj_path} "    <TargetFramework>net10.0</TargetFramework>\n")
  file(APPEND ${csproj_path} "    <LangVersion>13.0</LangVersion>\n")
  file(APPEND ${csproj_path} "    <ImplicitUsings>enable</ImplicitUsings>\n")
  file(APPEND ${csproj_path} "    <Nullable>enable</Nullable>\n")
  file(APPEND ${csproj_path} "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
  file(APPEND ${csproj_path} "    <IsAotCompatible>true</IsAotCompatible>\n")
  file(APPEND ${csproj_path} "    <AssemblyName>${assembly_name}</AssemblyName>\n")
  file(APPEND ${csproj_path} "    <EnableDynamicLoading>true</EnableDynamicLoading>\n")
  file(APPEND ${csproj_path} "    <Platforms>x64;ARM64</Platforms>\n")
  if(CSHARP_CONSTANTS)
    file(APPEND ${csproj_path} "    <DefineConstants>${CSHARP_CONSTANTS}</DefineConstants>\n")
  endif()
  file(APPEND ${csproj_path} "  </PropertyGroup>\n")

  file(APPEND ${csproj_path} "  <ItemGroup>\n")
  foreach(package_reference ${ARG_PACKAGE_REFERENCES})
    file(APPEND ${csproj_path} "    ${package_reference}\n")
  endforeach()
  hyp_append_type_registry_generator(${csproj_path})
  foreach(source ${ARG_SOURCES})
    file(APPEND ${csproj_path} "    <Compile Include=\"${source}\" />\n")
  endforeach()

  foreach(reference ${ARG_REFERENCES})
    if(USE_MSBUILD_FOR_CSHARP_BUILD)
      file(APPEND ${csproj_path} "    <ProjectReference Include=\"${CSHARP_PROJECTS_DIR}/${reference}/${reference}.csproj\" />\n")
    else()
      file(APPEND ${csproj_path} "    <Reference Include=\"${reference}\" >\n")
      file(APPEND ${csproj_path} "        <HintPath>${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${reference}.dll</HintPath>\n")
      file(APPEND ${csproj_path} "    </Reference>\n")
    endif()
  endforeach()

  file(APPEND ${csproj_path} "  </ItemGroup>\n")
  # copy dll
  file(APPEND ${csproj_path} "  <Target Name=\"CopyDLL\" AfterTargets=\"AfterBuild\">\n")
  file(APPEND ${csproj_path} "    <Copy SourceFiles=\"$(TargetPath)\" DestinationFolder=\"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/\" />\n")
  file(APPEND ${csproj_path} "  </Target>\n")
  file(APPEND ${csproj_path} "</Project>\n")

  # find dotnet executable
  find_program(DOTNET_EXE dotnet)
  if(NOT DOTNET_EXE)
    message(SEND_ERROR "dotnet executable not found. Please install the .NET SDK to build C# projects.")
  endif()

  # Clear MSBuildSdksPath environment variable if set to prevent SDK resolution issues
  set(MSBuildSdksPath_BACKUP $ENV{MSBuildSdksPath})
  unset(ENV{MSBuildSdksPath})

  execute_process(
          COMMAND ${DOTNET_EXE} restore
          WORKING_DIRECTORY ${project_dir}
      )

  # Restore environment variable
  if(MSBuildSdksPath_BACKUP)
    set(ENV{MSBuildSdksPath} ${MSBuildSdksPath_BACKUP})
  endif()

  if(NOT USE_MSBUILD_FOR_CSHARP_BUILD)
    message(STATUS "Using dotnet CLI to build ${assembly_name}")

    if(CMAKE_HOST_WIN32)
      set(DOTNET_BUILD_WRAPPER "${project_dir}/dotnet-build.bat")
      file(WRITE ${DOTNET_BUILD_WRAPPER} "@echo off\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "set MSBuildSdksPath=\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
    else()
      set(DOTNET_BUILD_WRAPPER "${project_dir}/dotnet-build.sh")
      file(WRITE ${DOTNET_BUILD_WRAPPER} "#!/bin/bash\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "unset MSBuildSdksPath\n")
      file(APPEND ${DOTNET_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
      execute_process(COMMAND chmod +x ${DOTNET_BUILD_WRAPPER})
    endif()

    add_custom_target(${assembly_name} ALL
              COMMAND ${DOTNET_BUILD_WRAPPER}
              DEPENDS ${ARG_DEPENDS}
              WORKING_DIRECTORY ${project_dir}
          )
  else()
    enable_language(CSharp)
    include_external_msproject(
              ${assembly_name}
              ${csproj_path}
              TYPE FAE04EC0-301F-11D3-BF4B-00C04F79EFBC
          )

    if(ARG_DEPENDS)
      add_dependencies(${assembly_name} ${ARG_DEPENDS})
    endif()
  endif()

  if(TARGET Hyperion.NET.Generators)
    add_dependencies(${assembly_name} Hyperion.NET.Generators)
  endif()
endfunction()

if(HYP_DOTNET)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../Generators/Generators.cmake)

  file(GLOB_RECURSE CSHARP_RUNTIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/**.cs" "${HYP_GENERATED_DIR}/CSharp/**.cs")

  if(NOT HYP_EDITOR)
    # bindings for editor classes, which don't exist natively without HYP_EDITOR
    list(FILTER CSHARP_RUNTIME_SOURCES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/Editor/")
    list(FILTER CSHARP_RUNTIME_SOURCES EXCLUDE REGEX "^${HYP_GENERATED_DIR}/CSharp/Editor/")
  endif()

  # plugin modules' bindings go into assemblies of their own (runtime modules, below) or into Hyperion.Editor (editor modules)
  list(FILTER CSHARP_RUNTIME_SOURCES EXCLUDE REGEX "^${HYP_GENERATED_DIR}/CSharp/Plugins/")
  list(FILTER CSHARP_RUNTIME_SOURCES EXCLUDE REGEX "^${HYP_GENERATED_DIR}/CSharp/Editor/Plugins/")

  set(HYP_PLUGIN_ASSEMBLIES "")

  if(CSHARP_RUNTIME_SOURCES)
    # Interop is referenced for HyperionApp, which boots the engine from a C# game
    hyp_add_managed_runtime_assembly(Hyperion.NET.Runtime
      SOURCES ${CSHARP_RUNTIME_SOURCES}
      REFERENCES Hyperion.NET.Shared Hyperion.NET.Interop
      DEPENDS hyperion-dotnet-shared Hyperion.NET.Interop
      PACKAGE_REFERENCES "<PackageReference Include=\"Microsoft.NETCore.DotNetAppHost\" Version=\"8.0.1\" />"
    )

    add_dependencies(${BUILD_NAME} Hyperion.NET.Runtime)

    # a built-in plugin runtime module's C# (its bindings and <Plugin>/Managed/<Module>) is Hyperion.Plugin.<Module>, which a game
    # references only when it links the module
    foreach(plugin_module ${HYP_PLUGIN_MODULES})
      if(plugin_module MATCHES "Editor$")
        continue()
      endif()

      get_filename_component(plugin_managed_dir "${HYP_PLUGIN_SOURCE_ROOT_${plugin_module}}/../Managed/${plugin_module}" ABSOLUTE)

      file(GLOB_RECURSE plugin_csharp_sources "${plugin_managed_dir}/*.cs" "${HYP_GENERATED_DIR}/CSharp/Plugins/${plugin_module}/*.cs")
      list(FILTER plugin_csharp_sources EXCLUDE REGEX ".*/(obj|bin)/.*")

      if(NOT plugin_csharp_sources)
        continue()
      endif()

      hyp_add_managed_runtime_assembly(Hyperion.Plugin.${plugin_module}
        SOURCES ${plugin_csharp_sources}
        REFERENCES Hyperion.NET.Shared Hyperion.NET.Interop Hyperion.NET.Runtime
        DEPENDS hyperion-dotnet-shared Hyperion.NET.Interop Hyperion.NET.Runtime
      )

      add_dependencies(${BUILD_NAME} Hyperion.Plugin.${plugin_module})
      list(APPEND HYP_PLUGIN_ASSEMBLIES Hyperion.Plugin.${plugin_module})
    endforeach()
  endif()

  set(HYP_PLUGIN_ASSEMBLIES ${HYP_PLUGIN_ASSEMBLIES} CACHE INTERNAL "Hyperion.Plugin.<Module>: the assemblies of built-in plugin runtime modules that have C#")
endif()
