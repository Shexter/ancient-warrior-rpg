option(HEROES_WERROR "Treat warnings as errors" OFF)

add_library(heroes_warnings INTERFACE)
if(MSVC)
    target_compile_options(heroes_warnings INTERFACE /W4 /permissive- /utf-8)
    if(HEROES_WERROR)
        target_compile_options(heroes_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(heroes_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wimplicit-fallthrough)
    if(HEROES_WERROR)
        target_compile_options(heroes_warnings INTERFACE -Werror)
    endif()
endif()
