cmake_minimum_required(VERSION 3.22)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ravennakit/cmake/helpers.cmake")
    message(FATAL_ERROR "Please replace the folder 'ravennakit' with the actual RAVENNAKIT SDK. See README.md for more details.")
endif ()

include(cmake/version.cmake)
include(ravennakit/cmake/helpers.cmake)
include(FetchContent)

project(ravennakit_juce_demo VERSION ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH} LANGUAGES C CXX)

#####################
# Project variables #
#####################

add_project_variable(PROJECT_COMPANY_NAME "Sound on Digital")
add_project_variable(PROJECT_PRODUCT_NAME "RAVENNAKIT Demo")
add_project_variable(PROJECT_BUNDLE_ID "com.soundondigital.ravennakit-juce-demo")
add_project_variable(PROJECT_URL "https://soundondigital.com/")
add_project_variable(PROJECT_SUPPORT_URL "https://soundondigital.com/support/")
add_project_variable(PROJECT_EMAIL "support@soundondigital.com")
add_project_variable(PROJECT_VERSION_STRING ${GIT_DESCRIBE_VERSION})

##########################
# Compiler configuration #
##########################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] "YES")
set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=RelWithDebInfo] "YES")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE INTERNAL "")
    add_link_options("/NODEFAULTLIB:libcmt.lib" "/NODEFAULTLIB:libcmtd.lib")
endif ()

#########
# VCPKG #
#########

if (WIN32)
    find_package(ASIOSDK REQUIRED REQUIRED)
endif ()

##############
# RAVENNAKIT #
##############

add_subdirectory(ravennakit)

########
# JUCE #
########

FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
)

set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

FetchContent_MakeAvailable(JUCE)

set(JUCE_COMMON_TARGET_DEFINITIONS
        DONT_SET_USING_JUCE_NAMESPACE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1
        JUCE_DISABLE_COREGRAPHICS_FONT_SMOOTHING=1
        JUCE_COREGRAPHICS_DRAW_ASYNC=1
        JUCE_CATCH_UNHANDLED_EXCEPTIONS=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        NOMINMAX
        JUCE_ENABLE_REPAINT_DEBUGGING=0
        JUCE_IOS_AUDIO_LOGGING=0
        JucePlugin_AAXDisableDynamicProcessing=1
        JUCE_DIRECTSOUND=0
        JUCE_WASAPI=1
        JUCE_ASIO=1
)

###################
# Audio Resampler #
###################

add_library(audio-resampler STATIC
        third_party/dbry/audio-resampler/biquad.h
        third_party/dbry/audio-resampler/biquad.c
        third_party/dbry/audio-resampler/decimator.h
        third_party/dbry/audio-resampler/decimator.c
        third_party/dbry/audio-resampler/resampler.h
        third_party/dbry/audio-resampler/resampler.c
)

target_include_directories(audio-resampler PUBLIC third_party/dbry)

####################
# Demo application #
####################

juce_add_gui_app(ravennakit_juce_demo
        PRODUCT_NAME ${PROJECT_PRODUCT_NAME}
        COMPANY_NAME ${PROJECT_COMPANY_NAME}
        BUNDLE_ID ${PROJECT_BUNDLE_ID}
)

file(GLOB_RECURSE SOURCE_FILES source/*.hpp source/*.cpp)

target_sources(ravennakit_juce_demo PRIVATE ${SOURCE_FILES})

target_include_directories(ravennakit_juce_demo PRIVATE source)

if (WIN32)
    target_include_directories(ravennakit_juce_demo PRIVATE ${ASIOSDK_INCLUDE_DIR})
endif ()

target_compile_definitions(ravennakit_juce_demo PUBLIC ${JUCE_COMMON_TARGET_DEFINITIONS})

if (WIN32)
    target_compile_definitions(ravennakit_juce_demo PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
    )
endif ()

target_link_libraries(ravennakit_juce_demo
        PUBLIC
        juce::juce_recommended_warning_flags
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags

        PRIVATE
        # JUCE
        juce::juce_core
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_cryptography
        ravennakit
        audio-resampler
)
