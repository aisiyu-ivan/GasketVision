cmake_minimum_required(VERSION 3.16)

project(GasketVision VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HMI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/HMI)
set(VISION_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VisionEngine)
set(CAMERA_SERVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CameraService)
set(LEGACY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Pie_Chart_Control)
set(CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

set(HMI_SOCKET_ENGINE_HMI_SOURCES
    ${HMI_DIR}/socket/engine_hmi/EngineHmiSocketServer.cpp
    ${HMI_DIR}/socket/engine_hmi/EngineHmiSocketServer.h
    ${HMI_DIR}/socket/engine_hmi/EngineHmiProtocol.h
)

set(VISION_ENGINE_SOCKET_CAMERA_SOURCES
    ${VISION_ENGINE_DIR}/socket/camera_engine/CameraEngineSocketServer.cpp
    ${VISION_ENGINE_DIR}/socket/camera_engine/CameraEngineSocketServer.h
    ${VISION_ENGINE_DIR}/socket/camera_engine/CameraEngineProtocol.h
)

set(VISION_ENGINE_SOCKET_ENGINE_HMI_SOURCES
    ${VISION_ENGINE_DIR}/socket/engine_hmi/EngineHmiSocketClient.cpp
    ${VISION_ENGINE_DIR}/socket/engine_hmi/EngineHmiSocketClient.h
)

set(CAMERA_SERVICE_SOCKET_SOURCES
    ${CAMERA_SERVICE_DIR}/socket/camera_engine/CameraEngineSocketClient.cpp
    ${CAMERA_SERVICE_DIR}/socket/camera_engine/CameraEngineSocketClient.h
)

# Qt Creator kits set CMAKE_PREFIX_PATH; generic IDE shells often do not.
if(DEFINED ENV{Qt6_DIR})
    get_filename_component(_qt6_prefix "$ENV{Qt6_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_qt6_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_qt6_prefix}")
    endif()
endif()
if(DEFINED ENV{Qt5_DIR})
    get_filename_component(_qt5_prefix "$ENV{Qt5_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_qt5_prefix}/lib/cmake/Qt5/Qt5Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_qt5_prefix}")
    endif()
endif()
if(DEFINED ENV{QTDIR})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()
if(WIN32)
    set(_qt_glob_matched FALSE)
    if(MSVC)
        set(_qt_kit_globs
            "F:/QtCreater/*/msvc2022_64"
            "F:/QtCreater/*/msvc2019_64"
            "C:/Qt/*/msvc2022_64"
            "C:/Qt/*/msvc2019_64"
            "D:/Qt/*/msvc2022_64"
            "D:/Qt/*/msvc2019_64")
    else()
        set(_qt_kit_globs
            "F:/QtCreater/*/mingw_64"
            "C:/Qt/*/mingw_64"
            "D:/Qt/*/mingw_64")
    endif()
    foreach(_qt_glob IN LISTS _qt_kit_globs)
        if(_qt_glob_matched)
            break()
        endif()
        file(GLOB _qt_dirs "${_qt_glob}")
        if(NOT _qt_dirs)
            continue()
        endif()
        list(SORT _qt_dirs COMPARE NATURAL ORDER DESCENDING)
        foreach(_qt_d IN LISTS _qt_dirs)
            if(EXISTS "${_qt_d}/lib/cmake/Qt6/Qt6Config.cmake")
                list(PREPEND CMAKE_PREFIX_PATH "${_qt_d}")
                set(_qt_glob_matched TRUE)
                break()
            endif()
        endforeach()
    endforeach()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# ---------- 旧版多饼图（Pie_Chart_Control/）----------

set(LEGACY_SOURCES
    ${LEGACY_DIR}/main.cpp
    ${LEGACY_DIR}/widget.cpp
    ${LEGACY_DIR}/widget.h
    ${LEGACY_DIR}/VarCountData.h
    ${LEGACY_DIR}/PieChartConfig.cpp
    ${LEGACY_DIR}/PieChartConfig.h
    ${LEGACY_DIR}/PieChartConfigDialog.cpp
    ${LEGACY_DIR}/PieChartConfigDialog.h
    ${LEGACY_DIR}/SinglePieChart.cpp
    ${LEGACY_DIR}/SinglePieChart.h
)

# ---------- 旧版多饼图程序 Pie_Chart_Control ----------

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Pie_Chart_Control
        MANUAL_FINALIZATION
        ${LEGACY_SOURCES}
    )
