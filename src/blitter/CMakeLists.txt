cmake_minimum_required(VERSION 3.5)

# Blitters
if (NOT OPTION_DEDICATED)
    add_files(
        32bpp_anim.cpp
        32bpp_anim.hpp
    )
    if (SSE_FOUND)
        add_files(
            32bpp_anim_sse2.cpp
            32bpp_anim_sse2.hpp
            32bpp_anim_sse4.cpp
            32bpp_anim_sse4.hpp
        )
    endif(SSE_FOUND)
    add_files(
        32bpp_base.cpp
        32bpp_base.hpp
        32bpp_optimized.cpp
        32bpp_optimized.hpp
        32bpp_simple.cpp
        32bpp_simple.hpp
    )
    if (SSE_FOUND)
        add_files(
            32bpp_sse_func.hpp
            32bpp_sse_type.h
            32bpp_sse2.cpp
            32bpp_sse2.hpp
            32bpp_sse4.cpp
            32bpp_sse4.hpp
            32bpp_ssse3.cpp
            32bpp_ssse3.hpp
        )
    endif (SSE_FOUND)
    add_files(
        8bpp_base.cpp
        8bpp_base.hpp
        8bpp_optimized.cpp
        8bpp_optimized.hpp
        8bpp_simple.cpp
        8bpp_simple.hpp
    )
endif (NOT OPTION_DEDICATED)
add_files(
    base.hpp
    common.hpp
    factory.hpp
    null.cpp
    null.hpp
)
