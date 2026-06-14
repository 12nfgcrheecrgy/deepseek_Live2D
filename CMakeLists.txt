cmake_minimum_required(VERSION 3.20)
project(Live2DCompanion VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_DIR ${PROJECT_ROOT}/Lib)
set(LIB_INCLUDE_DIR ${LIB_DIR}/include)
set(LIVE2D_SDK_DIR ${PROJECT_ROOT}/CubismSdkForNative)
set(LIVE2D_FRAMEWORK_DIR ${LIVE2D_SDK_DIR}/Framework)
set(LIVE2D_CORE_DIR ${LIVE2D_SDK_DIR}/Core) 
set(FRAMEWORK_SHADER_PATH ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/Shaders)

if(NOT EXISTS ${LIVE2D_SDK_DIR}/Core/include/Live2DCubismCore.h)
    message(FATAL_ERROR "Live2D Cubism SDK not found at ${LIVE2D_SDK_DIR}.\n"
        "Please download Cubism SDK for Native from https://www.live2d.com/en/download/cubism-sdk/")
endif()

add_definitions(-DCSM_DEBUG_MEMORY_LEAK=0)
add_definitions(-DCSM_TARGET_WIN_D3D11)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(LIVE2D_CORE_DIR ${LIVE2D_SDK_DIR}/Core)
set(LIVE2D_FRAMEWORK_DIR ${LIVE2D_SDK_DIR}/Framework)

set(LIVE2D_INCLUDE_DIRS
    ${LIVE2D_CORE_DIR}/include
    ${LIVE2D_FRAMEWORK_DIR}/src
)

set(LIVE2D_SOURCES
    ${LIVE2D_FRAMEWORK_DIR}/src/CubismCdiJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/CubismDefaultParameterId.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/CubismFramework.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/CubismModelSettingJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Effect/CubismBreath.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Effect/CubismEyeBlink.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Effect/CubismLook.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Effect/CubismPose.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Id/CubismId.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Id/CubismIdManager.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismMath.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismMatrix44.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismModelMatrix.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismTargetPoint.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismVector2.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Math/CubismViewMatrix.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismMoc.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismModel.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismModelMultiplyAndScreenColor.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismModelUserData.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismModelUserDataJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Model/CubismUserModel.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/ACubismMotion.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismBreathUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismExpressionMotion.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismExpressionMotionManager.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismExpressionUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismEyeBlinkUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismLipSyncUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismLookUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismMotion.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismMotionJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismMotionManager.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismMotionQueueEntry.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismMotionQueueManager.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismPhysicsUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismPoseUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/CubismUpdateScheduler.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/ICubismUpdater.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Motion/IParameterProvider.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Physics/CubismPhysics.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Physics/CubismPhysicsJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/csmBlendMode.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/CubismRenderer.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismDeviceInfo_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismOffscreenManager_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismOffscreenRenderTarget_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismOffscreenSurface_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismRenderer_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismRenderState_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismRenderTarget_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Rendering/D3D11/CubismShader_D3D11.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Type/csmRectF.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Type/csmString.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Utils/CubismDebug.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Utils/CubismJson.cpp
    ${LIVE2D_FRAMEWORK_DIR}/src/Utils/CubismString.cpp
)

set(LIVE2D_CORE_DLL_DIR ${LIVE2D_CORE_DIR}/dll/windows/x86_64)

if(MSVC)
    # MSVC: use static library
    set(LIVE2D_CORE_LIB_DIR ${LIVE2D_CORE_DIR}/lib/windows/x86_64/143)
    set(LIVE2D_LIB
        optimized ${LIVE2D_CORE_LIB_DIR}/Live2DCubismCore_MT.lib
        debug ${LIVE2D_CORE_LIB_DIR}/Live2DCubismCore_MTd.lib
    )
else()
    # MinGW/GCC: use DLL import library (generated from Live2DCubismCore.dll)
    set(LIVE2D_LIB ${LIVE2D_CORE_DLL_DIR}/libLive2DCubismCore.a)
endif()

set(STB_IMAGE_HEADER ${LIB_INCLUDE_DIR}/stb/stb_image.h)

set(STB_IMAGE_IMPLEMENTATION ${PROJECT_ROOT}/src/stb_image_impl.cpp)

