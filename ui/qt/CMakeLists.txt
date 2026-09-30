cmake_minimum_required(VERSION 3.25)
project(atena_ui VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ATENA_UI_BUILD_TESTS "Build Qt UI tests" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Svg)

add_library(atena-ui-lib STATIC
    src/Application.h
    src/AssetCatalog.h
    src/MainWindow.h
    src/client/AtenaClientFacade.h
    src/client/RealAtenaClient.h
    src/dialogs/OnboardingDialog.h
    src/dialogs/ProviderSetupDialog.h
    src/dialogs/ToolConfirmationDialog.h
    src/pages/ChatPage.h
    src/pages/DiagnosticsPage.h
    src/pages/HomePage.h
    src/pages/KnowledgePage.h
    src/pages/ModelsPage.h
    src/pages/PrivacyPage.h
    src/pages/ProvidersPage.h
    src/pages/SettingsPage.h
    src/pages/ToolsPage.h
    src/theme/ThemeManager.h
    src/theme/ThemePalette.h
    src/theme/UiMetrics.h
    src/theme/UiTypography.h
    src/widgets/ChatComposer.h
    src/widgets/MessageBubble.h
    src/widgets/NavButton.h
    src/widgets/StatusChip.h
    src/Application.cpp
    src/AssetCatalog.cpp
    src/MainWindow.cpp
    src/client/RealAtenaClient.cpp
    src/theme/ThemeManager.cpp
    src/widgets/NavButton.cpp
    src/widgets/StatusChip.cpp
    src/widgets/MessageBubble.cpp
    src/widgets/ChatComposer.cpp
    src/pages/HomePage.cpp
    src/pages/ChatPage.cpp
    src/pages/ModelsPage.cpp
    src/pages/ProvidersPage.cpp
    src/pages/KnowledgePage.cpp
    src/pages/ToolsPage.cpp
    src/pages/PrivacyPage.cpp
    src/pages/DiagnosticsPage.cpp
    src/pages/SettingsPage.cpp
    src/dialogs/OnboardingDialog.cpp
    src/dialogs/ProviderSetupDialog.cpp
    src/dialogs/ToolConfirmationDialog.cpp
    resources/atena.qrc
)

target_include_directories(atena-ui-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(atena-ui-lib PUBLIC Qt6::Widgets Qt6::Svg atena_client)
target_compile_definitions(atena-ui-lib PRIVATE QT_NO_KEYWORDS)

if(MSVC)
    target_compile_options(atena-ui-lib PRIVATE /W4 /permissive-)
else()
    target_compile_options(atena-ui-lib PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(WIN32)
    configure_file(resources/windows/atena.rc.in ${CMAKE_CURRENT_BINARY_DIR}/atena.rc @ONLY)
    add_executable(atena-ui WIN32 src/main.cpp ${CMAKE_CURRENT_BINARY_DIR}/atena.rc)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/atena.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/atena.ico")
else()
    add_executable(atena-ui src/main.cpp)
endif()
target_link_libraries(atena-ui PRIVATE atena-ui-lib)

# Development builds must carry the Core beside the Qt executable because
# the SDK starts a sibling Core before falling back to the installed libexec path.
add_dependencies(atena-ui atena-core)
add_custom_command(TARGET atena-ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:atena-core>
            $<TARGET_FILE_DIR:atena-ui>/$<TARGET_FILE_NAME:atena-core>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/identity
            $<TARGET_FILE_DIR:atena-ui>/identity
    COMMENT "Staging Atena Core and identity beside the Qt UI"
)

if(ATENA_UI_BUILD_TESTS)
    include(CTest)
    enable_testing()
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tests/ui ${CMAKE_CURRENT_BINARY_DIR}/tests-ui)
endif()
