cmake_minimum_required(VERSION 3.24)
project(mh4u_native_resource_lab VERSION 0.17.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL3-3.4.18 EXCLUDE_FROM_ALL)
find_package(OpenGL REQUIRED)
add_library(mh4u_assets src/assets.cpp)
target_include_directories(mh4u_assets PUBLIC src)
add_library(cgltf STATIC src/cgltf_impl.c)
target_include_directories(cgltf PUBLIC vendor/cgltf)
add_library(mh4u_scene src/scene.cpp src/gltf.cpp src/image.cpp src/texture_pack.cpp)
target_include_directories(mh4u_scene PUBLIC src)
target_include_directories(mh4u_scene PRIVATE vendor/picosha2)
target_link_libraries(mh4u_scene PUBLIC mh4u_assets PRIVATE cgltf SDL3::SDL3)
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC vendor/imgui vendor/imgui/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL)
add_executable(mh4u-resource-lab src/viewer.cpp)
target_link_libraries(mh4u-resource-lab PRIVATE imgui)
add_library(mh4u_localization src/localization.cpp)
target_include_directories(mh4u_localization PUBLIC src PRIVATE vendor/picosha2)
target_link_libraries(mh4u-resource-lab PRIVATE mh4u_localization)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v4/catalog_identity.hpp")
    target_include_directories(mh4u-resource-lab PRIVATE assets/local/localization-native-v4)
    target_compile_definitions(mh4u-resource-lab PRIVATE MH4U_HAS_NATIVE_CATALOG=1)
endif()

