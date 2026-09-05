cmake_minimum_required(VERSION 3.18)

project(
    igh_ethercat_diagnostics
    VERSION 3.0.0
    LANGUAGES CXX
)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(CMAKE_CXX_EXTENSIONS OFF)

set(INFRASTRUCTURE_SOURCES
    src/infrastructure/igh_master_device.cpp
    src/infrastructure/runtime_output.cpp
)

set(MONITORING_SOURCES
    src/monitoring/ioctl_snapshot_reader.cpp
    src/monitoring/ioctl_monitor_adapter.cpp
    src/monitoring/snapshot_comparator.cpp
    src/monitoring/monitor.cpp
    src/monitoring/port_error_monitor.cpp
)

set(DETECTION_SOURCES
    src/detection/event_detector.cpp
    src/detection/boundary_locator.cpp
    src/detection/recovery_tracker.cpp
    src/detection/port_error_tracker.cpp
)

set(RECORDING_SOURCES
    src/recording/blackbox.cpp
    src/recording/blackbox_filename.cpp
)

set(ESC_SOURCES
    src/esc/ioctl_esc_register_reader.cpp
    src/esc/esc_diagnostic_reader.cpp
    src/esc/al_status_decoder.cpp
    src/esc/dl_status_decoder.cpp
    src/esc/al_status_code_decoder.cpp
    src/esc/diagnostic_formatter.cpp
    src/esc/port_error_decoder.cpp
    src/esc/port_error_formatter.cpp
    src/esc/port_error_reader.cpp
)

set(DIAGNOSIS_SOURCES
    src/diagnosis/active_diagnosis.cpp
    src/diagnosis/diagnosis_coordinator.cpp
    src/diagnosis/diagnosis_result_formatter.cpp
)

set(ROOT_CAUSE_SOURCES
    src/root_cause/evidence_builder.cpp
    src/root_cause/evidence_window.cpp
    src/root_cause/root_cause_analyzer.cpp
    src/root_cause/root_cause_formatter.cpp
    src/root_cause/root_cause_calibration.cpp
)

set(CLI_SOURCES
    src/cli/probe_options.cpp
)

set(PUBLISHING_SOURCES
    src/publishing/web_data_publisher.cpp
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
    ${ROOT_CAUSE_SOURCES}
    ${CLI_SOURCES}
    ${PUBLISHING_SOURCES}
)

add_library(
    ethercat_diag_shell_backend
    STATIC
    src/infrastructure/command_runner.cpp
    src/monitoring/master_parser.cpp
    src/monitoring/slave_parser.cpp
    src/monitoring/ethercat_reader.cpp
    src/esc/esc_register_reader.cpp
    src/esc/shell_esc_diagnostic_reader.cpp
)

target_include_directories(
    ethercat_diag_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
    ethercat_diag_shell_backend
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    ethercat_diag_shell_backend
    PUBLIC
    cxx_std_17
)

