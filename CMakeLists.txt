cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME "Windicator")
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(_UNICODE UNICODE)

# Optimize release builds for size: /O1 /Os favor small code, /GL + /LTCG
# enable whole-program optimization, /Gy + /OPT:REF,ICF strip and fold
# unreferenced/identical functions, /GR- drops RTTI (not used).
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Os /GL /Gy /GR- /EHsc /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO")
endif()

# Uncomment the following line to add timeout to wait so that the
# registry handle can be released.  Not really needed and causes
# a very tiny bit of extra CPU to keep checking for exit condition.
#
#add_compile_definitions(_TIDY_TIMEOUT)

add_link_options(
    /SUBSYSTEM:WINDOWS
    /DYNAMICBASE
    /MACHINE:X64
)

add_executable(
        ${PROJECT_NAME}
        resources/resource.h
        resources/WinApp.rc
        resources/app.manifest
        include/messages.h
        include/aboutdialog.h
        src/aboutdialog.cpp
        include/MainWindow.h
        src/MainWindow.cpp
        include/notificationIcon.h
        src/notificationIcon.cpp
        include/desktopWatcher.h
        src/desktopWatcher.cpp
        src/winMain.cpp
)

target_link_libraries(${PROJECT_NAME} Version.lib Comctl32.lib)

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
