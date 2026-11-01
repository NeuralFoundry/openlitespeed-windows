cmake_minimum_required(VERSION 3.16)
project(udns C)
set(SRC udns_dn.c udns_dntosp.c udns_parse.c udns_resolver.c udns_init.c udns_misc.c
    udns_rr_a.c udns_rr_ptr.c udns_rr_mx.c udns_rr_txt.c udns_bl.c
    udns_rr_srv.c udns_rr_naptr.c udns_codes.c udns_jran.c)
list(TRANSFORM SRC PREPEND ${UDNS_SRC}/)
list(APPEND SRC ${CMAKE_CURRENT_SOURCE_DIR}/udns_win_xtox.c)
add_library(udns STATIC ${SRC})
target_include_directories(udns PRIVATE ${UDNS_SRC})
target_compile_definitions(udns PRIVATE WINDOWS _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
install(TARGETS udns ARCHIVE DESTINATION lib)
install(FILES ${UDNS_SRC}/udns.h DESTINATION include)
