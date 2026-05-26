cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME pocketbook)
project(${PROJECT_NAME} C CXX ASM)

# Include the SDK
include("$ENV{CMAKE_TOOLCHAIN_FILE}")

# Where to find pbres
string(REGEX REPLACE "[^/]+$" "pbres" PBRES "${CMAKE_C_COMPILER}")

# Add the standard link directories
link_directories("${PB_LINK_DIRECTORIES}")

# Add the standard include directories
include_directories("${PB_INCLUDE_DIRECTORIES}")
include_directories("${FREETYPE_INCLUDE_DIRS}")

# Add compiler flags for debugging
add_compile_options(-g -gdwarf-3)

# Start you CMakeLists.txt here...
set(APP_SOURCE "app.cpp" CACHE STRING "Main app source file (e.g. app.cpp or app-copy.cpp)")

add_executable(app ${APP_SOURCE})
target_link_libraries(app inkview curl)

# Set the executable output name to build.app
set_target_properties(app PROPERTIES OUTPUT_NAME "build.app")
