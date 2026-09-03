cmake_minimum_required(VERSION 3.18)

project(
    igh_ethercat_diagnostics
    VERSION 1.1.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)

set(INFRASTRUCTURE_SOURCES
    src/infrastructure/command_runner.cpp
)

set(MONITORING_SOURCES
    src/monitoring/master_parser.cpp
    src/monitoring/slave_parser.cpp
    src/monitoring/ethercat_reader.cpp
    src/monitoring/monitor.cpp
)

set(DETECTION_SOURCES
    src/detection/event_detector.cpp
    src/detection/boundary_locator.cpp
    src/detection/recovery_tracker.cpp
)

set(RECORDING_SOURCES
    src/recording/blackbox.cpp
    src/recording/blackbox_filename.cpp
)

set(ESC_SOURCES
    src/esc/esc_register_reader.cpp
    src/esc/esc_diagnostic_reader.cpp
    src/esc/al_status_decoder.cpp
    src/esc/dl_status_decoder.cpp
    src/esc/al_status_code_decoder.cpp
    src/esc/diagnostic_formatter.cpp
)

set(DIAGNOSIS_SOURCES
    src/diagnosis/active_diagnosis.cpp
    src/diagnosis/diagnosis_coordinator.cpp
    src/diagnosis/diagnosis_result_formatter.cpp
)

set(CLI_SOURCES
    src/cli/probe_options.cpp
)

add_library(
    ethercat_diag_core
    STATIC
    ${INFRASTRUCTURE_SOURCES}
    ${MONITORING_SOURCES}
    ${DETECTION_SOURCES}
    ${RECORDING_SOURCES}
    ${ESC_SOURCES}
    ${DIAGNOSIS_SOURCES}
    ${CLI_SOURCES}
)

target_include_directories(
    ethercat_diag_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    ethercat_diag_core
    PUBLIC
    cxx_std_17
)

target_compile_options(
    ethercat_diag_core
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(
    igh_ethercat_diagnostics
    apps/igh_ethercat_diagnostics.cpp
)

target_link_libraries(
    igh_ethercat_diagnostics
    PRIVATE
    ethercat_diag_core
)

target_compile_options(
    igh_ethercat_diagnostics
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(
    igh_ethercat_diagnostics
    PROPERTIES OUTPUT_NAME "igh-ethercat-diagnostics"
)

add_executable(
    esc_diagnostic_probe
    apps/esc_diagnostic_probe.cpp
)

target_link_libraries(
    esc_diagnostic_probe
    PRIVATE
    ethercat_diag_core
)

target_compile_options(
    esc_diagnostic_probe
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(
    esc_diagnostic_probe
    PROPERTIES OUTPUT_NAME "esc-diagnostic-probe"
)

install(
    TARGETS
        igh_ethercat_diagnostics
        esc_diagnostic_probe
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

enable_testing()

function(add_diag_test test_name source_file)
    set(target_name "test_${test_name}")

    add_executable(
        ${target_name}
        ${source_file}
    )

    target_link_libraries(
        ${target_name}
        PRIVATE
        ethercat_diag_core
    )

    target_compile_options(
        ${target_name}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-UNDEBUG>
    )

    add_test(
        NAME ${test_name}
        COMMAND ${target_name}
    )
endfunction()

add_diag_test(master_parser tests/monitoring/test_master_parser.cpp)
add_diag_test(slave_parser tests/monitoring/test_slave_parser.cpp)
add_diag_test(ethercat_reader tests/monitoring/test_ethercat_reader.cpp)
add_diag_test(monitor tests/monitoring/test_monitor.cpp)

add_diag_test(event_detector tests/detection/test_event_detector.cpp)
add_diag_test(boundary_locator tests/detection/test_boundary_locator.cpp)       
add_diag_test(recovery_tracker tests/detection/test_recovery_tracker.cpp)

add_diag_test(blackbox tests/recording/test_blackbox.cpp)
add_diag_test(blackbox_filename tests/recording/test_blackbox_filename.cpp)

add_diag_test(esc_register_reader tests/esc/test_esc_register_reader.cpp)
add_diag_test(esc_diagnostic_reader tests/esc/test_esc_diagnostic_reader.cpp)
add_diag_test(al_status_decoder tests/esc/test_al_status_decoder.cpp)
add_diag_test(dl_status_decoder tests/esc/test_dl_status_decoder.cpp)
add_diag_test(al_status_code_decoder tests/esc/test_al_status_code_decoder.cpp)
add_diag_test(diagnostic_formatter tests/esc/test_diagnostic_formatter.cpp)

add_diag_test(active_diagnosis tests/diagnosis/test_active_diagnosis.cpp)
add_diag_test(diagnosis_coordinator tests/diagnosis/test_diagnosis_coordinator.cpp)
add_diag_test(diagnosis_result_formatter tests/diagnosis/test_diagnosis_result_formatter.cpp)
add_diag_test(probe_options tests/cli/test_probe_options.cpp)
add_diag_test(header_compatibility tests/integration/test_header_compatibility.cpp)
add_diag_test(assertions_enabled tests/integration/test_assertions_enabled.cpp)

add_test(
    NAME esc_diagnostic_probe_rejects_missing_arguments
    COMMAND esc_diagnostic_probe
)

set_tests_properties(
    esc_diagnostic_probe_rejects_missing_arguments
    PROPERTIES WILL_FAIL TRUE
)
