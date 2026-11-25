set(ARISA_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ARISA_ROMFS_DIR "${CMAKE_BINARY_DIR}/romfs")

file(COPY "${ARISA_ASSETS_DIR}/" DESTINATION "${CMAKE_BINARY_DIR}/assets")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/assets/"
    DESTINATION "${ARISA_ROMFS_DIR}"
    PATTERN "fonts" EXCLUDE)
file(COPY "${ARISA_ASSETS_DIR}/" DESTINATION "${ARISA_ROMFS_DIR}")

set(arisa_script_fonts
    NotoSansArabic-OFL.txt
    NotoSansArabic-Subset.ttf
    NotoSansDevanagari-OFL.txt
    NotoSansDevanagari-Subset.ttf
    NotoSansHebrew-OFL.txt
    NotoSansHebrew-Subset.ttf
    NotoSansThai-OFL.txt
    NotoSansThai-Subset.ttf)
foreach(font IN LISTS arisa_script_fonts)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/assets/arisa/fonts/${font}"
        DESTINATION "${ARISA_ROMFS_DIR}/arisa/fonts")
endforeach()
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/assets/arisa/fonts/"
    DESTINATION "${CMAKE_BINARY_DIR}/assets/arisa/fonts")

file(GLOB arisa_swiftui_reference
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/*.png")
file(COPY ${arisa_swiftui_reference}
    DESTINATION "${CMAKE_BINARY_DIR}/assets/arisa/swiftui_reference")
file(COPY ${arisa_swiftui_reference}
    DESTINATION "${ARISA_ROMFS_DIR}/arisa/swiftui_reference")

set(ARISA_LETTERING_FONT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bumbbled/Bumbbled.otf")
if(EXISTS "${ARISA_LETTERING_FONT}")
    file(COPY "${ARISA_LETTERING_FONT}" DESTINATION "${CMAKE_BINARY_DIR}/assets/arisa/fonts")
    file(COPY "${ARISA_LETTERING_FONT}" DESTINATION "${ARISA_ROMFS_DIR}/arisa/fonts")
endif()
