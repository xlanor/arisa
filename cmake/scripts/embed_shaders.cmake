cmake_minimum_required(VERSION 3.28)

foreach(required HEADER NAMESPACE CLASS SHADERS OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "embed_shaders.cmake needs -D${required}=")
    endif()
endforeach()

set(functions "")
string(REPLACE "|" ";" entries "${SHADERS}")
foreach(entry IN LISTS entries)
    string(FIND "${entry}" "=" separator)
    if(separator EQUAL -1)
        message(FATAL_ERROR "embed_shaders.cmake expects name=path, got ${entry}")
    endif()
    string(SUBSTRING "${entry}" 0 ${separator} name)
    math(EXPR start "${separator} + 1")
    string(SUBSTRING "${entry}" ${start} -1 path)
    file(READ "${path}" hex HEX)
    string(LENGTH "${hex}" length)
    if(length EQUAL 0)
        message(FATAL_ERROR "${path} is empty")
    endif()
    math(EXPR count "${length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(APPEND functions "
auto ${CLASS}::${name}() noexcept -> std::span<const std::uint8_t> {
  alignas(4) static constexpr std::array<std::uint8_t, ${count}> code{
      ${bytes}};
  return code;
}
")
endforeach()

set(source "@HASH@include <${HEADER}>

@HASH@include <array>
@HASH@include <cstdint>

namespace ${NAMESPACE} {
${functions}
}
")

string(REPLACE "@HASH@" "#" source "${source}")
file(WRITE "${OUTPUT}.tmp" "${source}")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
