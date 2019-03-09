cmake_minimum_required(VERSION 3.5)

add_subdirectory(core)

# Source Files
add_files(
    network.cpp
    network_admin.cpp
    network_client.cpp
    network_command.cpp
    network_content.cpp
    network_gamelist.cpp
    network_server.cpp
    network_udp.cpp
)

# Header Files
add_files(
    network.h
    network_admin.h
    network_base.h
    network_client.h
    network_content.h
    network_content_gui.h
    network_func.h
    network_gamelist.h
    network_gui.h
    network_internal.h
    network_server.h
    network_type.h
    network_udp.h
)

# GUI Source Code
add_files(
    network_chat_gui.cpp
    network_content_gui.cpp
    network_gui.cpp
)