target_compile_options(
    ethercat_diag_shell_backend
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_include_directories(
    ethercat_diag_core
    SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/igh-ethercat-1.6.3
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

add_executable(
    snapshot_backend_compare
    apps/snapshot_backend_compare.cpp
)

add_executable(
    root_cause_matrix_demo
    apps/root_cause_matrix_demo.cpp
)

target_link_libraries(
    root_cause_matrix_demo
    PRIVATE
    ethercat_diag_core
)

target_compile_options(
    root_cause_matrix_demo
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(
    snapshot_backend_compare
    PRIVATE
    ethercat_diag_core
    ethercat_diag_shell_backend
)

target_compile_options(
    snapshot_backend_compare
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_library(
    ethercat_diag_web_core
    STATIC
    src/web/web_options.cpp
    src/web/web_data_store.cpp
    src/web/http_router.cpp
    src/web/http_server.cpp
)

target_include_directories(
    ethercat_diag_web_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ethercat_diag_web_core PUBLIC cxx_std_17)
target_compile_options(
    ethercat_diag_web_core
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(ethercat_diag_web apps/ethercat_diag_web.cpp)
target_link_libraries(
    ethercat_diag_web
    PRIVATE
    ethercat_diag_web_core
)
target_compile_options(
    ethercat_diag_web
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(
    ethercat_diag_web
    PROPERTIES OUTPUT_NAME "igh-ethercat-diagnostics-web"
)

install(
    TARGETS
        igh_ethercat_diagnostics
        esc_diagnostic_probe
        ethercat_diag_web
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES
        packaging/systemd/igh-ethercat-diagnostics.service
        packaging/systemd/igh-ethercat-diagnostics-web.service
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/systemd/system
)

install(
    DIRECTORY
        web/
    DESTINATION
        ${CMAKE_INSTALL_DATADIR}/igh-ethercat-diagnostics/web
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
        -UNDEBUG
    )

    add_test(
        NAME ${test_name}
        COMMAND ${target_name}
    )
endfunction()

add_diag_test(master_parser tests/monitoring/test_master_parser.cpp)
add_diag_test(slave_parser tests/monitoring/test_slave_parser.cpp)
add_diag_test(ethercat_reader tests/monitoring/test_ethercat_reader.cpp)
add_diag_test(ioctl_snapshot_reader tests/monitoring/test_ioctl_snapshot_reader.cpp)
add_diag_test(snapshot_comparator tests/monitoring/test_snapshot_comparator.cpp)
add_diag_test(monitor tests/monitoring/test_monitor.cpp)
add_diag_test(port_error_monitor tests/monitoring/test_port_error_monitor.cpp)

add_diag_test(event_detector tests/detection/test_event_detector.cpp)
add_diag_test(boundary_locator tests/detection/test_boundary_locator.cpp)       
add_diag_test(recovery_tracker tests/detection/test_recovery_tracker.cpp)
add_diag_test(port_error_tracker tests/detection/test_port_error_tracker.cpp)

add_diag_test(blackbox tests/recording/test_blackbox.cpp)
add_diag_test(blackbox_filename tests/recording/test_blackbox_filename.cpp)

add_diag_test(esc_register_reader tests/esc/test_esc_register_reader.cpp)
add_diag_test(ioctl_esc_register_reader tests/esc/test_ioctl_esc_register_reader.cpp)
add_diag_test(port_error_decoder tests/esc/test_port_error_decoder.cpp)
add_diag_test(port_error_formatter tests/esc/test_port_error_formatter.cpp)
add_diag_test(port_error_reader tests/esc/test_port_error_reader.cpp)
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
add_diag_test(ioctl_monitor_adapter tests/integration/test_ioctl_monitor_adapter.cpp)
add_diag_test(igh_ioctl_abi tests/integration/test_igh_ioctl_abi.cpp)
add_diag_test(igh_master_device tests/infrastructure/test_igh_master_device.cpp)
add_diag_test(runtime_output tests/infrastructure/test_runtime_output.cpp)
add_diag_test(evidence_builder tests/root_cause/test_evidence_builder.cpp)
add_diag_test(evidence_window tests/root_cause/test_evidence_window.cpp)
add_diag_test(root_cause_analyzer tests/root_cause/test_root_cause_analyzer.cpp)
add_diag_test(root_cause_formatter tests/root_cause/test_root_cause_formatter.cpp)
add_diag_test(root_cause_calibration tests/root_cause/test_root_cause_calibration.cpp)
add_diag_test(web_data_publisher tests/publishing/test_web_data_publisher.cpp)

# Legacy backend tests remain available during migration, but shell code is
# deliberately absent from both production executables.
target_link_libraries(test_master_parser PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_slave_parser PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_ethercat_reader PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_esc_register_reader PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_esc_diagnostic_reader PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_active_diagnosis PRIVATE ethercat_diag_shell_backend)
target_link_libraries(test_diagnosis_coordinator PRIVATE ethercat_diag_shell_backend)

add_test(
    NAME esc_diagnostic_probe_rejects_missing_arguments
    COMMAND esc_diagnostic_probe
)

set_tests_properties(
    esc_diagnostic_probe_rejects_missing_arguments
    PROPERTIES WILL_FAIL TRUE
)

add_test(
    NAME systemd_unit
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/deployment/test_systemd_unit.sh
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME install_layout
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/deployment/test_install_layout.sh
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(
    NAME web_systemd_unit
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/deployment/test_web_systemd_unit.sh
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(test_web_options tests/web/test_web_options.cpp)
target_link_libraries(test_web_options PRIVATE ethercat_diag_web_core)
target_compile_options(
    test_web_options
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
)
add_test(NAME web_options COMMAND test_web_options)

add_executable(test_web_data_store tests/web/test_web_data_store.cpp)
target_link_libraries(test_web_data_store PRIVATE ethercat_diag_web_core)
target_compile_options(
    test_web_data_store
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
)
add_test(NAME web_data_store COMMAND test_web_data_store)

add_executable(test_http_router tests/web/test_http_router.cpp)
target_link_libraries(test_http_router PRIVATE ethercat_diag_web_core)
target_compile_options(
    test_http_router
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
)
add_test(NAME http_router COMMAND test_http_router)

add_executable(test_http_server tests/web/test_http_server.cpp)
target_link_libraries(
    test_http_server
    PRIVATE
    ethercat_diag_web_core
    Threads::Threads
)
target_compile_options(
    test_http_server
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
)
add_test(NAME http_server COMMAND test_http_server)

add_executable(test_web_assets tests/web/test_web_assets.cpp)
target_link_libraries(test_web_assets PRIVATE ethercat_diag_web_core)
target_compile_definitions(
    test_web_assets
    PRIVATE
    TEST_WEB_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web"
)
target_compile_options(
    test_web_assets
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -UNDEBUG
)
add_test(NAME web_assets COMMAND test_web_assets)
