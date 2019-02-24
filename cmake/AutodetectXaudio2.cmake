macro(autodetect_xaudio2)
    include(CheckCXXSourceRuns)
    set(CMAKE_REQUIRED_FLAGS "")

    check_cxx_source_runs("
		#include <windows.h>

		#undef NTDDI_VERSION
		#undef _WIN32_WINNT

		#define NTDDI_VERSION    NTDDI_WIN8
		#define _WIN32_WINNT	 _WIN32_WINNT_WIN8

		#include <xaudio2.h>
        int main() { return 0; }"
        XAUDIO2_FOUND)
endmacro()
