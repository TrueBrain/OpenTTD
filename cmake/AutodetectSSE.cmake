macro(autodetect_sse)
    include(CheckCXXSourceRuns)
    set(CMAKE_REQUIRED_FLAGS "")
    if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_REQUIRED_FLAGS "-msse4.1")
    endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)

    check_cxx_source_runs("
        #include <xmmintrin.h>
        #include <smmintrin.h>
        #include <tmmintrin.h>
        int main() { return 0; }"
        SSE_FOUND)
endmacro()
