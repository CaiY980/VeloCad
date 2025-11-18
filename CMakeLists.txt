cmake_minimum_required(VERSION 3.14)
project(VeloCad)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_BINARY_DIR}/bin>)  

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set(WIN32_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon.rc)
endif ()
if (MSVC)
    add_compile_options(/bigobj)  
endif ()

# ========== Qt 环境配置（自动适配 Qt5/Qt6）==========
set(CMAKE_AUTOMOC ON)  # 自动处理 Qt 的 moc 编译
set(CMAKE_AUTORCC ON)  # 自动处理 Qt 的 qrc 资源文件

if(NOT FORCE_QT5)
    find_package(Qt6 QUIET COMPONENTS Core Widgets) 
endif()

if (Qt6_FOUND)
    message(STATUS "Found Qt6, using Qt6")
    set(QT_VERSION_MAJOR 6)
    set(QT_VERSION ${Qt6_VERSION})
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)  
    message(STATUS "Using Qt5")
    set(QT_VERSION_MAJOR 5)
    set(QT_VERSION ${Qt5_VERSION})
    if (MSVC)
        add_compile_options("/source-charset:utf-8")  
        add_compile_options("/execution-charset:utf-8")
    endif ()
endif()

# ========== OpenCASCADE 环境配置 ==========
find_package(OpenCASCADE REQUIRED)  
message(STATUS "OpenCASCADE version: ${OpenCASCADE_VERSION}")
message(STATUS "Qt version: ${QT_VERSION}")

# ========== 项目目标配置（仅保留编译核心逻辑）==========
# 收集源文件（src/*.cpp/.h + resource/*.qrc）
file(GLOB_RECURSE SRC_FILES 
    src/*.cpp 
    src/*.h 
    resource/*.qrc
)

# 生成可执行文件
add_executable(VeloCad 
    ${SRC_FILES} 
    ${WIN32_RESOURCES}  # Windows 资源文件（图标等）
)

# 头文件包含目录
target_include_directories(VeloCad PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}  # OpenCASCADE 头文件
    src 
    src/widgets 
    src/shapes  # 项目自身头文件目录
)

# 链接依赖库（Qt + OpenCASCADE）
if (QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(VeloCad PRIVATE
        Qt6::Core
        Qt6::Widgets
        ${OpenCASCADE_LIBRARIES}
    )
else()
    target_link_libraries(VeloCad PRIVATE
        Qt5::Widgets
        ${OpenCASCADE_LIBRARIES}
    )
endif()

# Windows Release 模式隐藏控制台
if ((${CMAKE_SYSTEM_NAME} MATCHES "Windows") AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    set_target_properties(VeloCad PROPERTIES WIN32_EXECUTABLE ON)
endif ()

if (WIN32)
    # 复制 OpenCASCADE DLL 到程序目录（避免运行时找不到库）
    file(GLOB_RECURSE OCC_DLLS 
        LIST_DIRECTORIES false
        "${OpenCASCADE_LIBRARY_DIR}/bin/*.dll"
        "${OpenCASCADE_LIBRARY_DIR}/*.dll"
    )
    foreach(DLL ${OCC_DLLS})
        add_custom_command(TARGET VeloCad POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}"
                "$<TARGET_FILE_DIR:VeloCad>"
            COMMENT "Copying OpenCASCADE DLL: ${DLL}"
        )
    endforeach()

    # Qt6 额外复制插件（Qt5 通常不需要手动复制）
    if (QT_VERSION_MAJOR EQUAL 6)
        add_custom_command(TARGET VeloCad POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DCONFIG_TYPE=$<CONFIG>
                -DQt6_DIR=${Qt6_DIR}
                -DTARGET_DIR=$<TARGET_FILE_DIR:VeloCad>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_qt_plugins.cmake
            COMMENT "Copying Qt6 plugins for $<CONFIG>..."
        )
        # 生成 qt.conf 确保 Qt6 能找到插件
        add_custom_command(TARGET VeloCad POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "[Paths]" > "$<TARGET_FILE_DIR:VeloCad>/qt.conf"
            COMMAND ${CMAKE_COMMAND} -E echo "Plugins=plugins" >> "$<TARGET_FILE_DIR:VeloCad>/qt.conf"
            COMMENT "Generating qt.conf"
        )
    endif()
endif ()