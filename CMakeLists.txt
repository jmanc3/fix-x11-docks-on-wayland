cmake_minimum_required(VERSION 3.31)
set(project_name fix_x11_docks)

project(${project_name})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF) ## on g++ this ensures: -std=c++11 and not -std=gnu++11


find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT})


include_directories(wayland_protocol)
file(GLOB PROTOCOL wayland_protocol/*.c wayland_protocol/*.h)


add_executable(${project_name} ${PROTOCOL} x_proxy_windows.h x_proxy_windows.cpp main.cpp)


find_package(PkgConfig)

if (NOT PkgConfig_FOUND)
    message(FATAL_ERROR "You need to have pkg-config installed. On voidlinux, sudo xbps-install pkg-config")
endif ()


function(try_to_add_dependency lib_name)
    if (${lib_name}_FOUND)
        target_link_libraries(${project_name} PUBLIC ${${lib_name}_LIBRARIES})
        target_include_directories(${project_name} PUBLIC ${${lib_name}_INCLUDE_DIRS})
        target_compile_options(${project_name} PUBLIC ${${lib_name}_CFLAGS_OTHER})
    else ()
        message(FATAL_ERROR "Could not find: ${lib_name}.\
                             Make sure your system has it installed.")
    endif ()
endfunction(try_to_add_dependency)


set(
        LIBS
        wayland-client # to talk with wayland
        xcb # to open our fake window
        x11
        xfixes
)


foreach (LIB IN LISTS LIBS)
    pkg_check_modules(D_${LIB} ${LIB})
    try_to_add_dependency(D_${LIB} ${LIB})
endforeach ()

# install ${project_name} executable to /usr/bin/${project_name}
#
install(TARGETS ${project_name}
        DESTINATION /usr/bin/)
