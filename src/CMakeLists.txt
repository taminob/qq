add_library(qq_lib STATIC command_matcher.cpp)
target_link_libraries(qq_lib PUBLIC ppplugin::ppplugin Boost::program_options)
target_compile_options(
  qq_lib PRIVATE $<$<BOOL:true>:SHELL:${QQ_WARNING_OPTIONS}>
                 $<$<BOOL:true>:SHELL:${QQ_SANITIZE_OPTIONS}>)

add_executable(qq main.cpp)
target_link_libraries(qq PRIVATE qq_lib)
target_compile_options(qq PRIVATE $<$<BOOL:true>:SHELL:${QQ_WARNING_OPTIONS}>
                                  $<$<BOOL:true>:SHELL:${QQ_SANITIZE_OPTIONS}>)
