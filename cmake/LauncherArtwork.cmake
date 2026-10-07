# Keep downloaded artwork outside the source tree. Reuse the verified copy on
# later configurations; an offline build can supply the same image explicitly.
set(V82_LAUNCHER_BOXART "" CACHE FILEPATH "Existing V82 launcher cover image (optional)")
if(V82_LAUNCHER_BOXART)
    if(NOT EXISTS "${V82_LAUNCHER_BOXART}")
        message(FATAL_ERROR "V82_LAUNCHER_BOXART does not exist: ${V82_LAUNCHER_BOXART}")
    endif()
else()
    set(V82_LAUNCHER_BOXART "${CMAKE_CURRENT_BINARY_DIR}/launcher_assets/v82-boxart.png")
    set(_v82_boxart_sha256 "6a572b8bd14eb658adf6dc108858bcd4bcb17ad67933e32070c339350642de76")
    set(_v82_boxart_actual "")
    if(EXISTS "${V82_LAUNCHER_BOXART}")
        file(SHA256 "${V82_LAUNCHER_BOXART}" _v82_boxart_actual)
    endif()
    if(NOT _v82_boxart_actual STREQUAL _v82_boxart_sha256)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/launcher_assets")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/libretro-thumbnails/Sony_-_PlayStation/25c6d021383b6dd4f7c74bad24845b15f6fb7d04/Named_Boxarts/Vigilante%208%20-%202nd%20Offense%20(USA).png"
            "${V82_LAUNCHER_BOXART}"
            EXPECTED_HASH "SHA256=${_v82_boxart_sha256}"
            TLS_VERIFY ON TIMEOUT 60 STATUS _v82_boxart_status)
        list(GET _v82_boxart_status 0 _v82_boxart_result)
        if(NOT _v82_boxart_result EQUAL 0)
            message(FATAL_ERROR "Could not fetch V82 cover art: ${_v82_boxart_status}. Set V82_LAUNCHER_BOXART to an existing image for an offline build.")
        endif()
    endif()
endif()
