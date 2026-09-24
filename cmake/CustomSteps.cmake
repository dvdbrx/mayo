# Custom steps for CadView fork
# Configures product branding, linker paths, and packaging

set(CADVIEW_PRODUCT_NAME "CadView" CACHE STRING "Product name")
set(CADVIEW_COMPANY_NAME "CadView Team" CACHE STRING "Company name")
set(CADVIEW_COMPANY_DOMAIN "cadview.local" CACHE STRING "Company domain")
set(CADVIEW_APP_DESCRIPTION "CadView - Lightweight 3D CAD viewer and measurement tool" CACHE STRING "Application description")

# Generate branding header
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/branding.h.in
    ${CMAKE_BINARY_DIR}/common/branding.h
    @ONLY
)

# Generate qt.conf in build directory for automatic Qt plugin resolution
if(EXISTS "${PROJECT_SOURCE_DIR}/.deps/sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins")
    file(WRITE "${CMAKE_BINARY_DIR}/qt.conf"
        "[Paths]\nPrefix = ${PROJECT_SOURCE_DIR}/.deps/sysroot/usr/lib/x86_64-linux-gnu/qt6\nPlugins = plugins\n"
    )
endif()


# Link directories for all targets
set(_ALL_CADVIEW_TARGETS
    mayo
    MayoCore
    MayoIO
    test-base
    test-graphics
    test-io
    test-measure
    test-app
)

set(_CADVIEW_LIB_DIRS
    "${PROJECT_SOURCE_DIR}/.deps/sysroot/usr/lib/x86_64-linux-gnu"
    "${PROJECT_SOURCE_DIR}/.deps/sysroot/usr/lib"
)

foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
    list(APPEND _CADVIEW_LIB_DIRS "${prefix}/lib/x86_64-linux-gnu" "${prefix}/lib")
endforeach()

if(DEFINED ENV{CMAKE_PREFIX_PATH})
    set(_env_prefixes "$ENV{CMAKE_PREFIX_PATH}")
    string(REPLACE ":" ";" _env_prefixes "${_env_prefixes}")
    foreach(prefix IN LISTS _env_prefixes)
        list(APPEND _CADVIEW_LIB_DIRS "${prefix}/lib/x86_64-linux-gnu" "${prefix}/lib")
    endforeach()
endif()

foreach(libdir IN LISTS _CADVIEW_LIB_DIRS)
    if(IS_DIRECTORY "${libdir}")
        link_directories("${libdir}")
        foreach(tgt IN LISTS _ALL_CADVIEW_TARGETS)
            if(TARGET ${tgt})
                target_link_directories(${tgt} PRIVATE "${libdir}")
            endif()
        endforeach()
    endif()
endforeach()


if(TARGET mayo)
    if(UNIX AND NOT APPLE)
        target_link_options(mayo PRIVATE "-Wl,--disable-new-dtags")
    endif()

    # Include binary dir so #include <common/branding.h> is found

    target_include_directories(mayo PRIVATE ${CMAKE_BINARY_DIR})
    target_compile_definitions(mayo PRIVATE
        CADVIEW_PRODUCT_NAME="${CADVIEW_PRODUCT_NAME}"
        CADVIEW_COMPANY_NAME="${CADVIEW_COMPANY_NAME}"
        CADVIEW_COMPANY_DOMAIN="${CADVIEW_COMPANY_DOMAIN}"
    )

    # Set executable output name / properties
    set_target_properties(mayo PROPERTIES
        OUTPUT_NAME "cadview"
        MACOSX_BUNDLE_BUNDLE_NAME "${CADVIEW_PRODUCT_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.cadview.cadview"
    )

    # Install rules
    install(TARGETS mayo
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY.md"
        DESTINATION .
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "cadview")
set(CPACK_PACKAGE_VENDOR "${CADVIEW_COMPANY_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${CADVIEW_APP_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)