else()
    add_executable(Pie_Chart_Control
        ${LEGACY_SOURCES}
    )
endif()

target_include_directories(Pie_Chart_Control PRIVATE ${LEGACY_DIR})
target_link_libraries(Pie_Chart_Control PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(MSVC)
    target_compile_options(Pie_Chart_Control PRIVATE /Zc:__cplusplus)
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.GasketVision)
endif()
set_target_properties(Pie_Chart_Control PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Pie_Chart_Control
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Pie_Chart_Control)
endif()

# ---------- 视觉质检 HMI（HMI/）----------

set(HMI_SOURCES
    ${HMI_DIR}/main.cpp
    ${HMI_DIR}/ui/InspectionWindow.cpp
    ${HMI_DIR}/ui/InspectionWindow.h
    ${HMI_DIR}/ui/OkNgPieChart.cpp
    ${HMI_DIR}/ui/OkNgPieChart.h
    ${HMI_DIR}/stats/OkNgStatsPanel.cpp
    ${HMI_DIR}/stats/OkNgStatsPanel.h
    ${HMI_DIR}/stats/InspectionAggregator.cpp
    ${HMI_DIR}/stats/InspectionAggregator.h
    ${HMI_DIR}/ipc/InspectionDataService.cpp
    ${HMI_DIR}/ipc/InspectionDataService.h
    ${HMI_DIR}/ipc/InspectionResult.h
    ${HMI_DIR}/ipc/HmiIpcSubscriber.cpp
    ${HMI_DIR}/ipc/HmiIpcSubscriber.h
    ${HMI_DIR}/ipc/HmiIpcSubscriberWorker.cpp
    ${HMI_DIR}/ipc/HmiIpcSubscriberWorker.h
    ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcSync.cpp
    ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcSync.h
    ${HMI_SOCKET_ENGINE_HMI_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HMI
        MANUAL_FINALIZATION
        ${HMI_SOURCES}
    )
else()
    add_executable(HMI
        ${HMI_SOURCES}
    )
endif()

target_include_directories(HMI PRIVATE
    ${HMI_DIR}
    ${HMI_DIR}/ui
    ${HMI_DIR}/stats
    ${HMI_DIR}/ipc
    ${HMI_DIR}/socket
    ${HMI_DIR}/socket/engine_hmi
    ${VISION_ENGINE_DIR}/ipc_hmi
)
target_link_libraries(HMI PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(MSVC)
    target_compile_options(HMI PRIVATE /Zc:__cplusplus)
endif()

set_target_properties(HMI PROPERTIES WIN32_EXECUTABLE TRUE)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_finalize_executable(HMI)
endif()

install(TARGETS HMI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------- HMI_Test（合成测试专用，编译期锁定 test/ 配置）----------

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HMI_Test
        MANUAL_FINALIZATION
        ${HMI_SOURCES}
    )
else()
    add_executable(HMI_Test
        ${HMI_SOURCES}
    )
endif()

target_compile_definitions(HMI_Test PRIVATE GASKETVISION_HMI_TEST=1)
target_include_directories(HMI_Test PRIVATE
    ${HMI_DIR}
    ${HMI_DIR}/ui
    ${HMI_DIR}/stats
    ${HMI_DIR}/ipc
    ${HMI_DIR}/socket
    ${HMI_DIR}/socket/engine_hmi
    ${VISION_ENGINE_DIR}/ipc_hmi
)
target_link_libraries(HMI_Test PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(MSVC)
    target_compile_options(HMI_Test PRIVATE /Zc:__cplusplus)
endif()

set_target_properties(HMI_Test PROPERTIES WIN32_EXECUTABLE TRUE)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_finalize_executable(HMI_Test)
endif()

install(TARGETS HMI_Test
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------- OpenCV（VisionEngine 使用）----------

if(NOT OpenCV_DIR)
    if(MINGW AND EXISTS "F:/opencv/mingw-install/x64/mingw/lib/OpenCVConfig.cmake")
        set(OpenCV_DIR "F:/opencv/mingw-install/x64/mingw/lib" CACHE PATH
            "MinGW OpenCV (matches Qt mingw_64 kit)")
    elseif(MSVC AND EXISTS "F:/opencv/build/x64/vc16/lib/OpenCVConfig.cmake")
        set(OpenCV_DIR "F:/opencv/build/x64/vc16/lib" CACHE PATH
            "MSVC OpenCV (matches Qt msvc2022_64 kit)")
    else()
        set(OpenCV_DIR "F:/opencv/build" CACHE PATH "Directory of OpenCVConfig.cmake")
    endif()
endif()

find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
if(OpenCV_FOUND)
    message(STATUS "OpenCV ${OpenCV_VERSION} found (${OpenCV_DIR})")
else()
    message(WARNING
        "OpenCV not found. MinGW kit: F:/opencv/mingw-install/x64/mingw/lib ; "
        "MSVC kit: F:/opencv/build/x64/vc16/lib")
endif()

# ---------- VisionEngine ----------

if(OpenCV_FOUND)
    set(VISION_ENGINE_SOURCES
        ${VISION_ENGINE_DIR}/main.cpp
        ${VISION_ENGINE_DIR}/worker/StationAlgoWorker.cpp
        ${VISION_ENGINE_DIR}/worker/StationAlgoWorker.h
        ${VISION_ENGINE_DIR}/worker/StationCommWorker.cpp
        ${VISION_ENGINE_DIR}/worker/StationCommWorker.h
        ${VISION_ENGINE_DIR}/algo/GasketInspector.cpp
        ${VISION_ENGINE_DIR}/algo/GasketInspector.h
        ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcLayout.h
        ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcSync.cpp
        ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcSync.h
        ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcPublisher.cpp
        ${VISION_ENGINE_DIR}/ipc_hmi/HmiIpcPublisher.h
        ${VISION_ENGINE_DIR}/ipc_camera/CameraIpcReader.cpp
        ${VISION_ENGINE_DIR}/ipc_camera/CameraIpcReader.h
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcSync.cpp
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcSync.h
        ${VISION_ENGINE_SOCKET_CAMERA_SOURCES}
        ${VISION_ENGINE_SOCKET_ENGINE_HMI_SOURCES}
    )

    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_add_executable(VisionEngine
            MANUAL_FINALIZATION
            ${VISION_ENGINE_SOURCES}
        )
    else()
        add_executable(VisionEngine ${VISION_ENGINE_SOURCES})
    endif()

    target_include_directories(VisionEngine PRIVATE
        ${VISION_ENGINE_DIR}
        ${VISION_ENGINE_DIR}/worker
        ${VISION_ENGINE_DIR}/algo
        ${VISION_ENGINE_DIR}/ipc_camera
        ${VISION_ENGINE_DIR}/ipc_hmi
        ${VISION_ENGINE_DIR}/socket
        ${VISION_ENGINE_DIR}/socket/camera_engine
        ${VISION_ENGINE_DIR}/socket/engine_hmi
        ${HMI_DIR}/socket/engine_hmi
        ${CAMERA_SERVICE_DIR}/ipc
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(VisionEngine PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        ${OpenCV_LIBS}
    )

    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_finalize_executable(VisionEngine)
    endif()

    install(TARGETS VisionEngine
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    set(CAMERA_SERVICE_SOURCES
        ${CAMERA_SERVICE_DIR}/main.cpp
        ${CAMERA_SERVICE_DIR}/worker/CameraGrabWorker.cpp
        ${CAMERA_SERVICE_DIR}/worker/CameraGrabWorker.h
        ${CAMERA_SERVICE_DIR}/worker/CameraPublishWorker.cpp
        ${CAMERA_SERVICE_DIR}/worker/CameraPublishWorker.h
        ${CAMERA_SERVICE_DIR}/ipc/VisionFrame.h
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcLayout.h
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcSync.cpp
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcSync.h
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcPublisher.cpp
        ${CAMERA_SERVICE_DIR}/ipc/CameraIpcPublisher.h
        ${CAMERA_SERVICE_DIR}/camera/SyntheticImageSource.cpp
        ${CAMERA_SERVICE_DIR}/camera/SyntheticImageSource.h
        ${CAMERA_SERVICE_DIR}/camera/GigEVisionCamera.cpp
        ${CAMERA_SERVICE_DIR}/camera/GigEVisionCamera.h
        ${CAMERA_SERVICE_DIR}/camera/ImageSourceFactory.cpp
        ${CAMERA_SERVICE_DIR}/camera/ImageSourceFactory.h
        ${CAMERA_SERVICE_DIR}/camera/IVisionImageSource.h
        ${CAMERA_SERVICE_SOCKET_SOURCES}
    )

    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_add_executable(CameraService
            MANUAL_FINALIZATION
            ${CAMERA_SERVICE_SOURCES}
        )
    else()
        add_executable(CameraService ${CAMERA_SERVICE_SOURCES})
    endif()

    target_include_directories(CameraService PRIVATE
        ${CAMERA_SERVICE_DIR}
        ${CAMERA_SERVICE_DIR}/worker
        ${CAMERA_SERVICE_DIR}/camera
        ${CAMERA_SERVICE_DIR}/ipc
        ${CAMERA_SERVICE_DIR}/socket
        ${CAMERA_SERVICE_DIR}/socket/camera_engine
        ${VISION_ENGINE_DIR}/socket/camera_engine
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(CameraService PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        ${OpenCV_LIBS}
    )

    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        qt_finalize_executable(CameraService)
    endif()

    install(TARGETS CameraService
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

function(piechart_copy_runtime_assets target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONFIG_DIR}/vision_engine.json"
            "$<TARGET_FILE_DIR:${target_name}>/vision_engine.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONFIG_DIR}/pie_chart_okng.json"
            "$<TARGET_FILE_DIR:${target_name}>/pie_chart_okng.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target_name}>/templates"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONFIG_DIR}/templates/fiducial_L.png"
            "$<TARGET_FILE_DIR:${target_name}>/templates/fiducial_L.png"
    )
endfunction()

function(piechart_deploy_opencv target_name)
    if(NOT OpenCV_FOUND)
        return()
    endif()
    get_filename_component(_opencv_root "${OpenCV_DIR}" DIRECTORY)
    set(_opencv_bin "${_opencv_root}/bin")
    if(NOT EXISTS "${_opencv_bin}")
        message(WARNING "OpenCV bin not found at ${_opencv_bin}; VisionEngine may fail at runtime.")
        return()
    endif()
    file(GLOB _opencv_dlls
        "${_opencv_bin}/opencv_*.dll"
        "${_opencv_bin}/libopencv_*.dll")
    foreach(_dll IN LISTS _opencv_dlls)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Deploy OpenCV: ${CMAKE_MATCH_0}"
        )
    endforeach()
endfunction()

function(piechart_deploy_qt_runtime target_name)
    if(NOT WIN32 OR NOT TARGET ${target_name})
        return()
    endif()
    get_filename_component(_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
    get_filename_component(_qt_plugins "${Qt6_DIR}/../../../plugins" ABSOLUTE)
    if(NOT EXISTS "${_qt_bin}/Qt6Core.dll")
        message(WARNING "Qt6Core.dll not found in ${_qt_bin}; ${target_name} may fail at runtime.")
        return()
    endif()
    set(_qt_dlls Qt6Core.dll)
    if(${target_name} STREQUAL HMI OR ${target_name} STREQUAL HMI_Test)
        list(APPEND _qt_dlls Qt6Gui.dll Qt6Widgets.dll)
    endif()
    if(${target_name} STREQUAL VisionEngine OR ${target_name} STREQUAL CameraService)
        list(APPEND _qt_dlls Qt6Network.dll)
    endif()
    foreach(_dll IN LISTS _qt_dlls)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_bin}/${_dll}"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Deploy Qt runtime: ${_dll}"
        )
    endforeach()
    if((${target_name} STREQUAL HMI OR ${target_name} STREQUAL HMI_Test) AND EXISTS "${_qt_plugins}/platforms/qwindows.dll")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_plugins}/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:${target_name}>/platforms/qwindows.dll"
            COMMENT "Deploy Qt platform plugin: qwindows.dll"
        )
    endif()
    get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_rt IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${_mingw_bin}/${_rt}")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_mingw_bin}/${_rt}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Deploy MinGW runtime: ${_rt}"
            )
        endif()
    endforeach()
endfunction()

piechart_copy_runtime_assets(HMI)
piechart_deploy_qt_runtime(HMI)
piechart_copy_runtime_assets(HMI_Test)
piechart_deploy_qt_runtime(HMI_Test)
if(TARGET VisionEngine)
    piechart_copy_runtime_assets(VisionEngine)
    piechart_deploy_opencv(VisionEngine)
    piechart_deploy_qt_runtime(VisionEngine)
endif()
if(TARGET CameraService)
    piechart_copy_runtime_assets(CameraService)
    piechart_deploy_opencv(CameraService)
    piechart_deploy_qt_runtime(CameraService)
endif()
