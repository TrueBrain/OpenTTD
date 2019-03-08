cmake_minimum_required(VERSION 3.5)

# Source Files
add_files(
    demands.cpp
    flowmapper.cpp
    linkgraph.cpp
    linkgraphjob.cpp
    linkgraphschedule.cpp
    mcf.cpp
    refresh.cpp
)

# Header Files
add_files(
    demands.h
    flowmapper.h
    init.h
    linkgraph.h
    linkgraph_base.h
    linkgraph_gui.h
    linkgraph_type.h
    linkgraphjob.h
    linkgraphjob_base.h
    linkgraphschedule.h
    mcf.h
    refresh.h
)

# GUI Source Code
add_files(linkgraph_gui.cpp)