# Native state1 dispatch composes canonical controller and motion owners.
add_library(mh4u_player_state1_dynamics src/player_state1_dynamics.cpp)
target_include_directories(mh4u_player_state1_dynamics PUBLIC src)
target_link_libraries(mh4u_player_state1_dynamics PUBLIC mh4u_player_selection)
add_library(mh4u_player_object_query src/player_object_query.cpp)
target_include_directories(mh4u_player_object_query PUBLIC src)
target_link_libraries(mh4u_player_object_query PUBLIC mh4u_player_controller_step mh4u_player_reference)
add_library(mh4u_player_spatial_acquisition src/player_spatial_acquisition.cpp)
target_include_directories(mh4u_player_spatial_acquisition PUBLIC src)
target_link_libraries(mh4u_player_spatial_acquisition PUBLIC mh4u_player_controller_step mh4u_player_state1_dynamics mh4u_spatial_query)
add_library(mh4u_player_update_begin src/player_update_begin.cpp)
target_include_directories(mh4u_player_update_begin PUBLIC src)
target_link_libraries(mh4u_player_update_begin PUBLIC mh4u_player_controller_step)
add_library(mh4u_player_update_accumulator src/player_update_accumulator.cpp)
target_include_directories(mh4u_player_update_accumulator PUBLIC src)
target_link_libraries(mh4u_player_update_accumulator PUBLIC mh4u_motion_transform_math)
add_library(mh4u_player_input_filter src/player_input_filter.cpp src/player_input_block.cpp)
target_include_directories(mh4u_player_input_filter PUBLIC src)
target_link_libraries(mh4u_player_input_filter PUBLIC mh4u_player_controller_step)
add_library(mh4u_player_world_lookup src/player_world_lookup.cpp)
target_include_directories(mh4u_player_world_lookup PUBLIC src)
add_library(mh4u_player_update_prefix src/player_update_prefix.cpp)
target_include_directories(mh4u_player_update_prefix PUBLIC src)
target_link_libraries(mh4u_player_update_prefix PUBLIC mh4u_player_input_filter mh4u_player_update_accumulator mh4u_player_update_begin mh4u_player_world_lookup)
add_library(mh4u_player_state1 src/player_state1.cpp)
target_include_directories(mh4u_player_state1 PUBLIC src)
target_link_libraries(mh4u_player_state1 PUBLIC mh4u_player_controller_step mh4u_player_state1_dynamics mh4u_player_object_query mh4u_player_spatial_acquisition)
foreach(state1_owner mh4u_player_state1_dynamics mh4u_player_object_query mh4u_player_spatial_acquisition mh4u_player_update_begin mh4u_player_update_accumulator mh4u_player_input_filter mh4u_player_world_lookup mh4u_player_update_prefix)
    if(MSVC)
        target_compile_options(${state1_owner} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${state1_owner} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()


if(MSVC)
    target_compile_options(mh4u_player_state1 PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(mh4u_player_state1 PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
if(MSVC)
    target_compile_options(mh4u_assets PRIVATE /W4 /utf-8)
    target_compile_options(mh4u_scene PRIVATE /W4 /utf-8)
    target_compile_options(mh4u-resource-lab PRIVATE /W4 /utf-8)
else()
    target_compile_options(mh4u_assets PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(mh4u_scene PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(mh4u-resource-lab PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(APPLE)
    target_compile_definitions(mh4u-resource-lab PRIVATE GL_SILENCE_DEPRECATION)
endif()
enable_testing()
add_library(mh4u_motion_request src/motion_request.cpp)
target_include_directories(mh4u_motion_request PUBLIC src)
add_executable(mh4u-motion-request-tests src/motion_request_tests.cpp)
target_link_libraries(mh4u-motion-request-tests PRIVATE mh4u_motion_request)
add_test(NAME motion-request-boundaries COMMAND mh4u-motion-request-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-request-v1/oracle.bin")
    add_test(NAME motion-request-arm-oracle COMMAND mh4u-motion-request-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-request-v1/oracle.bin")
endif()
foreach(request_target mh4u_motion_request mh4u-motion-request-tests)
    if(MSVC)
        target_compile_options(${request_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${request_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_motion_command src/motion_command.cpp)
target_include_directories(mh4u_motion_command PUBLIC src)
target_link_libraries(mh4u_motion_command PUBLIC mh4u_motion_request mh4u_motion_query)
add_executable(mh4u-motion-command-tests src/motion_command_tests.cpp)
target_link_libraries(mh4u-motion-command-tests PRIVATE mh4u_motion_command)
add_test(NAME motion-command-boundaries COMMAND mh4u-motion-command-tests)
add_executable(mh4u-motion-command-resource-tests src/motion_command_resource_tests.cpp)
target_include_directories(mh4u-motion-command-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-command-resource-tests PRIVATE mh4u_motion_command mh4u_motion_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-command-live-v1/arm-storage-final/oracle.bin")
    add_test(NAME motion-command-live-resources COMMAND mh4u-motion-command-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-command-live-v1/arm-storage-final/oracle.bin"
        "7792ff35a871c651225700d0b83c3f9720adbbd9f310e55cd3461eeb577e013f"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-command-v1/oracle.bin")
    add_test(NAME motion-command-arm-oracle COMMAND mh4u-motion-command-tests --oracle
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-command-v1/oracle.bin")
endif()
add_library(mh4u_motion_binding src/motion_binding.cpp)
target_include_directories(mh4u_motion_binding PUBLIC src)
target_link_libraries(mh4u_motion_binding PUBLIC mh4u_motion_request)
add_executable(mh4u-motion-binding-tests src/motion_binding_tests.cpp)
target_link_libraries(mh4u-motion-binding-tests PRIVATE mh4u_motion_binding)
add_test(NAME motion-binding-boundaries COMMAND mh4u-motion-binding-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-binding-v1/oracle.bin")
    add_test(NAME motion-binding-arm-oracle COMMAND mh4u-motion-binding-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-binding-v1/oracle.bin")
endif()
foreach(motion_target mh4u_motion_command mh4u-motion-command-tests mh4u_motion_binding mh4u-motion-binding-tests)
    if(MSVC)
        target_compile_options(${motion_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${motion_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_motion_sampler src/motion_sampler.cpp)
target_include_directories(mh4u_motion_sampler PUBLIC src)
add_library(mh4u_motion_resource src/motion_resource.cpp)
target_include_directories(mh4u_motion_resource PUBLIC src PRIVATE vendor/picosha2)
add_library(mh4u_motion_initialization src/motion_initialization.cpp)
target_include_directories(mh4u_motion_initialization PUBLIC src)
target_link_libraries(mh4u_motion_initialization PUBLIC mh4u_motion_binding mh4u_motion_sampler mh4u_motion_transform_math)
add_executable(mh4u-motion-sampler-tests src/motion_sampler_tests.cpp)
target_include_directories(mh4u-motion-sampler-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-sampler-tests PRIVATE mh4u_motion_sampler)
add_executable(mh4u-motion-resource-tests src/motion_resource_tests.cpp)
target_include_directories(mh4u-motion-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-resource-tests PRIVATE mh4u_motion_resource mh4u_motion_binding mh4u_motion_sampler)
add_executable(mh4u-motion-initialization-tests src/motion_initialization_tests.cpp)
target_link_libraries(mh4u-motion-initialization-tests PRIVATE mh4u_motion_initialization)
add_executable(mh4u-motion-initialization-resource-tests src/motion_initialization_resource_tests.cpp)
target_link_libraries(mh4u-motion-initialization-resource-tests PRIVATE mh4u_motion_initialization mh4u_motion_resource)
add_test(NAME motion-sampler-boundaries COMMAND mh4u-motion-sampler-tests)
add_test(NAME motion-resource-boundaries COMMAND mh4u-motion-resource-tests)
add_test(NAME motion-initialization-boundaries COMMAND mh4u-motion-initialization-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-sampler-v1/arm-final/oracle.bin")
    add_test(NAME motion-sampler-arm-oracle COMMAND mh4u-motion-sampler-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-sampler-v1/arm-final/oracle.bin"
        "532e018e3d4e0002a15cd2b6585751f2f9ffc7bd84dba17eb8e26e9bd87c2c6f")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-resource-v1/source-facts.bin")
    add_test(NAME motion-resource-source-facts COMMAND mh4u-motion-resource-tests --oracle
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-resource-v1/source-facts.bin"
        "33f61c70b8f8e993380e39ef25f98acbea29e9a7914c86efdf3f13acc4d8712f")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-initialization-v1/reference-final/oracle.bin")
    add_test(NAME motion-initialization-arm-oracle COMMAND mh4u-motion-initialization-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-initialization-v1/reference-final/oracle.bin")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-initialization-v1/real-oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt")
    add_test(NAME motion-initialization-real-resources COMMAND mh4u-motion-initialization-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-initialization-v1/real-oracle.bin"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
foreach(animation_target mh4u_motion_sampler mh4u_motion_resource mh4u_motion_initialization
        mh4u-motion-sampler-tests mh4u-motion-resource-tests mh4u-motion-initialization-tests mh4u-motion-initialization-resource-tests)
    if(MSVC)
        target_compile_options(${animation_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${animation_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_joint_pose src/joint_pose.cpp)
target_include_directories(mh4u_joint_pose PUBLIC src)
add_library(mh4u_motion_pending src/motion_pending.cpp)
target_include_directories(mh4u_motion_pending PUBLIC src)
target_link_libraries(mh4u_motion_pending PUBLIC mh4u_motion_initialization mh4u_joint_pose)
add_executable(mh4u-joint-pose-tests src/joint_pose_tests.cpp)
target_link_libraries(mh4u-joint-pose-tests PRIVATE mh4u_joint_pose)
add_executable(mh4u-motion-pending-tests src/motion_pending_tests.cpp)
target_link_libraries(mh4u-motion-pending-tests PRIVATE mh4u_motion_pending)
add_executable(mh4u-motion-pending-resource-tests src/motion_pending_resource_tests.cpp)
target_include_directories(mh4u-motion-pending-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-pending-resource-tests PRIVATE mh4u_motion_pending mh4u_motion_resource)
add_test(NAME joint-pose-boundaries COMMAND mh4u-joint-pose-tests)
add_test(NAME motion-pending-boundaries COMMAND mh4u-motion-pending-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-pose-v1/arm-final/oracle.bin")
    add_test(NAME joint-pose-arm-oracle COMMAND mh4u-joint-pose-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-pose-v1/arm-final/oracle.bin")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-pending-runtime-v1/arm-final/oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt")
    add_test(NAME motion-pending-real-resources COMMAND mh4u-motion-pending-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-pending-runtime-v1/arm-final/oracle.bin"
        "0048bd4f08d087312c59f7c96ff02339612d085375816d7c79801b22f6dbe67b"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
foreach(pose_target mh4u_joint_pose mh4u_motion_pending mh4u-joint-pose-tests mh4u-motion-pending-tests mh4u-motion-pending-resource-tests)
    if(MSVC)
        target_compile_options(${pose_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${pose_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_motion_transform_math src/motion_transform_math.cpp)
target_include_directories(mh4u_motion_transform_math PUBLIC src)
add_library(mh4u_motion_query src/motion_query.cpp)
target_include_directories(mh4u_motion_query PUBLIC src)
target_link_libraries(mh4u_motion_query PUBLIC mh4u_motion_sampler mh4u_motion_transform_math)
add_library(mh4u_joint_update src/joint_update.cpp)
target_include_directories(mh4u_joint_update PUBLIC src)
target_link_libraries(mh4u_joint_update PUBLIC mh4u_joint_pose mh4u_motion_sampler mh4u_motion_transform_math)
add_executable(mh4u-motion-query-tests src/motion_query_tests.cpp)
target_link_libraries(mh4u-motion-query-tests PRIVATE mh4u_motion_query)
add_executable(mh4u-motion-query-resource-tests src/motion_query_resource_tests.cpp)
target_include_directories(mh4u-motion-query-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-query-resource-tests PRIVATE mh4u_motion_query mh4u_motion_resource)
add_executable(mh4u-joint-update-tests src/joint_update_tests.cpp)
target_link_libraries(mh4u-joint-update-tests PRIVATE mh4u_joint_update)
add_test(NAME motion-query-boundaries COMMAND mh4u-motion-query-tests)
add_test(NAME joint-update-boundaries COMMAND mh4u-joint-update-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-motion-query-v1/arm-complete/oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt")
    add_test(NAME motion-query-real-resources COMMAND mh4u-motion-query-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-motion-query-v1/arm-complete/oracle.bin"
        "699799261bf45dd60445e7fa603f9e9c42390d55368527ab2083c6b035ddb603"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-update-native-v1/arm-final/oracle.bin")
    add_test(NAME joint-update-arm-oracle COMMAND mh4u-joint-update-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-update-native-v1/arm-final/oracle.bin")
endif()
foreach(update_target mh4u_motion_transform_math mh4u_motion_query mh4u_joint_update
        mh4u-motion-query-tests mh4u-motion-query-resource-tests mh4u-joint-update-tests)
    if(MSVC)
        target_compile_options(${update_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${update_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_motion_transition src/motion_transition.cpp)
target_include_directories(mh4u_motion_transition PUBLIC src)
target_link_libraries(mh4u_motion_transition PUBLIC mh4u_motion_transform_math)
add_library(mh4u_joint_output src/joint_output.cpp)
target_include_directories(mh4u_joint_output PUBLIC src)
target_link_libraries(mh4u_joint_output PUBLIC mh4u_motion_transform_math)
add_executable(mh4u-motion-transition-tests src/motion_transition_tests.cpp)
target_link_libraries(mh4u-motion-transition-tests PRIVATE mh4u_motion_transition)
add_executable(mh4u-joint-output-tests src/joint_output_tests.cpp)
target_link_libraries(mh4u-joint-output-tests PRIVATE mh4u_joint_output)
add_executable(mh4u-joint-output-resource-tests src/joint_output_resource_tests.cpp)
target_include_directories(mh4u-joint-output-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-joint-output-resource-tests PRIVATE mh4u_motion_transition mh4u_joint_output)
add_test(NAME motion-transition-boundaries COMMAND mh4u-motion-transition-tests)
add_test(NAME joint-output-boundaries COMMAND mh4u-joint-output-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-output-composition-v1/arm-final/oracle.bin")
    add_test(NAME transition-output-arm-oracle COMMAND mh4u-joint-output-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-output-composition-v1/arm-final/oracle.bin"
        "df1dd4403982caa13e6a4aa2140d5a92dfdf2e81f7fbb2bf99468f7aa1abe566")
endif()
foreach(output_target mh4u_motion_transition mh4u_joint_output mh4u-motion-transition-tests
        mh4u-joint-output-tests mh4u-joint-output-resource-tests)
    if(MSVC)
        target_compile_options(${output_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${output_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_motion_clock src/motion_clock.cpp)
target_include_directories(mh4u_motion_clock PUBLIC src)
target_link_libraries(mh4u_motion_clock PUBLIC mh4u_motion_transform_math)
add_library(mh4u_motion_application src/motion_application.cpp)
target_include_directories(mh4u_motion_application PUBLIC src)
target_link_libraries(mh4u_motion_application PUBLIC mh4u_motion_sampler mh4u_motion_transform_math)
add_library(mh4u_model_animation_frame src/model_animation_frame.cpp)
target_include_directories(mh4u_model_animation_frame PUBLIC src)
target_link_libraries(mh4u_model_animation_frame PUBLIC mh4u_motion_pending mh4u_motion_application mh4u_joint_update mh4u_motion_clock)
add_executable(mh4u-motion-application-tests src/motion_application_tests.cpp)
target_link_libraries(mh4u-motion-application-tests PRIVATE mh4u_motion_application)
add_test(NAME motion-application-boundaries COMMAND mh4u-motion-application-tests)
add_executable(mh4u-motion-application-resource-tests src/motion_application_resource_tests.cpp)
target_include_directories(mh4u-motion-application-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-application-resource-tests PRIVATE mh4u_motion_application mh4u_motion_resource)
add_executable(mh4u-model-animation-frame-tests src/model_animation_frame_tests.cpp)
target_include_directories(mh4u-model-animation-frame-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-model-animation-frame-tests PRIVATE mh4u_model_animation_frame mh4u_motion_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-animation-frame-v1/arm-third/oracle.bin")
    add_test(NAME model-animation-frame-real-resources COMMAND mh4u-model-animation-frame-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-animation-frame-v1/arm-third/oracle.bin"
        "5f0ced4b10b5707d5d0d6e41725f180d4b8145cce49a9873558deebbddb772b1"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-application-native-v1/arm-complete/oracle.bin")
    add_test(NAME motion-application-real-resources COMMAND mh4u-motion-application-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-application-native-v1/arm-complete/oracle.bin"
        "655dbe45e6f2fc9d484bf9756dd9d015da73e9b4a68959341ffae97f319eb3c2"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
add_executable(mh4u-motion-clock-tests src/motion_clock_tests.cpp)
target_include_directories(mh4u-motion-clock-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-clock-tests PRIVATE mh4u_motion_clock mh4u_motion_resource)
add_test(NAME motion-clock-boundaries COMMAND mh4u-motion-clock-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-clock-native-v1/arm-initial/oracle.bin")
    add_test(NAME motion-clock-real-resources COMMAND mh4u-motion-clock-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-clock-native-v1/arm-initial/oracle.bin"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
foreach(frame_target mh4u_motion_clock mh4u_motion_application mh4u_model_animation_frame
        mh4u-motion-clock-tests mh4u-motion-application-tests mh4u-motion-application-resource-tests mh4u-model-animation-frame-tests)
    if(MSVC)
        target_compile_options(${frame_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${frame_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
add_library(mh4u_model_resource src/model_resource.cpp)
target_include_directories(mh4u_model_resource PUBLIC src PRIVATE vendor/picosha2)
add_library(mh4u_joint_palette src/joint_palette.cpp)
target_include_directories(mh4u_joint_palette PUBLIC src)
target_link_libraries(mh4u_joint_palette PUBLIC mh4u_motion_transform_math)
add_library(mh4u_vertex_skinning src/vertex_skinning.cpp)
target_include_directories(mh4u_vertex_skinning PUBLIC src)
target_link_libraries(mh4u_vertex_skinning PUBLIC mh4u_motion_transform_math)
add_library(mh4u_native_actor src/native_actor.cpp)
add_library(mh4u_joint_bootstrap src/joint_bootstrap.cpp)
target_include_directories(mh4u_joint_bootstrap PUBLIC src)
target_link_libraries(mh4u_joint_bootstrap PUBLIC mh4u_joint_pose)
add_executable(mh4u-joint-bootstrap-tests src/joint_bootstrap_tests.cpp)
target_include_directories(mh4u-joint-bootstrap-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-joint-bootstrap-tests PRIVATE mh4u_joint_bootstrap)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-animation-bootstrap-v1/arm-complete/joint-fixtures.bin")
    add_test(NAME joint-bootstrap-original COMMAND mh4u-joint-bootstrap-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-animation-bootstrap-v1/arm-complete/joint-fixtures.bin"
        "149eb28604ac02d824f026b80043ad5d26f1e1226ff9a0b8b6aaace28b4f330f")
endif()
add_library(mh4u_motion_bank src/motion_bank.cpp)
target_include_directories(mh4u_motion_bank PUBLIC src)
add_executable(mh4u-motion-bank-tests src/motion_bank_tests.cpp)
target_include_directories(mh4u-motion-bank-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-bank-tests PRIVATE mh4u_motion_bank)
add_test(NAME motion-bank-boundaries COMMAND mh4u-motion-bank-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-motion-banks-v1/arm-lifetime/oracle.bin")
    add_test(NAME motion-bank-original COMMAND mh4u-motion-bank-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-motion-banks-v1/arm-lifetime/oracle.bin"
        "6702b6627069465dff2d6441bc5432440ad98766b32414e4bdc759b65900fd4b")
endif()
foreach(new_target mh4u_joint_bootstrap mh4u-joint-bootstrap-tests mh4u_motion_bank mh4u-motion-bank-tests mh4u-motion-command-resource-tests)
    if(MSVC)
        target_compile_options(${new_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${new_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
target_include_directories(mh4u_native_actor PUBLIC src PRIVATE vendor/picosha2)
target_link_libraries(mh4u_native_actor PUBLIC mh4u_scene mh4u_model_resource mh4u_motion_resource
    mh4u_model_animation_frame mh4u_motion_transition mh4u_joint_output mh4u_joint_palette mh4u_vertex_skinning
    mh4u_joint_bootstrap mh4u_motion_bank mh4u_motion_command)
add_executable(mh4u-native-actor-tests src/native_actor_tests.cpp)
target_link_libraries(mh4u-native-actor-tests PRIVATE mh4u_native_actor)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative")
    add_test(NAME native-actor-seeking COMMAND mh4u-native-actor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/em062/em062.mhnative"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative")
endif()
add_executable(mh4u-model-geometry-tests src/model_geometry_tests.cpp)
target_include_directories(mh4u-model-geometry-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-model-geometry-tests PRIVATE mh4u_model_resource)
add_executable(mh4u-vertex-skinning-tests src/vertex_skinning_tests.cpp)
target_include_directories(mh4u-vertex-skinning-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-vertex-skinning-tests PRIVATE mh4u_vertex_skinning mh4u_model_resource)
add_test(NAME model-geometry-boundaries COMMAND mh4u-model-geometry-tests)
add_test(NAME vertex-skinning-boundaries COMMAND mh4u-vertex-skinning-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-geometry-native-v1/source-with-joints/geometry.bin")
    add_test(NAME model-geometry-source-facts COMMAND mh4u-model-geometry-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-geometry-native-v1/source-with-joints/geometry.bin"
        "e9c6004adb2ad450feb2aaadcf5b60a123312b89c5dbd9f3546517c87da3d23b"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/one133/one133.mod")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/vertex-skinning-v1/source-final/oracle.bin")
    add_test(NAME vertex-skinning-source-algebra COMMAND mh4u-vertex-skinning-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/vertex-skinning-v1/source-final/oracle.bin"
        "7bb3042c91254d3ce41f0f7e9cd019a2dcfa112b53decee6558080ded644f43b"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/one133/one133.mod")
endif()
foreach(native_target mh4u_native_actor mh4u_vertex_skinning mh4u-model-geometry-tests mh4u-vertex-skinning-tests)
    if(MSVC)
        target_compile_options(${native_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${native_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_executable(mh4u-model-resource-tests src/model_resource_tests.cpp)
target_include_directories(mh4u-model-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-model-resource-tests PRIVATE mh4u_model_resource)
add_executable(mh4u-joint-palette-tests src/joint_palette_tests.cpp)
target_link_libraries(mh4u-joint-palette-tests PRIVATE mh4u_joint_palette)
add_executable(mh4u-joint-palette-resource-tests src/joint_palette_resource_tests.cpp)
target_include_directories(mh4u-joint-palette-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-joint-palette-resource-tests PRIVATE mh4u_joint_palette mh4u_model_resource)
add_test(NAME model-resource-boundaries COMMAND mh4u-model-resource-tests)
add_test(NAME joint-palette-boundaries COMMAND mh4u-joint-palette-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-resource-v1/source-final/source-facts.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin")
    add_test(NAME model-resource-source-facts COMMAND mh4u-model-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/model-resource-v1/source-final/source-facts.bin"
        "de5f0e3faa5a6ef440a6b7182ff702fca10ebb25ee2fadb9ef0704e91224b061"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-palette-composition-v1/arm-write-observed/oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin")
    add_test(NAME joint-palette-source-composition COMMAND mh4u-joint-palette-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/joint-palette-composition-v1/arm-write-observed/oracle.bin"
        "e4e33ecaad1e411566a9b5bb5a4628819640c24f12f89201c2523c596bc405b0"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/actor-probe/em062.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/player-probe/body001/source/entry-44780.bin")
endif()
foreach(palette_target mh4u_model_resource mh4u_joint_palette mh4u-model-resource-tests
        mh4u-joint-palette-tests mh4u-joint-palette-resource-tests)
    if(MSVC)
        target_compile_options(${palette_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${palette_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_library(mh4u_input_quantization src/input_quantization.cpp)
target_include_directories(mh4u_input_quantization PUBLIC src)
add_library(mh4u_input_filter src/input_filter.cpp)
target_include_directories(mh4u_input_filter PUBLIC src)
add_executable(mh4u-input-filter-tests src/input_filter_tests.cpp)
target_link_libraries(mh4u-input-filter-tests PRIVATE mh4u_input_filter mh4u_input_quantization)
add_test(NAME input-filter-boundaries COMMAND mh4u-input-filter-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/filter-oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/normalizer-oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/composition-oracle.bin")
    add_test(NAME input-filter-arm-oracle COMMAND mh4u-input-filter-tests
        --filter-oracle "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/filter-oracle.bin"
        --normalizer-oracle "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/normalizer-oracle.bin"
        --composition-oracle "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-filter-v1/composition-oracle.bin")
endif()
foreach(filter_target mh4u_input_filter mh4u-input-filter-tests)
    if(MSVC)
        target_compile_options(${filter_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${filter_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_executable(mh4u-input-quantization-tests src/input_quantization_tests.cpp)
target_link_libraries(mh4u-input-quantization-tests PRIVATE mh4u_input_quantization)
add_library(mh4u_player_reference src/player_reference.cpp)
target_include_directories(mh4u_player_reference PUBLIC src)
add_library(mh4u_player_input src/player_input.cpp)
target_include_directories(mh4u_player_input PUBLIC src)
target_link_libraries(mh4u_player_input PUBLIC mh4u_player_reference)
add_executable(mh4u-player-input-tests src/player_input_tests.cpp)
target_link_libraries(mh4u-player-input-tests PRIVATE mh4u_player_input)
add_executable(mh4u-player-reference-tests src/player_reference_tests.cpp)
target_link_libraries(mh4u-player-reference-tests PRIVATE mh4u_player_input)
foreach(input_target mh4u_player_input mh4u-player-input-tests mh4u_player_reference mh4u-player-reference-tests mh4u_input_quantization mh4u-input-quantization-tests)
    if(MSVC)
        target_compile_options(${input_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${input_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME player-input-classifiers COMMAND mh4u-player-input-tests)
add_test(NAME input-quantization-boundaries COMMAND mh4u-input-quantization-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-quantization-v1/s8-oracle.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-quantization-v1/s32-oracle.bin")
    add_test(NAME input-quantization-arm-oracle COMMAND mh4u-input-quantization-tests
        --s8-oracle "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-quantization-v1/s8-oracle.bin"
        --s32-oracle "${CMAKE_CURRENT_SOURCE_DIR}/research/private/input-quantization-v1/s32-oracle.bin")
endif()
add_test(NAME player-reference-boundaries COMMAND mh4u-player-reference-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-reference-v1/oracle.bin")
    add_test(NAME player-reference-arm-oracle COMMAND mh4u-player-reference-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-reference-v1/oracle.bin")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-reference-v1/extreme-oracle.bin")
    add_test(NAME player-reference-f32-extremes COMMAND mh4u-player-reference-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-reference-v1/extreme-oracle.bin")
endif()
add_library(mh4u_player_transition src/player_transition.cpp)
add_library(mh4u_source_rng src/source_rng.cpp)
target_include_directories(mh4u_source_rng PUBLIC src)
target_include_directories(mh4u_player_transition PUBLIC src PRIVATE vendor/picosha2)
target_link_libraries(mh4u_player_transition PUBLIC mh4u_player_input mh4u_source_rng)
add_library(mh4u_player_selection src/player_selection.cpp)
target_include_directories(mh4u_player_selection PUBLIC src)
target_link_libraries(mh4u_player_selection PUBLIC mh4u_player_transition)
add_executable(mh4u-player-selection-tests src/player_selection_tests.cpp)
target_include_directories(mh4u-player-selection-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-selection-tests PRIVATE mh4u_player_selection)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-selection-runtime-v1/generated/player_selection_fixtures.hpp" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player_transition_constants.hpp")
    target_include_directories(mh4u-player-selection-tests PRIVATE
        research/private/player-selection-runtime-v1/generated research/private/player-transition-runtime-v1/generated)
    target_compile_definitions(mh4u-player-selection-tests PRIVATE MH4U_PLAYER_SELECTION_GENERATED=1)
endif()
add_test(NAME player-selection-profile COMMAND mh4u-player-selection-tests)
foreach(selection_target mh4u_player_selection mh4u-player-selection-tests)
    if(MSVC)
        target_compile_options(${selection_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${selection_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_executable(mh4u-player-transition-tests src/player_transition_tests.cpp)
add_library(mh4u_player_controller_step src/player_controller_step.cpp)
target_include_directories(mh4u_player_controller_step PUBLIC src)
target_link_libraries(mh4u_player_controller_step PUBLIC mh4u_player_selection mh4u_motion_command)
add_executable(mh4u-player-controller-step-tests src/player_controller_step_tests.cpp)
target_link_libraries(mh4u-player-controller-step-tests PRIVATE mh4u_player_controller_step)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-controller-live-v1/generated-final/player_controller_step_fixtures.hpp" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player_transition_constants.hpp")
    target_include_directories(mh4u-player-controller-step-tests PRIVATE
        research/private/player-controller-live-v1/generated-final research/private/player-transition-runtime-v1/generated)
    target_compile_definitions(mh4u-player-controller-step-tests PRIVATE MH4U_PLAYER_CONTROLLER_STEP_GENERATED=1)
endif()
add_test(NAME player-controller-step-profile COMMAND mh4u-player-controller-step-tests)
add_library(mh4u_model_animation_output src/model_animation_output.cpp)
target_include_directories(mh4u_model_animation_output PUBLIC src)
target_link_libraries(mh4u_model_animation_output PUBLIC mh4u_model_animation_frame mh4u_motion_transition mh4u_joint_output)
add_library(mh4u_player_orientation src/player_orientation.cpp)
target_include_directories(mh4u_player_orientation PUBLIC src)
target_link_libraries(mh4u_player_orientation PUBLIC mh4u_motion_transform_math)
add_library(mh4u_player_animation src/player_animation.cpp)
target_include_directories(mh4u_player_animation PUBLIC src)
target_link_libraries(mh4u_player_animation PUBLIC mh4u_player_controller_step
    mh4u_model_animation_output)
target_link_libraries(mh4u_player_animation PUBLIC mh4u_player_state1)
add_library(mh4u_player_position_bridge src/player_position_bridge.cpp)
target_include_directories(mh4u_player_position_bridge PUBLIC src)
target_link_libraries(mh4u_player_position_bridge PUBLIC mh4u_player_animation mh4u_player_orientation)
add_executable(mh4u-player-position-bridge-boundary-tests src/player_position_bridge_boundary_tests.cpp)
target_link_libraries(mh4u-player-position-bridge-boundary-tests PRIVATE mh4u_player_position_bridge)
add_test(NAME player-position-bridge-boundaries COMMAND mh4u-player-position-bridge-boundary-tests)
add_executable(mh4u-player-position-bridge-tests src/player_position_bridge_tests.cpp)
target_include_directories(mh4u-player-position-bridge-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-position-bridge-tests PRIVATE mh4u_player_position_bridge mh4u_motion_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-position-bridge-native-v1/oracle-adapter/retained-final/oracle.bin")
    add_test(NAME player-position-bridge-arm COMMAND mh4u-player-position-bridge-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-position-bridge-native-v1/oracle-adapter/retained-final/oracle.bin"
        "093867384d90a85b54b40d085e65a5cd9d02e52f416f48bd883c421e789eeaef"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()
add_executable(mh4u-player-orientation-tests src/player_orientation_tests.cpp)
target_link_libraries(mh4u-player-orientation-tests PRIVATE mh4u_player_orientation)
target_include_directories(mh4u-player-orientation-tests PRIVATE vendor/picosha2)
add_test(NAME player-orientation-boundaries COMMAND mh4u-player-orientation-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-orientation-native-v1/arm-complete/oracle.bin")
    add_test(NAME player-orientation-arm COMMAND mh4u-player-orientation-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-orientation-native-v1/arm-complete/oracle.bin"
        "89bdc81facfb3eded4e01cf546c8b2c373e235c7ad5e556881586e4ec988ede3" 69076)
endif()
foreach(placement_target mh4u_model_animation_output mh4u_player_orientation mh4u_player_position_bridge
        mh4u-player-position-bridge-boundary-tests mh4u-player-position-bridge-tests mh4u-player-orientation-tests)
    if(MSVC)
        target_compile_options(${placement_target} PRIVATE /W4 /fp:strict)
    else()
        target_compile_options(${placement_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
add_library(mh4u_native_player_actor src/player_actor.cpp)
target_include_directories(mh4u_native_player_actor PUBLIC src)
target_link_libraries(mh4u_native_player_actor PUBLIC mh4u_native_actor mh4u_player_position_bridge mh4u_player_update_prefix)
add_executable(mh4u-player-actor-tests src/player_actor_tests.cpp)
target_link_libraries(mh4u-player-actor-tests PRIVATE mh4u_player_preview)
add_library(mh4u_player_preview src/player_preview.cpp)
target_include_directories(mh4u_player_preview PUBLIC src)
target_link_libraries(mh4u_player_preview PUBLIC mh4u_native_player_actor)
target_link_libraries(mh4u-resource-lab PRIVATE mh4u_player_preview)
add_library(mh4u_player_policy_predicate src/player_policy_predicate.cpp)
target_include_directories(mh4u_player_policy_predicate PUBLIC src)
target_link_libraries(mh4u_player_policy_predicate PUBLIC mh4u_player_input)
add_executable(mh4u-player-policy-predicate-tests src/player_policy_predicate_tests.cpp)
target_include_directories(mh4u-player-policy-predicate-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-policy-predicate-tests PRIVATE mh4u_player_policy_predicate)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-predicate-v1/arm-complete/oracle.bin")
    add_test(NAME player-policy-predicate-arm COMMAND mh4u-player-policy-predicate-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-predicate-v1/arm-complete/oracle.bin"
        "f261dee0c966fb81366ff611ab06d2903fed6f21956d8ad0e20d211e0786387e")
endif()

add_library(mh4u_collision_resource src/collision_resource.cpp)
target_include_directories(mh4u_collision_resource PUBLIC src PRIVATE vendor/picosha2)
add_executable(mh4u-collision-resource-tests src/collision_resource_tests.cpp)
target_include_directories(mh4u-collision-resource-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-collision-resource-tests PRIVATE mh4u_collision_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/collision-resource-native-v1/derived-v1/oracle.bin")
    add_test(NAME collision-resource-source-facts COMMAND mh4u-collision-resource-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/collision-resource-native-v1/derived-v1/oracle.bin"
        "ae4c4df6485a7f16791f70e09f8dfcec9df6988d5da23c5441685102de13663b" "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
add_library(mh4u_spatial_face_query src/spatial_face_query.cpp)
target_include_directories(mh4u_spatial_face_query PUBLIC src)
target_link_libraries(mh4u_spatial_face_query PUBLIC mh4u_motion_transform_math)
add_executable(mh4u-spatial-face-query-tests src/spatial_face_query_tests.cpp)
target_include_directories(mh4u-spatial-face-query-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-spatial-face-query-tests PRIVATE mh4u_spatial_face_query mh4u_collision_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-face-query-native-v1/oracle-adapter/derived-v1/sidecar.bin")
    add_test(NAME spatial-face-query-arm COMMAND mh4u-spatial-face-query-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-face-query-v1/oracle.bin"
        "ddcf592af5c4ea78f8c9eb9c5df80d7eb77ed4589d22cbaef66c6f15a11cebd7"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-face-query-native-v1/oracle-adapter/derived-v1/sidecar.bin"
        "bf24d5c3c9b7141104b1d1daf3fa3db5f7a6aa8292e7fae5f7dd3c18bbcf52d2"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/v00a01-core2/source.sbc"
        "3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07")
endif()
add_library(mh4u_spatial_grid src/spatial_grid.cpp)
target_include_directories(mh4u_spatial_grid PUBLIC src)
target_link_libraries(mh4u_spatial_grid PUBLIC mh4u_collision_resource mh4u_motion_transform_math)
add_executable(mh4u-spatial-grid-tests src/spatial_grid_tests.cpp)
target_include_directories(mh4u-spatial-grid-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-spatial-grid-tests PRIVATE mh4u_spatial_grid)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-grid-native-v1/derived-final/oracle.bin")
    add_test(NAME spatial-grid-arm COMMAND mh4u-spatial-grid-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/v00a01-core2/source.sbc"
        "3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-grid-native-v1/derived-final/oracle.bin"
        "fd54e0764a060998b3bc28e6fb833d4418fc0e0b9a0ab68afa09d01bdb5ccd4a")
endif()
add_library(mh4u_spatial_query src/spatial_query.cpp)
target_include_directories(mh4u_spatial_query PUBLIC src)
target_link_libraries(mh4u_spatial_query PUBLIC mh4u_spatial_grid mh4u_spatial_face_query)
add_executable(mh4u-spatial-query-tests src/spatial_query_tests.cpp)
target_include_directories(mh4u-spatial-query-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-spatial-query-tests PRIVATE mh4u_spatial_query)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-query-native-v1/oracle-adapter/lifecycle-bounded/oracle.bin")
    add_test(NAME spatial-query-arm COMMAND mh4u-spatial-query-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-query-native-v1/oracle-adapter/retained-final/oracle.bin"
        "f0403786aa2beef1e73fc53c3f97759b1608393551c3f810d818ab1c7f750853"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-query-native-v1/oracle-adapter/contract-final/oracle.bin"
        "6001c779fa087083f9ebc4b857cc3ad4fbbdfde8efe8c85b68680f919a2e218e"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-query-native-v1/oracle-adapter/grid-final/oracle.bin"
        "f12805407ee45671734d5af7ddb6ad832b6313bc7431a9df78a43934ce26ada8"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-query-native-v1/oracle-adapter/lifecycle-bounded/oracle.bin"
        "ec5b9446afc77ec4d26ed3588eba79b710a3a204bf27ae9b423acf0cef8529d3"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/v00a01-core2/source.sbc"
        "3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07")
endif()
foreach(collision_target mh4u_collision_resource mh4u-collision-resource-tests mh4u_spatial_face_query mh4u-spatial-face-query-tests mh4u_spatial_grid mh4u-spatial-grid-tests mh4u_spatial_query mh4u-spatial-query-tests)
    if(MSVC)
        target_compile_options(${collision_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${collision_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
add_executable(mh4u-player-animation-tests src/player_animation_tests.cpp)
target_link_libraries(mh4u-player-animation-tests PRIVATE mh4u_player_animation)
set(MH4U_PLAYER_CONSTANTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player-transition-constants.bin")
set(MH4U_PLAYER_CONSTANTS_SHA "1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f")
if(EXISTS "${MH4U_PLAYER_CONSTANTS}")
    add_test(NAME player-animation-boundaries COMMAND mh4u-player-animation-tests
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}")
    add_test(NAME player-actor-real-rig COMMAND mh4u-player-actor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-animation-driver-v1/generated/player_arena_projection.hpp")
    add_executable(mh4u-player-animation-resource-tests src/player_animation_resource_tests.cpp)
    target_include_directories(mh4u-player-animation-resource-tests PRIVATE vendor/picosha2
        research/private/player-animation-driver-v1/generated)
    target_link_libraries(mh4u-player-animation-resource-tests PRIVATE mh4u_player_animation mh4u_motion_resource)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-animation-driver-v1/arm-complete/oracle.bin" AND EXISTS "${MH4U_PLAYER_CONSTANTS}")
        add_test(NAME player-animation-live-resources COMMAND mh4u-player-animation-resource-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-animation-driver-v1/arm-complete/oracle.bin"
            "16c6c82d3c1a635177c6823e21b915aa3876e3941e212803d02ae00ae6ceabad"
            "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}"
            54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
            "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
            92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
            "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
    endif()
endif()
add_executable(mh4u-player-selection-zero-tests src/player_selection_zero_tests.cpp)
target_link_libraries(mh4u-player-selection-zero-tests PRIVATE mh4u_player_controller_step mh4u_motion_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/generated-final/player_selection_zero_fixtures.hpp" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player_transition_constants.hpp")
    target_include_directories(mh4u-player-selection-zero-tests PRIVATE
        research/private/player-action-policy-v1/generated-final research/private/player-transition-runtime-v1/generated)
    target_compile_definitions(mh4u-player-selection-zero-tests PRIVATE MH4U_PLAYER_SELECTION_ZERO_GENERATED=1)
endif()
add_test(NAME player-selection-zero-profile COMMAND mh4u-player-selection-zero-tests "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(mh4u-player-selection-area-tests src/player_selection_area_tests.cpp)
target_link_libraries(mh4u-player-selection-area-tests PRIVATE mh4u_player_controller_step mh4u_motion_resource)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-selection-area-v1/generated-final/player_selection_area_fixtures.hpp")
    target_include_directories(mh4u-player-selection-area-tests PRIVATE
        research/private/player-selection-area-v1/generated-final research/private/player-transition-runtime-v1/generated)
    target_compile_definitions(mh4u-player-selection-area-tests PRIVATE MH4U_PLAYER_SELECTION_AREA_GENERATED=1)
endif()
add_test(NAME player-selection-area-profile COMMAND mh4u-player-selection-area-tests "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(driver_target mh4u_player_animation mh4u-player-animation-tests mh4u-player-animation-resource-tests mh4u-player-selection-zero-tests
        mh4u_native_player_actor mh4u-player-actor-tests mh4u_player_preview mh4u_player_policy_predicate mh4u-player-policy-predicate-tests mh4u-player-selection-area-tests)
    if(TARGET ${driver_target})
        if(MSVC)
            target_compile_options(${driver_target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${driver_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
        endif()
    endif()
endforeach()
foreach(controller_target mh4u_player_controller_step mh4u-player-controller-step-tests)
    if(MSVC)
        target_compile_options(${controller_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${controller_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
target_include_directories(mh4u-player-transition-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-transition-tests PRIVATE mh4u_player_transition)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player_transition_fixtures.hpp" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player_transition_constants.hpp")
    target_include_directories(mh4u-player-transition-tests PRIVATE research/private/player-transition-runtime-v1/generated)
    target_compile_definitions(mh4u-player-transition-tests PRIVATE MH4U_PLAYER_TRANSITION_GENERATED=1)
endif()
foreach(transition_target mh4u_player_transition mh4u-player-transition-tests)
    if(MSVC)
        target_compile_options(${transition_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${transition_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME player-transition-profile COMMAND mh4u-player-transition-tests)
add_executable(mh4u-localization-tests src/localization_tests.cpp)
target_include_directories(mh4u-localization-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-localization-tests PRIVATE mh4u_localization)
add_test(NAME localization-catalog COMMAND mh4u-localization-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v2/catalog.mhtxt")
    add_test(NAME real-localization-catalog-v2 COMMAND mh4u-localization-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v2/catalog.mhtxt"
        "e1edd7defeef717fd7a07a1088a9ee62377a068cadf4417f6b197f0ec8007d0d"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v2/lookup-oracle.tsv" 282)
endif()
add_library(mh4u_root_motion src/root_motion.cpp)
target_include_directories(mh4u_root_motion PUBLIC src)
add_executable(mh4u-root-motion-tests src/root_motion_tests.cpp)
target_link_libraries(mh4u-root-motion-tests PRIVATE mh4u_root_motion)
add_test(NAME root-motion-layer COMMAND mh4u-root-motion-tests)
add_executable(mh4u-root-motion-events-tests src/root_motion_events_tests.cpp)
target_link_libraries(mh4u-root-motion-events-tests PRIVATE mh4u_root_motion)
add_test(NAME root-motion-events COMMAND mh4u-root-motion-events-tests)
add_library(mh4u_motion_events src/motion_events.cpp)
target_include_directories(mh4u_motion_events PUBLIC src PRIVATE vendor/picosha2)
add_executable(mh4u-motion-events-tests src/motion_events_tests.cpp)
target_include_directories(mh4u-motion-events-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-motion-events-tests PRIVATE mh4u_motion_events)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/motion-event-dispatch/prefix-fixtures.hpp")
    target_include_directories(mh4u-motion-events-tests PRIVATE research/private/motion-event-dispatch)
    target_compile_definitions(mh4u-motion-events-tests PRIVATE MH4U_MOTION_PREFIX_ORACLE=1)
endif()
add_test(NAME motion-events-boundaries COMMAND mh4u-motion-events-tests)
add_library(mh4u_model_motion src/model_motion.cpp)
target_include_directories(mh4u_model_motion PUBLIC src PRIVATE vendor/picosha2)
add_library(mh4u_action_motion src/action_motion.cpp)
target_include_directories(mh4u_action_motion PUBLIC src)
target_link_libraries(mh4u_action_motion PUBLIC mh4u_model_motion mh4u_root_motion mh4u_motion_events)
add_library(mh4u_action_scene src/action_scene.cpp)
target_include_directories(mh4u_action_scene PUBLIC src)
target_link_libraries(mh4u_action_scene PUBLIC mh4u_action_motion mh4u_scene)
target_link_libraries(mh4u-resource-lab PRIVATE mh4u_action_scene)
add_executable(mh4u-action-motion src/action_motion_cli.cpp)
target_link_libraries(mh4u-action-motion PRIVATE mh4u_action_motion)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/motion-events-native-v1/motion_events_identity.hpp")
    target_include_directories(mh4u-motion-events-tests PRIVATE assets/local/motion-events-native-v1)
    target_compile_definitions(mh4u-motion-events-tests PRIVATE MH4U_MOTION_EVENTS_GENERATED=1)
    foreach(events_consumer mh4u-action-motion mh4u-resource-lab)
        target_include_directories(${events_consumer} PRIVATE assets/local/motion-events-native-v1)
        target_compile_definitions(${events_consumer} PRIVATE MH4U_HAS_MOTION_EVENTS=1)
    endforeach()
    add_test(NAME real-motion-events COMMAND mh4u-motion-events-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/motion-events-native-v1/motion-events.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/motion-events-native-v1/range-oracle.tsv")
endif()
add_executable(mh4u-model-motion-tests src/model_motion_tests.cpp)
target_include_directories(mh4u-model-motion-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-model-motion-tests PRIVATE mh4u_model_motion)
add_test(NAME model-motion-boundaries COMMAND mh4u-model-motion-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model_motion_identity.hpp")
    target_include_directories(mh4u-resource-lab PRIVATE assets/local/model-motion-native-v2)
    target_compile_definitions(mh4u-resource-lab PRIVATE MH4U_HAS_MODEL_MOTION=1)
    target_include_directories(mh4u-action-motion PRIVATE assets/local/model-motion-native-v2)
    target_compile_definitions(mh4u-action-motion PRIVATE MH4U_HAS_MODEL_MOTION=1)
    target_include_directories(mh4u-model-motion-tests PRIVATE assets/local/model-motion-native-v2)
    target_compile_definitions(mh4u-model-motion-tests PRIVATE MH4U_MODEL_MOTION_GENERATED=1)
    add_test(NAME real-model-motion-sampler COMMAND mh4u-model-motion-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model-motion.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/paired-player-actions-v1"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/sample-oracle.tsv")
endif()
add_executable(mh4u-asset-tests src/asset_tests.cpp)
target_link_libraries(mh4u-asset-tests PRIVATE mh4u_assets)
add_test(NAME asset-boundaries COMMAND mh4u-asset-tests)
add_executable(mh4u-scene-tests src/scene_tests.cpp)
target_link_libraries(mh4u-scene-tests PRIVATE mh4u_scene)
add_executable(mh4u-animation-samples src/animation_samples.cpp)
target_link_libraries(mh4u-animation-samples PRIVATE mh4u_scene)
add_test(NAME scene-math COMMAND mh4u-scene-tests)
set(MH4U_ACTOR_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/animation-probe/em062-slot002/animated.gltf")
if(EXISTS "${MH4U_ACTOR_PROBE}")
    add_test(NAME real-actor-animation COMMAND mh4u-scene-tests "${MH4U_ACTOR_PROBE}")
endif()

add_library(mh4u_spatial_triangle src/spatial_triangle.cpp)
target_include_directories(mh4u_spatial_triangle PUBLIC src)
target_link_libraries(mh4u_spatial_triangle PUBLIC mh4u_motion_transform_math)
add_executable(mh4u-spatial-triangle-tests src/spatial_triangle_tests.cpp)
target_include_directories(mh4u-spatial-triangle-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-spatial-triangle-tests PRIVATE mh4u_spatial_triangle)
add_executable(mh4u-spatial-triangle-boundary-tests src/spatial_triangle_boundary_tests.cpp)
target_link_libraries(mh4u-spatial-triangle-boundary-tests PRIVATE mh4u_spatial_triangle)
foreach(spatial_target mh4u_spatial_triangle mh4u-spatial-triangle-tests mh4u-spatial-triangle-boundary-tests)
    if(MSVC)
        target_compile_options(${spatial_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${spatial_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
add_test(NAME spatial-triangle-boundaries COMMAND mh4u-spatial-triangle-boundary-tests)
set(MH4U_SPATIAL_TRIANGLE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/research/private/spatial-triangle-native-v1/oracle-adapter/derived-v1/oracle.bin")
if(EXISTS "${MH4U_SPATIAL_TRIANGLE_FIXTURE}")
    add_test(NAME spatial-triangle-arm COMMAND mh4u-spatial-triangle-tests
        "${MH4U_SPATIAL_TRIANGLE_FIXTURE}"
        d32a6b3b6d1411eae7cc37edf9d3d24bb60b3c1c8c398eef8e423af8c12db4c6)
endif()

add_library(mh4u_collision src/collision.cpp)
target_include_directories(mh4u_collision PUBLIC src)
target_link_libraries(mh4u_collision PUBLIC mh4u_scene)
add_library(mh4u_world src/world_config.cpp src/locomotion.cpp)
target_include_directories(mh4u_world PUBLIC src)
target_link_libraries(mh4u_world PUBLIC mh4u_collision)
add_executable(mh4u-locomotion-tests src/locomotion_tests.cpp)
target_link_libraries(mh4u-locomotion-tests PRIVATE mh4u_world)
add_test(NAME locomotion-geometry COMMAND mh4u-locomotion-tests)
set(MH4U_WALLS_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/stage-composition-probe/v00a01-walls/scene.gltf")
set(MH4U_FLOOR_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/v00a01-core2/scene.gltf")
if(EXISTS "${MH4U_WALLS_PROBE}" AND EXISTS "${MH4U_FLOOR_PROBE}")
    add_test(NAME real-locomotion-geometry COMMAND mh4u-locomotion-tests "${MH4U_FLOOR_PROBE}" "${MH4U_WALLS_PROBE}")
endif()
add_executable(mh4u-collision-query src/collision_cli.cpp)
target_link_libraries(mh4u-collision-query PRIVATE mh4u_collision)
add_executable(mh4u-collision-tests src/collision_tests.cpp)
target_link_libraries(mh4u-collision-tests PRIVATE mh4u_collision)
foreach(collision_target mh4u_collision mh4u-collision-query mh4u-collision-tests)
    if(MSVC)
        target_compile_options(${collision_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${collision_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME collision-geometry COMMAND mh4u-collision-tests)
set(MH4U_COLLISION_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/v00a01-core2/scene.gltf")
if(EXISTS "${MH4U_COLLISION_PROBE}")
    add_test(NAME real-collision-geometry COMMAND mh4u-collision-tests "${MH4U_COLLISION_PROBE}")
endif()
add_library(mh4u_net src/net/socket.cpp src/net/content.cpp src/net/world.cpp src/net/world_client.cpp)
target_include_directories(mh4u_net PUBLIC src/net PRIVATE vendor/picosha2)
target_link_libraries(mh4u_net PUBLIC mh4u_world PRIVATE cgltf mh4u_native_actor)
target_link_libraries(mh4u-resource-lab PRIVATE mh4u_net)
if(WIN32)
    target_link_libraries(mh4u_net PUBLIC ws2_32)
endif()
add_executable(mh4u-network-lab src/net/main.cpp)
target_link_libraries(mh4u-network-lab PRIVATE mh4u_net)
if(MSVC)
    target_compile_options(mh4u_net PRIVATE /W4 /utf-8)
    target_compile_options(mh4u-network-lab PRIVATE /W4 /utf-8)
else()
    target_compile_options(mh4u_net PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(mh4u-network-lab PRIVATE -Wall -Wextra -Wpedantic)
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/em062/em062.mhnative")
    add_test(NAME native-actor-recipe-boundaries COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/native_actor_smoke.py"
        --binary "$<TARGET_FILE:mh4u-resource-lab>" --recipe "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/em062/em062.mhnative")
    set_tests_properties(native-actor-recipe-boundaries PROPERTIES TIMEOUT 45)
endif()
if(Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME network-process-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/network_smoke.py" --binary "$<TARGET_FILE:mh4u-network-lab>")
    set_tests_properties(network-process-smoke PROPERTIES TIMEOUT 45)
endif()
if(Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING)
    set(MH4U_WORLD_RECIPE "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/village-native-actor-v2.world.tsv")
    if(EXISTS "${MH4U_WORLD_RECIPE}")
        add_test(NAME world-process-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/world_smoke.py" --binary "$<TARGET_FILE:mh4u-resource-lab>" --recipe "${MH4U_WORLD_RECIPE}")
        set_tests_properties(world-process-smoke PROPERTIES TIMEOUT 45)
        add_test(NAME network-world-process COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/network_world_smoke.py" --binary "$<TARGET_FILE:mh4u-network-lab>" --world "${MH4U_WORLD_RECIPE}")
        add_test(NAME network-world-client-boundaries COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/network_world_client_smoke.py" --binary "$<TARGET_FILE:mh4u-network-lab>" --world "${MH4U_WORLD_RECIPE}")
        set_tests_properties(network-world-process network-world-client-boundaries PROPERTIES TIMEOUT 60)
        add_test(NAME network-membership-order COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/network_membership_smoke.py"
            --binary "$<TARGET_FILE:mh4u-network-lab>" --world "${MH4U_WORLD_RECIPE}")
        set_tests_properties(network-membership-order PROPERTIES TIMEOUT 30)
        add_test(NAME native-world-content-boundaries COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/native_world_smoke.py"
            --binary "$<TARGET_FILE:mh4u-network-lab>" --world "${MH4U_WORLD_RECIPE}")
        set_tests_properties(native-world-content-boundaries PROPERTIES TIMEOUT 45)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/village-diagnostic-v1.world.tsv")
        add_test(NAME legacy-world-content COMMAND mh4u-network-lab verify --world
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/village-diagnostic-v1.world.tsv")
    endif()
    set(MH4U_MOTION_ORACLE "${CMAKE_CURRENT_SOURCE_DIR}/research/private/root-motion-runtime/real-curves-sequences-v1")
    if(EXISTS "${MH4U_MOTION_ORACLE}/manifest.json" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model-motion.bin")
        add_test(NAME real-action-motion-sequences COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/action_motion_smoke.py"
            --binary "$<TARGET_FILE:mh4u-action-motion>" --package "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model-motion.bin"
            --paired-root "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/paired-player-actions-v1" --oracle "${MH4U_MOTION_ORACLE}")
        set_tests_properties(real-action-motion-sequences PROPERTIES TIMEOUT 45)
        add_test(NAME action-viewer-process COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/action_viewer_smoke.py"
            --viewer "$<TARGET_FILE:mh4u-resource-lab>" --action-cli "$<TARGET_FILE:mh4u-action-motion>"
            --package "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model-motion.bin"
            --paired-root "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/paired-player-actions-v1")
        set_tests_properties(action-viewer-process PROPERTIES TIMEOUT 90)
    endif()
    set(MH4U_EVENTS_ORACLE "${CMAKE_CURRENT_SOURCE_DIR}/research/private/root-motion-runtime/real-events-sequences-v1")
    if(EXISTS "${MH4U_EVENTS_ORACLE}/manifest.json" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/motion-events-native-v1/motion-events.bin")
        add_test(NAME real-action-events-sequences COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/action_events_smoke.py"
            --binary "$<TARGET_FILE:mh4u-action-motion>" --viewer "$<TARGET_FILE:mh4u-resource-lab>"
            --package "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/model-motion-native-v2/model-motion.bin"
            --package-sha256 ffb34e83d3f807272ca41b3a9d4d4cf4414735203a3cceaa4cd5d7ec2166d139
            --events-package "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/motion-events-native-v1/motion-events.bin"
            --events-sha256 7e95034eeaa35e43c8b0967ab754ff93dfcea4fbce58929cfdd824f527eac209
            --paired-root "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/paired-player-actions-v1"
            --paired-sha256 30f85c1c2707cad8406b42d3e07bc2be2945f20ee781dbec3c05de075ffca261
            --oracle "${MH4U_EVENTS_ORACLE}"
            --oracle-sha256 e446cdde7b2194f4225a6cce77d582586b1ac882bca7efebf1006b35d15890c7)
        set_tests_properties(real-action-events-sequences PROPERTIES TIMEOUT 90)
    endif()
    add_test(NAME texture-pack-process-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/texture_pack_smoke.py" --binary "$<TARGET_FILE:mh4u-resource-lab>")
    set_tests_properties(texture-pack-process-smoke PROPERTIES TIMEOUT 45)
    add_test(NAME collision-process-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/collision_smoke.py" --binary "$<TARGET_FILE:mh4u-collision-query>")
    set_tests_properties(collision-process-smoke PROPERTIES TIMEOUT 45)
    if(EXISTS "${MH4U_PLAYER_CONSTANTS}" AND EXISTS "${MH4U_WORLD_RECIPE}")
        add_test(NAME player-preview-process COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/player_preview_smoke.py"
            --binary "$<TARGET_FILE:mh4u-resource-lab>"
            --recipe "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative"
            --constants "${MH4U_PLAYER_CONSTANTS}" --world "${MH4U_WORLD_RECIPE}")
        set_tests_properties(player-preview-process PROPERTIES TIMEOUT 90)
    endif()
endif()

# Independent policy owners; not yet attached to the gameplay scheduler.
add_library(mh4u_player_policy_table src/player_policy_table.cpp)
target_include_directories(mh4u_player_policy_table PUBLIC src PRIVATE vendor/picosha2)
add_executable(mh4u-player-policy-table-tests src/player_policy_table_tests.cpp)
target_include_directories(mh4u-player-policy-table-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-policy-table-tests PRIVATE mh4u_player_policy_table)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-table-v1/final/arm/oracle.bin")
    add_test(NAME player-policy-table-arm COMMAND mh4u-player-policy-table-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-table-v1/final/arm/table.bin"
        "688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-table-v1/final/arm/oracle.bin"
        "95e39d228d4f2fa0722732ed387389a5bbb7e2a2509269348d1957c4526a7ad3")
endif()
add_library(mh4u_player_policy_timing src/player_policy_timing.cpp)
target_include_directories(mh4u_player_policy_timing PUBLIC src)
add_executable(mh4u-player-policy-timing-tests src/player_policy_timing_tests.cpp)
target_include_directories(mh4u-player-policy-timing-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-policy-timing-tests PRIVATE mh4u_player_policy_timing)
add_test(NAME player-policy-timing-profile COMMAND mh4u-player-policy-timing-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-timing-v1/arm-complete/oracle.bin")
    add_test(NAME player-policy-timing-arm COMMAND mh4u-player-policy-timing-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-timing-v1/arm-complete/oracle.bin"
        "9b7b9bbd91e2ba797c3ec89b0ec7cccc4940e621f0621513dd70bd5940c48a56")
endif()
foreach(policy_target mh4u_player_policy_table mh4u-player-policy-table-tests mh4u_player_policy_timing mh4u-player-policy-timing-tests)
    if(MSVC)
        target_compile_options(${policy_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${policy_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v3/catalog.mhtxt")
    add_executable(mh4u-localization-v3-tests src/localization_tests.cpp)
    target_compile_definitions(mh4u-localization-v3-tests PRIVATE MH4U_LOCALIZATION_GENERATED)
    target_include_directories(mh4u-localization-v3-tests PRIVATE vendor/picosha2 assets/local/localization-native-v3)
    target_link_libraries(mh4u-localization-v3-tests PRIVATE mh4u_localization)
    add_test(NAME real-localization-catalog-v3 COMMAND mh4u-localization-v3-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v3/catalog.mhtxt"
        "992d32d160cf884d548c89c4713999897987a0f8f86f253e616df53dad31b226"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v3/lookup-oracle.tsv" 615)
endif()

# Historical byte fixtures reprojected to the canonical policy owner; original
# artifacts remain immutable and independently pinned by migration metadata.
set(MH4U_POLICY_OWNER_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-owner-migration-v1/generated")
foreach(policy_fixture_target mh4u-player-transition-tests mh4u-player-selection-tests mh4u-player-controller-step-tests mh4u-player-selection-zero-tests mh4u-player-selection-area-tests mh4u-player-animation-tests mh4u-player-animation-resource-tests)
    target_include_directories(${policy_fixture_target} BEFORE PRIVATE "${MH4U_POLICY_OWNER_FIXTURES}")
endforeach()
target_link_libraries(mh4u_player_transition PUBLIC mh4u_player_policy_table)
add_library(mh4u_player_policy_interpreter src/player_policy_interpreter.cpp)
target_include_directories(mh4u_player_policy_interpreter PUBLIC src)
target_link_libraries(mh4u_player_policy_interpreter PUBLIC mh4u_player_selection mh4u_player_policy_predicate mh4u_player_policy_timing)
target_link_libraries(mh4u_native_player_actor PUBLIC mh4u_player_policy_interpreter)
add_executable(mh4u-player-policy-actor-tests src/player_policy_actor_tests.cpp)
target_link_libraries(mh4u-player-policy-actor-tests PRIVATE mh4u_player_preview)
if(EXISTS "${MH4U_PLAYER_CONSTANTS}" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/source-observed/table-f2efb8.bin")
    add_test(NAME player-policy-actor COMMAND mh4u-player-policy-actor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/source-observed/table-f2efb8.bin"
        "688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a")
endif()
add_executable(mh4u-player-policy-interpreter-tests src/player_policy_interpreter_tests.cpp)
target_include_directories(mh4u-player-policy-interpreter-tests PRIVATE vendor/picosha2 "${MH4U_POLICY_OWNER_FIXTURES}")
target_link_libraries(mh4u-player-policy-interpreter-tests PRIVATE mh4u_player_policy_interpreter)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-interpreter-v1/arm-source-order/oracle.bin")
    add_test(NAME player-policy-interpreter-arm COMMAND mh4u-player-policy-interpreter-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-interpreter-v1/arm-source-order/oracle.bin"
        "48b82ec4e7d9bc2a4a2e0450833349bbe772ec217eec18f33efcee66c3081798"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-policy-table-v1/final/arm/table.bin"
        "688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-transition-runtime-v1/generated/player-transition-constants.bin"
        "1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f")
endif()
foreach(policy_target mh4u_player_policy_interpreter mh4u-player-policy-interpreter-tests mh4u-player-policy-actor-tests)
    if(MSVC)
        target_compile_options(${policy_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${policy_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v4/catalog.mhtxt")
    add_executable(mh4u-localization-v4-tests src/localization_tests.cpp)
    target_compile_definitions(mh4u-localization-v4-tests PRIVATE MH4U_LOCALIZATION_GENERATED)
    target_include_directories(mh4u-localization-v4-tests PRIVATE vendor/picosha2 assets/local/localization-native-v4)
    target_link_libraries(mh4u-localization-v4-tests PRIVATE mh4u_localization)
    add_test(NAME real-localization-catalog-v4 COMMAND mh4u-localization-v4-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v4/catalog.mhtxt"
        "b271948651a6d7d036168d08afc67a8cb972af0d00b9761766d59417912be379"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/localization-native-v4/lookup-oracle.tsv" 925)
endif()

add_executable(mh4u-player-state1-tests src/player_state1_tests.cpp)
target_include_directories(mh4u-player-state1-tests PRIVATE vendor/picosha2 "${MH4U_POLICY_OWNER_FIXTURES}")
target_link_libraries(mh4u-player-state1-tests PRIVATE mh4u_player_state1 mh4u_motion_resource)
add_executable(mh4u-player-state1-actor-tests src/player_state1_actor_tests.cpp)
target_link_libraries(mh4u-player-state1-actor-tests PRIVATE mh4u_player_preview)
add_executable(mh4u-player-state1-continuation-tests src/player_state1_continuation_tests.cpp)
target_include_directories(mh4u-player-state1-continuation-tests PRIVATE vendor/picosha2 "${MH4U_POLICY_OWNER_FIXTURES}")
target_link_libraries(mh4u-player-state1-continuation-tests PRIVATE mh4u_player_state1 mh4u_motion_resource)
add_executable(mh4u-player-state1-id2-tests src/player_state1_id2_tests.cpp)
target_include_directories(mh4u-player-state1-id2-tests PRIVATE vendor/picosha2 "${MH4U_POLICY_OWNER_FIXTURES}")
target_link_libraries(mh4u-player-state1-id2-tests PRIVATE mh4u_player_state1 mh4u_motion_resource)
add_executable(mh4u-player-state1-dynamics-tests src/player_state1_dynamics_tests.cpp)
target_link_libraries(mh4u-player-state1-dynamics-tests PRIVATE mh4u_player_state1_dynamics)
add_test(NAME player-state1-dynamics-boundaries COMMAND mh4u-player-state1-dynamics-tests)
add_executable(mh4u-player-spatial-offset-tests src/player_spatial_offset_tests.cpp)
target_include_directories(mh4u-player-spatial-offset-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-spatial-offset-tests PRIVATE mh4u_player_state1_dynamics)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-spatial-offset-native-v1/arm-final/oracle.bin")
    add_test(NAME player-spatial-offset-arm COMMAND mh4u-player-spatial-offset-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-spatial-offset-native-v1/arm-final/oracle.bin"
        "6eb652ed2b2b41d4ee9588cf94aa1a442711d83de57530a726e64dbd887a9835")
endif()
add_executable(mh4u-player-spatial-acquisition-tests src/player_spatial_acquisition_tests.cpp)
target_include_directories(mh4u-player-spatial-acquisition-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-spatial-acquisition-tests PRIVATE mh4u_player_spatial_acquisition)
add_executable(mh4u-player-update-begin-tests src/player_update_begin_tests.cpp)
target_include_directories(mh4u-player-update-begin-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-update-begin-tests PRIVATE mh4u_player_update_begin)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-update-begin-v1/arm-final/oracle.bin")
    add_test(NAME player-update-begin-arm COMMAND mh4u-player-update-begin-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-update-begin-v1/arm-final/oracle.bin"
        "24625ce5de83912d2f338a5ea3edc0e4c1781df1a4ebf8e62a6a90491b2749fe")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-spatial-acquisition-v1/derived-indexed/oracle.bin")
    add_test(NAME player-spatial-acquisition-arm COMMAND mh4u-player-spatial-acquisition-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-spatial-acquisition-v1/derived-indexed/oracle.bin"
        "4f234709fa3b9bf0cf8f3fb3a5f342a1ca353456d161901aa7d0415afbe69e56"
        "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/oracle-adapter/derived-v1/oracle.bin")
    add_test(NAME player-state1-continuations-arm COMMAND mh4u-player-state1-continuation-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/oracle-adapter/derived-v1/oracle.bin"
        "b304c4760b13938787921e427ad8f7b78dc8ab853f2139fade52e0ecb26dbcf3"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"
        92610 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-motion-banks/lmt/92610.lmt"
        "6dc7bd0b51f5c47722883efbaff8a95fc2504ebfc0e436a0e0f811ba51e04274")
endif()
if(EXISTS "${MH4U_PLAYER_CONSTANTS}" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/source-observed/table-f2efb8.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-runtime-v1/inputs/table-f2f2d0.bin" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/actor-fixture/hunter-seven-co2.mhnative" AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/multigroup-search/86059.sbc")
    add_test(NAME player-state1-actor COMMAND mh4u-player-state1-actor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/actor-fixture/hunter-seven-co2.mhnative"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/source-observed/table-f2efb8.bin"
        "688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-runtime-v1/inputs/table-f2f2d0.bin"
        "c7eacbb0116b97e7f88ad675e109e27675ea93791f4112928ccd1e8fe6f0e310"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/local/collision-probe/multigroup-search/86059.sbc"
        "a024dcc555ed3dd4df965e762f24349d4112ec0fe8b4630a254e84dbd1e48fb8")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-id2-native-v1/oracle-adapter/derived-v2/oracle.bin")
    add_test(NAME player-state1-id2-arm COMMAND mh4u-player-state1-id2-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-id2-native-v1/oracle-adapter/derived-v2/oracle.bin"
        "84c5ccae6be34807edc4d7764d1e1cfb45967e0c89cafb49f57f22b43eb0ea06"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"
        92610 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-motion-banks/lmt/92610.lmt"
        "6dc7bd0b51f5c47722883efbaff8a95fc2504ebfc0e436a0e0f811ba51e04274")
endif()
foreach(state1_target mh4u-player-state1-tests mh4u-player-state1-actor-tests mh4u-player-state1-continuation-tests mh4u-player-state1-id2-tests mh4u-player-state1-dynamics-tests mh4u-player-spatial-offset-tests mh4u-player-spatial-acquisition-tests)
    if(MSVC)
        target_compile_options(${state1_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${state1_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-runtime-v1/fixture-v2/oracle.bin")
    add_test(NAME player-state1-entry-arm COMMAND mh4u-player-state1-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-runtime-v1/fixture-v2/oracle.bin"
        "5a48fa8dcf24f45cfe45456115e122eded7a7abdde7726608af32a198ce0ce0c"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-action-policy-v1/source-observed/table-f2efb8.bin"
        "688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a"
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-runtime-v1/inputs/table-f2f2d0.bin"
        "c7eacbb0116b97e7f88ad675e109e27675ea93791f4112928ccd1e8fe6f0e310"
        54913 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-54913.lmt"
        "dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a"
        92608 "${CMAKE_CURRENT_SOURCE_DIR}/research/private/lmt-events-v1/sources/entry-92608.lmt"
        "c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")
endif()

add_executable(mh4u-player-update-accumulator-tests src/player_update_accumulator_tests.cpp)
target_include_directories(mh4u-player-update-accumulator-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-update-accumulator-tests PRIVATE mh4u_player_update_accumulator)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-update-accumulator-v1/arm-final/oracle.bin")
    add_test(NAME player-update-accumulator-arm COMMAND mh4u-player-update-accumulator-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-update-accumulator-v1/arm-final/oracle.bin"
        "58ae1b9b255e0b3e4e8e7b0b21831431562a1d2af767ba35d58b7810cdbd1a47")
endif()
add_executable(mh4u-player-input-block-tests src/player_input_block_tests.cpp)
target_link_libraries(mh4u-player-input-block-tests PRIVATE mh4u_player_input_filter)
add_test(NAME player-input-block-boundaries COMMAND mh4u-player-input-block-tests)
add_executable(mh4u-player-input-filter-tests src/player_input_filter_tests.cpp)
target_include_directories(mh4u-player-input-filter-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-input-filter-tests PRIVATE mh4u_player_input_filter)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-input-filter-v1/derived-final/oracle.bin")
    add_test(NAME player-input-filter-arm COMMAND mh4u-player-input-filter-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-input-filter-v1/derived-final/oracle.bin"
        "b407c95947f88a2d5f39d8f6f1c558cb32b81de2e2fa64e4282e381eecf48961")
endif()
add_executable(mh4u-player-update-preparation-actor-tests src/player_update_preparation_actor_tests.cpp)
target_link_libraries(mh4u-player-update-preparation-actor-tests PRIVATE mh4u_native_player_actor)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/actor-fixture/hunter-seven-co2.mhnative" AND EXISTS "${MH4U_PLAYER_CONSTANTS}")
    add_test(NAME player-update-preparation-actor COMMAND mh4u-player-update-preparation-actor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-state1-continuation-native-v1/actor-fixture/hunter-seven-co2.mhnative"
        "${MH4U_PLAYER_CONSTANTS}" "${MH4U_PLAYER_CONSTANTS_SHA}")
endif()

add_executable(mh4u-player-world-lookup-tests src/player_world_lookup_tests.cpp)
target_include_directories(mh4u-player-world-lookup-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-world-lookup-tests PRIVATE mh4u_player_world_lookup mh4u_player_policy_table)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-world-lookup-v1/derived-first/oracle.bin")
    add_test(NAME player-world-lookup-arm COMMAND mh4u-player-world-lookup-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-world-lookup-v1/derived-first/oracle.bin"
        "54ab91648a311d27c96bf310e0dcd18885c602512d100608cbf561e9dd59e08c")
endif()
add_executable(mh4u-player-update-prefix-tests src/player_update_prefix_tests.cpp)
target_include_directories(mh4u-player-update-prefix-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-player-update-prefix-tests PRIVATE mh4u_player_update_prefix)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-world-lookup-v1/update-prefix-oracle/derived-first/oracle.bin")
    add_test(NAME player-update-prefix-arm COMMAND mh4u-player-update-prefix-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/player-world-lookup-v1/update-prefix-oracle/derived-first/oracle.bin"
        "32acf17318c0fedd11a7c30edd7ab539bd1de1412cdde7e0fd18bc70e56428dc")
endif()
foreach(world_target mh4u-player-world-lookup-tests mh4u-player-update-prefix-tests mh4u-player-update-preparation-actor-tests)
    if(MSVC)
        target_compile_options(${world_target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${world_target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
endforeach()

add_library(mh4u_item_pop_resources src/item_pop_resources.cpp)
target_include_directories(mh4u_item_pop_resources PUBLIC src PRIVATE vendor/picosha2)
add_executable(mh4u-item-pop-resources-tests src/item_pop_resources_tests.cpp)
target_include_directories(mh4u-item-pop-resources-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-item-pop-resources-tests PRIVATE mh4u_item_pop_resources)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/item-pop-resources-v1/derived-first/oracle.bin")
    add_test(NAME item-pop-resources-source COMMAND mh4u-item-pop-resources-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/item-pop-resources-v1/derived-first/oracle.bin"
        "984c8f5f4ecd5df614c296e991084ec930113c202c8abca8966ffdda9912851b"
        "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
foreach(item_pop_target mh4u_item_pop_resources mh4u-item-pop-resources-tests)
    if(MSVC)
        target_compile_options(${item_pop_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${item_pop_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

add_library(mh4u_item_pop_builder src/item_pop_builder.cpp)
target_include_directories(mh4u_item_pop_builder PUBLIC src)
target_link_libraries(mh4u_item_pop_builder PUBLIC mh4u_item_pop_resources mh4u_player_selection)
add_executable(mh4u-source-primitives-tests src/source_rng_tests.cpp)
target_include_directories(mh4u-source-primitives-tests PRIVATE vendor/picosha2)
target_link_libraries(mh4u-source-primitives-tests PRIVATE mh4u_player_selection)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/research/private/item-pop-builder-v2/shared-primitives/arm-first/oracle.bin")
    add_test(NAME source-primitives-arm-oracle COMMAND mh4u-source-primitives-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/research/private/item-pop-builder-v2/shared-primitives/arm-first/oracle.bin"
        "1644c045636b2aeab55d456ed7c67263654aec2d540d0de4167fac813f849ab5")
endif()
foreach(builder_target mh4u_item_pop_builder mh4u_source_rng mh4u-source-primitives-tests)
    if(MSVC)
        target_compile_options(${builder_target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${builder_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
