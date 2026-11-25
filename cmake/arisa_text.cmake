set(arisa_freetype_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype")
set(arisa_harfbuzz_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz")
set(arisa_sheenbidi_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sheenbidi")
set(arisa_unibreak_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libunibreak")

function(arisa_configure_text_vendor target)
    arisa_disable_exceptions_and_rtti(${target})
    if(NINTENDO_SWITCH)
        target_compile_options(${target} PRIVATE -Os)
    endif()
endfunction()

add_library(arisa_freetype_vendor STATIC
    "${arisa_freetype_dir}/src/base/ftbase.c"
    "${arisa_freetype_dir}/src/base/ftbitmap.c"
    "${arisa_freetype_dir}/src/base/ftdebug.c"
    "${arisa_freetype_dir}/src/base/ftinit.c"
    "${arisa_freetype_dir}/src/base/ftmm.c"
    "${arisa_freetype_dir}/src/base/ftsystem.c"
    "${arisa_freetype_dir}/src/cff/cff.c"
    "${arisa_freetype_dir}/src/psaux/psaux.c"
    "${arisa_freetype_dir}/src/psnames/psnames.c"
    "${arisa_freetype_dir}/src/sfnt/sfnt.c"
    "${arisa_freetype_dir}/src/smooth/smooth.c"
    "${arisa_freetype_dir}/src/truetype/truetype.c")
target_include_directories(arisa_freetype_vendor SYSTEM PUBLIC
    "${arisa_freetype_dir}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype_config")
target_compile_definitions(arisa_freetype_vendor
    PUBLIC
        "FT_CONFIG_MODULES_H=<arisa_ftmodule.h>"
        "FT_CONFIG_OPTIONS_H=<arisa_ftoption.h>"
    PRIVATE FT2_BUILD_LIBRARY)
set_target_properties(arisa_freetype_vendor PROPERTIES C_STANDARD 99)
arisa_configure_text_vendor(arisa_freetype_vendor)

add_library(arisa_harfbuzz_vendor STATIC "${arisa_harfbuzz_dir}/src/harfbuzz.cc")
target_include_directories(arisa_harfbuzz_vendor SYSTEM PUBLIC "${arisa_harfbuzz_dir}/src")
target_include_directories(arisa_harfbuzz_vendor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz_config")
target_compile_definitions(arisa_harfbuzz_vendor PRIVATE
    "HB_CONFIG_OVERRIDE_H=\"arisa_hb_config.h\"")
target_compile_features(arisa_harfbuzz_vendor PRIVATE cxx_std_23)
arisa_configure_text_vendor(arisa_harfbuzz_vendor)

add_library(arisa_sheenbidi_vendor STATIC "${arisa_sheenbidi_dir}/Source/SheenBidi.c")
target_include_directories(arisa_sheenbidi_vendor SYSTEM PUBLIC "${arisa_sheenbidi_dir}/Headers")
target_include_directories(arisa_sheenbidi_vendor PRIVATE "${arisa_sheenbidi_dir}/Source")
target_compile_definitions(arisa_sheenbidi_vendor PRIVATE
    SB_CONFIG_UNITY
    SB_CONFIG_DISABLE_SCRATCH_MEMORY)
set_target_properties(arisa_sheenbidi_vendor PROPERTIES C_STANDARD 11)
arisa_configure_text_vendor(arisa_sheenbidi_vendor)

add_library(arisa_unibreak_vendor STATIC
    "${arisa_unibreak_dir}/src/eastasianwidthdef.c"
    "${arisa_unibreak_dir}/src/emojidef.c"
    "${arisa_unibreak_dir}/src/graphemebreak.c"
    "${arisa_unibreak_dir}/src/linebreak.c"
    "${arisa_unibreak_dir}/src/linebreakdata.c"
    "${arisa_unibreak_dir}/src/linebreakdef.c"
    "${arisa_unibreak_dir}/src/unibreakbase.c"
    "${arisa_unibreak_dir}/src/unibreakdef.c")
target_include_directories(arisa_unibreak_vendor SYSTEM PUBLIC "${arisa_unibreak_dir}/src")
set_target_properties(arisa_unibreak_vendor PROPERTIES C_STANDARD 99)
arisa_configure_text_vendor(arisa_unibreak_vendor)

add_library(arisa_text STATIC
    src/text/atlas_page.cpp
    src/text/bidi_text.cpp
    src/text/case_mapping.cpp
    src/text/cache_key.cpp
    src/text/font_collection.cpp
    src/text/font_face.cpp
    src/text/glyph_atlas.cpp
    src/text/glyph_bitmap.cpp
    src/text/layout_width.cpp
    src/text/date_format_style.cpp
    src/text/locale.cpp
    src/text/locale_data.cpp
    src/text/locale_tables.cpp
    src/text/format_tables.cpp
    src/text/format_specifier.cpp
    src/text/byte_count_format_style.cpp
    src/text/measurement.cpp
    src/text/number_format_style.cpp
    src/text/plural_rules.cpp
    src/text/point_size.cpp
    src/text/shaped_text.cpp
    src/text/shaper.cpp
    src/text/software_renderer.cpp
    src/text/text_breaks.cpp
    src/text/text_error.cpp
    src/text/text_layout.cpp
    src/text/text_runs.cpp
    src/text/typesetter.cpp
    src/text/typesetter_cache.cpp
    src/text/utf8.cpp
    src/bridge/toml/string_catalog.cpp)
target_include_directories(arisa_text PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_text
    PUBLIC arisa_support arisa_provider arisa_graphics
    PRIVATE
        arisa_toml_vendor
        arisa_freetype_vendor
        arisa_harfbuzz_vendor
        arisa_sheenbidi_vendor
        arisa_unibreak_vendor)
arisa_enable_strict_warnings(arisa_text)
