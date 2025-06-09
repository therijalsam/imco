cmake_minimum_required(VERSION 3.10)
project(DronePlanner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# — find system packages —
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(OpenCV REQUIRED)

# — point at your existing folders, do NOT move anything —
set(IMGUI_DIR        ${CMAKE_SOURCE_DIR}/imgui)
set(IMGUI_BACKENDS   ${IMGUI_DIR}/backends)
set(FILEDIALOG_DIR   ${CMAKE_SOURCE_DIR}/imgui_file_dialog)

# — grab ImGui core + backends —
file(GLOB IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_BACKENDS}/imgui_impl_sdl2.cpp
        ${IMGUI_BACKENDS}/imgui_impl_sdlrenderer2.cpp
)

# — grab FileDialog source —
file(GLOB FILEDIALOG_SOURCES
        ${FILEDIALOG_DIR}/ImGuiFileDialog.cpp
)

# — your app + all ImGui code —
add_executable(${PROJECT_NAME}
        src/main.cpp
        ${IMGUI_SOURCES}
        ${FILEDIALOG_SOURCES}
)

# — headers for all of the above —
target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_BACKENDS}
        ${FILEDIALOG_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

# — link libs —
target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_image::SDL2_image
        ${OpenCV_LIBS}
)
