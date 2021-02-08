add_library(input_common STATIC
    analog_from_button.cpp
    analog_from_button.h
    keyboard.cpp
    keyboard.h
    main.cpp
    main.h
    motion_from_button.cpp
    motion_from_button.h
    motion_input.cpp
    motion_input.h
    settings.cpp
    settings.h
    touch_from_button.cpp
    touch_from_button.h
    gcadapter/gc_adapter.cpp
    gcadapter/gc_adapter.h
    gcadapter/gc_poller.cpp
    gcadapter/gc_poller.h
    mouse/mouse_input.cpp
    mouse/mouse_input.h
    mouse/mouse_poller.cpp
    mouse/mouse_poller.h
    joycon/jc_adapter.cpp
    joycon/jc_adapter.h
    joycon/jc_poller.cpp
    joycon/jc_poller.h
    sdl/sdl.cpp
    sdl/sdl.h
    udp/client.cpp
    udp/client.h
    udp/protocol.cpp
    udp/protocol.h
    udp/udp.cpp
    udp/udp.h
)

if (MSVC)
    target_compile_options(input_common PRIVATE
        /W4
        /WX

        # 'expression' : signed/unsigned mismatch
        /we4018
        # 'argument' : conversion from 'type1' to 'type2', possible loss of data (floating-point)
        /we4244
        # 'conversion' : conversion from 'type1' to 'type2', signed/unsigned mismatch
        /we4245
        # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /we4254
        # 'var' : conversion from 'size_t' to 'type', possible loss of data
        /we4267
        # 'context' : truncation from 'type1' to 'type2'
        /we4305
    )
else()
    target_compile_options(input_common PRIVATE
        -Werror
        -Werror=conversion
        -Werror=ignored-qualifiers
        -Werror=implicit-fallthrough
        -Werror=reorder
        -Werror=shadow
        -Werror=sign-compare
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-parameter>
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-variable>
        -Werror=unused-variable
    )
endif()

if(SDL2_FOUND)
    target_sources(input_common PRIVATE
        sdl/sdl_impl.cpp
        sdl/sdl_impl.h
    )
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()

target_include_directories(input_common SYSTEM PRIVATE ${LIBUSB_INCLUDE_DIR})
target_link_libraries(input_common PRIVATE ${LIBUSB_LIBRARIES})

target_link_libraries(input_common PUBLIC ${HIDAPI_LIBRARIES})

create_target_directory_groups(input_common)
target_link_libraries(input_common PUBLIC core PRIVATE common Boost::boost)