set(APP_SOURCES
    ${PROJECT_ROOT}/src/main.cpp
    ${PROJECT_ROOT}/src/Config.cpp
    ${PROJECT_ROOT}/src/ConfigJson.cpp
    ${PROJECT_ROOT}/src/CompanionWindow.cpp
    ${PROJECT_ROOT}/src/LAppDelegate.cpp
    ${PROJECT_ROOT}/src/LAppView.cpp
    ${PROJECT_ROOT}/src/LAppPal.cpp
    ${PROJECT_ROOT}/src/LAppModel.cpp
    ${PROJECT_ROOT}/src/Live2DManager.cpp
    ${PROJECT_ROOT}/src/AudioManager.cpp
    ${PROJECT_ROOT}/src/DeepSeekClient.cpp
    ${PROJECT_ROOT}/src/TTSManager.cpp
    ${PROJECT_ROOT}/src/OCRManager.cpp
    ${PROJECT_ROOT}/src/ScreenCapture.cpp
    ${PROJECT_ROOT}/src/ExpressionManager.cpp
    ${PROJECT_ROOT}/src/MotionManager.cpp
    ${PROJECT_ROOT}/src/PythonBridge.cpp
    ${PROJECT_ROOT}/src/ChatDialog.cpp
    ${STB_IMAGE_IMPLEMENTATION}
    ${LIVE2D_SOURCES}
)

set(APP_HEADERS
    ${PROJECT_ROOT}/src/Config.h
    ${PROJECT_ROOT}/src/CompanionWindow.h
    ${PROJECT_ROOT}/src/LAppDelegate.h
    ${PROJECT_ROOT}/src/LAppView.h
    ${PROJECT_ROOT}/src/LAppPal.h
    ${PROJECT_ROOT}/src/LAppModel.h
    ${PROJECT_ROOT}/src/Live2DManager.h
    ${PROJECT_ROOT}/src/AudioManager.h
    ${PROJECT_ROOT}/src/DeepSeekClient.h
    ${PROJECT_ROOT}/src/TTSManager.h
    ${PROJECT_ROOT}/src/OCRManager.h
    ${PROJECT_ROOT}/src/ScreenCapture.h
    ${PROJECT_ROOT}/src/ExpressionManager.h
    ${PROJECT_ROOT}/src/MotionManager.h
    ${PROJECT_ROOT}/src/PythonBridge.h
    ${PROJECT_ROOT}/src/ChatDialog.h
    ${PROJECT_ROOT}/src/utils.h
)

add_executable(Live2DCompanion WIN32
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(Live2DCompanion PRIVATE
    ${PROJECT_ROOT}/src
    ${LIVE2D_INCLUDE_DIRS}
    ${LIB_INCLUDE_DIR}
    ${LIB_INCLUDE_DIR}/cpp-httplib
    ${LIB_INCLUDE_DIR}/nlohmann
    ${LIB_INCLUDE_DIR}/stb
    # DirectXMath compat (Windows SDK's DirectXMath.h + sal.h for MinGW)
    ${LIB_INCLUDE_DIR}/directxmath-compat
)

target_link_libraries(Live2DCompanion PRIVATE
    d3d11
    dxgi
    dxguid
    d3dcompiler
    winmm
    comctl32
    ${LIVE2D_LIB}
)

if(MSVC)
    target_compile_options(Live2DCompanion PRIVATE /W3 /EHsc /utf-8)
    target_link_options(Live2DCompanion PRIVATE /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(Live2DCompanion PRIVATE -Wall -Wextra)
    target_link_options(Live2DCompanion PRIVATE -mwindows)
endif()

set_target_properties(Live2DCompanion PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_ROOT}/bin/$<CONFIG>
)

add_custom_command(TARGET Live2DCompanion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${PROJECT_ROOT}/bin/$<CONFIG>/python
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_ROOT}/python
    ${PROJECT_ROOT}/bin/$<CONFIG>/python
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${PROJECT_ROOT}/config.json
    ${PROJECT_ROOT}/bin/$<CONFIG>/config.json
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_ROOT}/Resources
    ${PROJECT_ROOT}/bin/$<CONFIG>/Resources
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${PROJECT_ROOT}/bin/$<CONFIG>/FrameworkShaders
    COMMAND ${CMAKE_COMMAND} -E remove_directory
    ${PROJECT_ROOT}/bin/$<CONFIG>/FrameworkShaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${FRAMEWORK_SHADER_PATH}
    ${PROJECT_ROOT}/bin/$<CONFIG>/FrameworkShaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${LIVE2D_CORE_DLL_DIR}/Live2DCubismCore.dll
    ${PROJECT_ROOT}/bin/$<CONFIG>/Live2DCubismCore.dll
)