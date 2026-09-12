# Asset/KB-independent C++ contract selection for the clean public snapshot.
#
# This file intentionally does not include the repository's broad
# cmake/Testing.cmake: that catalog contains oracle/content/runtime tests.
# Every target below reuses an existing test and its existing implementation
# TU(s); no fixture or behavior implementation is duplicated here.

function(mmx_public_add_contract TARGET TEST_NAME)
    add_executable("${TARGET}" ${ARGN})
    target_include_directories("${TARGET}" PRIVATE
        "${MMX_PUBLIC_SOURCE_ROOT}/src")
    set_target_properties("${TARGET}" PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    # These contracts use assert(), including assertions with side effects.
    # Keep them live even when the caller chooses a Release configuration.
    if(MSVC)
        target_compile_options("${TARGET}" PRIVATE /UNDEBUG)
    else()
        target_compile_options("${TARGET}" PRIVATE -UNDEBUG)
    endif()

    add_test(NAME "${TEST_NAME}"
        COMMAND "$<TARGET_FILE:${TARGET}>")
    set_tests_properties("${TEST_NAME}" PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        LABELS "public;contract;asset-independent")
endfunction()

function(mmx_public_add_json_contract TARGET TEST_NAME)
    mmx_public_add_contract("${TARGET}" "${TEST_NAME}" ${ARGN})
    target_link_libraries("${TARGET}" PRIVATE nlohmann_json::nlohmann_json)
endfunction()

# Static/path and input contracts: no file reads, ROM, content, or KB.
mmx_public_add_contract(randomizer_contract_test randomizer.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/randomizer_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/x1_catalog.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/systems/randomizer.cpp")

mmx_public_add_contract(stage_identity_contract_test stage-identity.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/stage_identity_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/stage_identity.cpp")

mmx_public_add_contract(mmx1_password_contract_test mmx1-password.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/mmx1_password_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/mmx1_password.cpp")

mmx_public_add_contract(password_entry_contract_test password-entry.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/password_entry_contract_test.cpp")

mmx_public_add_contract(autotest_armor_mask_contract_test autotest-armor-mask.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/autotest_armor_mask_contract_test.cpp")

# Source-backed std-only model kernels. Their adapters own content/KB/terrain
# lookup and are intentionally outside these tests.
mmx_public_add_contract(intro_robot_scheduler_contract_test intro-robot-scheduler.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/intro_robot_scheduler_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/intro_robot_scheduler.cpp")

mmx_public_add_contract(linked_enemy_child_model_contract_test linked-enemy-child-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/linked_enemy_child_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/linked_enemy_child_model.cpp")

mmx_public_add_contract(ride_armor_model_contract_test ride-armor-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/ride_armor_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/ride_armor_model.cpp")

mmx_public_add_contract(mine_cart_model_contract_test mine-cart-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/mine_cart_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/mine_cart_model.cpp")

mmx_public_add_contract(moving_platform_model_contract_test moving-platform-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/moving_platform_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/moving_platform_model.cpp")

mmx_public_add_contract(underwater_vertical_model_contract_test underwater-vertical-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/underwater_vertical_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/underwater_vertical_model.cpp")

mmx_public_add_contract(falling_rock_model_contract_test falling-rock-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/falling_rock_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/falling_rock_model.cpp")

mmx_public_add_contract(midboss_behavior_model_contract_test midboss-behavior-model.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/midboss_behavior_model_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/entities/midboss_behavior_model.cpp")

# Settings contract writes only a temporary JSON under the CTest binary
# directory; it does not open content/ or knowledge_base/.
mmx_public_add_json_contract(settings_contract_test settings.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/settings_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/settings.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/localization.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/json_io.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/difficulty.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/app/input_bindings.cpp")
mmx_public_add_json_contract(json_io_contract_test json-io.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/json_io_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/data/json_io.cpp")

mmx_public_add_contract(input_bindings_contract_test input-bindings.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/input_bindings_contract_test.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/app/input_bindings.cpp")

# Screen-transform contract: pure math on constants, no window or assets.
mmx_public_add_contract(screen_transform_contract_test screen-transform.contract
    "${MMX_PUBLIC_SOURCE_ROOT}/tests/cpp/screen_transform_contract_test.cpp")
