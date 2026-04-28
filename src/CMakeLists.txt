file(GLOB_RECURSE HEADER_FILES ${CMAKE_SOURCE_DIR}/include/**/*.h)

add_library(qq_lib STATIC ${HEADER_FILES} matcher/script_matcher.cpp plugin_type.cpp)
target_link_libraries(qq_lib PUBLIC ppplugin::ppplugin Boost::headers
                                    Boost::program_options Boost::json)
target_include_directories(qq_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(
  qq_lib PRIVATE $<$<BOOL:true>:SHELL:${QQ_WARNING_OPTIONS}>
                 $<$<BOOL:true>:SHELL:${QQ_SANITIZE_OPTIONS}>)

add_executable(qq main.cpp)
target_link_libraries(qq PRIVATE qq_lib)
target_compile_options(qq PRIVATE $<$<BOOL:true>:SHELL:${QQ_WARNING_OPTIONS}>
                                  $<$<BOOL:true>:SHELL:${QQ_SANITIZE_OPTIONS}>)
