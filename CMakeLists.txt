# Nébula 360 - build para PC (desenvolvimento, testes e verificação cruzada).
#
# O executável do Xbox 360 (XEX) NÃO é gerado aqui: exige o XDK da Microsoft
# (ver docs/fases/FASE_2.md, seção "Ambiente de compilação do Xbox 360").
cmake_minimum_required(VERSION 3.16)
project(Nebula360 VERSION 0.2.0 LANGUAGES C CXX)

option(NEBULA_WITH_SDL2 "Frontend com janela (SDL2) para PC" ON)
option(NEBULA_WERROR "Tratar avisos como erros" ON)
option(NEBULA_BUILD_TESTS "Compilar testes automatizados" ON)

# C++03 estrito: o compilador do XDK (MSVC 2010) não suporta C++ moderno.
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(NEBULA_WARNINGS -Wall -Wextra -pedantic-errors -Wno-long-long)
  if(NEBULA_WERROR)
    list(APPEND NEBULA_WARNINGS -Werror)
  endif()
endif()

# ---------------------------------------------------------------- terceiros
add_library(nebula_stb STATIC third_party/stb/stb_impl.c)
target_include_directories(nebula_stb SYSTEM PUBLIC third_party/stb)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nebula_stb PRIVATE -w)
endif()

# ---------------------------------------------------------------- núcleo portátil
set(NEBULA_CORE_SOURCES
  src/core/Color.cpp
  src/core/Json.cpp
  src/core/Log.cpp
  src/core/Path.cpp
  src/core/Sha256.cpp
  src/core/Status.cpp
  src/core/StringUtil.cpp
  src/core/Version.cpp
  src/platform/Launcher.cpp
  src/platform/MemoryFileSystem.cpp
  src/formats/StfsHeader.cpp
  src/formats/XexHeader.cpp
  src/library/Category.cpp
  src/library/CoverResolver.cpp
  src/library/Library.cpp
  src/library/LibraryQuery.cpp
  src/library/Scanner.cpp
  src/theme/Theme.cpp
  src/store/Catalog.cpp
  src/store/DownloadQueue.cpp
  src/store/StorePolicy.cpp
  src/integrations/Integration.cpp
  src/plugins/BootGuard.cpp
  src/plugins/ModuleManifest.cpp
  src/plugins/ModuleRegistry.cpp
  src/app/AppConfig.cpp
  src/app/BuiltinModules.cpp
)
add_library(nebula_core STATIC ${NEBULA_CORE_SOURCES})
target_include_directories(nebula_core PUBLIC src)
target_compile_options(nebula_core PRIVATE ${NEBULA_WARNINGS})

# ---------------------------------------------------------------- interface
set(NEBULA_UI_SOURCES
  src/ui/Font.cpp
  src/ui/Image.cpp
  src/ui/Input.cpp
  src/ui/Screen.cpp
  src/ui/SoftwareRenderer.cpp
  src/ui/Strings.cpp
  src/ui/TextureCache.cpp
  src/ui/Widgets.cpp
  src/ui/screens/GameDetailsScreen.cpp
  src/ui/screens/KeyboardScreen.cpp
  src/ui/screens/LibraryScreen.cpp
  src/ui/screens/MenuScreens.cpp
  src/app/App.cpp
)
add_library(nebula_ui STATIC ${NEBULA_UI_SOURCES})
target_link_libraries(nebula_ui PUBLIC nebula_core nebula_stb)
target_compile_options(nebula_ui PRIVATE ${NEBULA_WARNINGS})

# ---------------------------------------------------------------- plataforma PC
add_library(nebula_host STATIC src/platform/host/PosixFileSystem.cpp)
target_link_libraries(nebula_host PUBLIC nebula_core)
target_compile_options(nebula_host PRIVATE ${NEBULA_WARNINGS})

# ---------------------------------------------------------------- aplicativos
add_executable(nebula360 apps/nebula_host/main.cpp)
target_link_libraries(nebula360 PRIVATE nebula_ui nebula_host)
target_compile_options(nebula360 PRIVATE ${NEBULA_WARNINGS})
target_compile_definitions(nebula360 PRIVATE
  NEBULA_DEFAULT_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(NEBULA_WITH_SDL2)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    target_sources(nebula360 PRIVATE apps/nebula_host/SdlFrontend.cpp)
    target_compile_definitions(nebula360 PRIVATE NEBULA_HAVE_SDL2=1)
    if(TARGET SDL2::SDL2)
      target_link_libraries(nebula360 PRIVATE SDL2::SDL2)
    else()
      target_include_directories(nebula360 SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(nebula360 PRIVATE ${SDL2_LIBRARIES})
    endif()
    message(STATUS "Nébula: frontend SDL2 habilitado")
  else()
    message(STATUS "Nébula: SDL2 não encontrado; só o modo headless será compilado")
  endif()
endif()

add_executable(nebula_scan apps/nebula_scan/main.cpp)
target_link_libraries(nebula_scan PRIVATE nebula_core nebula_host)
target_compile_options(nebula_scan PRIVATE ${NEBULA_WARNINGS})

# ---------------------------------------------------------------- testes
if(NEBULA_BUILD_TESTS)
  enable_testing()
  add_executable(nebula_tests
    tests/TestMain.cpp
    tests/TestCore.cpp
    tests/TestFormats.cpp
    tests/TestLibrary.cpp
    tests/TestStore.cpp
    tests/TestTheme.cpp
    tests/TestUi.cpp
    tests/TestApp.cpp
    tests/TestPlugins.cpp
    tests/TestDownloads.cpp
  )
  target_link_libraries(nebula_tests PRIVATE nebula_ui nebula_host)
  target_compile_options(nebula_tests PRIVATE ${NEBULA_WARNINGS} -Wno-sign-compare)
  target_compile_definitions(nebula_tests PRIVATE
    NEBULA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME nebula_tests COMMAND nebula_tests)
endif()
