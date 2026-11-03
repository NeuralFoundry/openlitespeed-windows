cmake_minimum_required(VERSION 3.20)
project(openlitespeed VERSION 1.9.4 LANGUAGES C CXX)

get_filename_component(OLS_TOP "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(OLS_SRC "${OLS_TOP}/src")
set(DEPS "${OLS_TOP}/.deps/prefix")
set(COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/compat")
set(LSQUIC_TOP "${OLS_TOP}/lsquic")

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MOD_PAGESPEED OFF)
set(MOD_SECURITY OFF)
set(MOD_LUA OFF)
set(MOD_REQPARSER OFF)
set(MOD_INSPECTOR OFF)
set(MOD_UPLOADPROGRESS OFF)
set(MOD_LSPHP OFF)
set(COMPILE_IO_URING OFF)
set(COMPILE_LINUX_AIO OFF)
set(LSQUIC_SERVER_MODE 1)

add_compile_options(/utf-8 /W3 /wd4996 /wd4244 /wd4267 /wd4018 /wd4146 /wd4200 /wd4201 /wd4204 /wd4214
                    /wd4100 /wd4127 /wd4305 /wd4334 /wd4133 /wd4047 /wd4024 /wd4090 /wd4057
                    /we4311 /we4312 /we4302 /Zc:preprocessor /Zc:__cplusplus /bigobj /Zi)
add_compile_options($<$<COMPILE_LANGUAGE:C>:/std:clatest>)
add_link_options(/DEBUG)

add_compile_definitions(
    _CRT_DECLARE_NONSTDC_NAMES=0 _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _CRT_RAND_S
    _WINSOCK_DEPRECATED_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
    PCRE2_STATIC XML_STATIC HAVE_BORINGSSL OPENSSL_NO_CXX DISABLE_OBJ_POOL
    USE_UDNS USE_BROTLI LS_WINDOWS ENABLE_IPTOGEO2 USE_IP2LOCATION
)

configure_file("${OLS_SRC}/config.h.cmake" "${PROJECT_BINARY_DIR}/src/config.h")

include_directories(BEFORE "${COMPAT}/include")
include_directories(
    "${OLS_TOP}/include"
    "${OLS_SRC}"
    "${PROJECT_BINARY_DIR}/src"
    "${DEPS}/include"
    "${LSQUIC_TOP}/include"
    "${LSQUIC_TOP}/src/liblsquic/ls-qpack"
    "${LSQUIC_TOP}/src/lshpack"
)

add_library(wepoll STATIC "${COMPAT}/wepoll/wepoll.c")
target_compile_definitions(wepoll PRIVATE
    epoll_create=wep_epoll_create epoll_create1=wep_epoll_create1
    epoll_close=wep_epoll_close epoll_ctl=wep_epoll_ctl epoll_wait=wep_epoll_wait)
target_compile_options(wepoll PRIVATE /W0)

add_library(olscompat STATIC
    "${COMPAT}/src/posix_fd.c"
    "${COMPAT}/src/posix_fs.c"
    "${COMPAT}/src/posix_proc.c"
    "${COMPAT}/src/posix_time.c"
    "${COMPAT}/src/posix_str.c"
    "${COMPAT}/src/posix_mman.c"
    "${COMPAT}/src/pthread_win.c"
    "${COMPAT}/src/posix_misc.c"
    "${COMPAT}/src/posix_net.c"
    "${COMPAT}/src/bssl_compat.c"
    "${COMPAT}/src/posix_crash.c"
    "${COMPAT}/src/posix_spawn.c"
    "${COMPAT}/src/win_control.c"
    "${COMPAT}/src/win_castore.c"
    "${COMPAT}/src/posix_crypt.c"
    "${COMPAT}/src/posix_handoff.c"
    "${COMPAT}/src/win_tools.c"
    "${COMPAT}/src/win_acl.c"
    "${COMPAT}/src/win_dev.c"
    "${COMPAT}/src/win_devca.c"
    "${COMPAT}/src/win_devstore.c"
    "${COMPAT}/src/win_db.c"
    "${COMPAT}/src/win_update.c"
    "${COMPAT}/src/win_tunnel.c"
)
target_include_directories(olscompat PRIVATE "${OLS_TOP}/win/common")
target_link_libraries(olscompat PUBLIC wepoll ws2_32 mswsock bcrypt ntdll advapi32 userenv iphlpapi synchronization dbghelp shell32 crypt32 version)

set(LSQUIC_INC "${LSQUIC_TOP}/include" "${LSQUIC_TOP}/wincompat"
               "${LSQUIC_TOP}/src/liblsquic" "${LSQUIC_TOP}/src/liblsquic/ls-qpack"
               "${LSQUIC_TOP}/src/lshpack" "${DEPS}/include")
include(GNUInstallDirs)
find_program(PERL_EXECUTABLE perl PATHS "C:/Program Files/Git/usr/bin" REQUIRED)
add_subdirectory("${LSQUIC_TOP}/src/liblsquic" "${PROJECT_BINARY_DIR}/lsquic")
set_property(TARGET lsquic PROPERTY INCLUDE_DIRECTORIES ${LSQUIC_INC})
target_compile_options(lsquic PRIVATE /W0 /std:clatest)
target_compile_definitions(lsquic PRIVATE
    "XXH_HEADER_NAME=\"lsquic_xxhash.h\""
    "LSQPACK_ENC_LOGGER_HEADER=\"lsquic_qpack_enc_logger.h\""
    "LSQPACK_DEC_LOGGER_HEADER=\"lsquic_qpack_dec_logger.h\""
    LSQUIC_DEBUG_NEXT_ADV_TICK=0)

set(OLS_DIRS adns edio extensions http log4cxx lsiapi lsr main modules quic shm socket sslpp ssi thread util h2)
foreach(d ${OLS_DIRS})
    add_subdirectory("${OLS_SRC}/${d}" "${PROJECT_BINARY_DIR}/src/${d}")
endforeach()

set(OLS_LIBS cache modgzip lsiapi main http ssi registry cgi fcgi scgi uwsgi jk extensions
             lsapi proxy socket sslpp lsshm thread log4cxx adns quic h2 util lsr edio)
foreach(t ${OLS_LIBS})
    if(TARGET ${t})
        target_compile_options(${t} PRIVATE "/FI${COMPAT}/include/ols_win.h")
    endif()
endforeach()

set(BCRYPT_SRC "${OLS_TOP}/.deps/libbcrypt")
add_library(lsbcrypt STATIC "${BCRYPT_SRC}/bcrypt.c" "${BCRYPT_SRC}/crypt_blowfish/crypt_blowfish.c"
            "${BCRYPT_SRC}/crypt_blowfish/crypt_gensalt.c" "${BCRYPT_SRC}/crypt_blowfish/wrapper.c")
target_include_directories(lsbcrypt PRIVATE "${BCRYPT_SRC}" "${BCRYPT_SRC}/crypt_blowfish")
target_compile_options(lsbcrypt PRIVATE "/FI${COMPAT}/include/ols_win.h" /W0)
target_include_directories(http PRIVATE "${BCRYPT_SRC}")

add_library(olsimport STATIC "${COMPAT}/src/win_apache.cpp" "${COMPAT}/src/win_acme.cpp")
target_include_directories(olsimport PRIVATE "${OLS_TOP}/win/common")

set(LUA_SRC lsluaengine.cpp edluastream.cpp lsluaapi.cpp lsluasession.cpp lsluaheader.cpp
            lsluashared.cpp lsluaregex.cpp modlua.cpp)
list(TRANSFORM LUA_SRC PREPEND "${OLS_SRC}/modules/lua/")
set_source_files_properties(${LUA_SRC} PROPERTIES COMPILE_DEFINITIONS "myParam=lsLuaParam"
                            INCLUDE_DIRECTORIES "${DEPS}/include/luajit-2.1")
add_executable(openlitespeed
    "${OLS_SRC}/modules/prelinkedmods.cpp"
    "${OLS_SRC}/modules/modacme/mod_acme.c"
    "${OLS_SRC}/modules/uploadprogress/uploadprogress.cpp"
    "${OLS_SRC}/modules/modreqparser/modreqparser.cpp"
    ${LUA_SRC}
    "${OLS_SRC}/modules/modsecurity-ls/mod_security.cpp"
    "${OLS_SRC}/httpdtest.cpp"
    "${OLS_SRC}/main.cpp")
target_compile_options(openlitespeed PRIVATE "/FI${COMPAT}/include/ols_win.h")
option(OLS_POISON_NEW "Fill heap allocations with a pattern to expose uninitialized fields" OFF)
if(OLS_POISON_NEW)
    target_sources(openlitespeed PRIVATE "${COMPAT}/src/poison_new.cpp")
endif()
target_link_options(openlitespeed PRIVATE /STACK:8388608 /OPT:REF /OPT:ICF /WHOLEARCHIVE:util /WHOLEARCHIVE:lsr)
target_link_libraries(openlitespeed
    ${OLS_LIBS} lsquic lsbcrypt olscompat olsimport
    "${DEPS}/lib/ssl.lib" "${DEPS}/lib/crypto.lib" "${DEPS}/lib/decrepit.lib"
    "${DEPS}/lib/zs.lib" "${DEPS}/lib/pcre2-8-static.lib" "${DEPS}/lib/libexpatMT.lib"
    "${DEPS}/lib/brotlienc.lib" "${DEPS}/lib/brotlidec.lib" "${DEPS}/lib/brotlicommon.lib"
    "${DEPS}/lib/udns.lib" "${DEPS}/lib/maxminddb.lib" "${DEPS}/lib/IP2Location.lib"
    "${DEPS}/lib/luajit.lib"
    "${DEPS}/lib/modsecurity.lib" "${DEPS}/lib/injection.lib" "${DEPS}/lib/mbedcrypto.lib"
    "${DEPS}/lib/yajl.lib" "${DEPS}/lib/libxml2s.lib"
    ws2_32 mswsock bcrypt crypt32 iphlpapi userenv advapi32 ntdll winhttp)
file(STRINGS "${OLS_TOP}/win/RELEASE" OLS_RELEASE LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OLS_TOP}/win/RELEASE")
add_executable(olstray WIN32 "${OLS_TOP}/win/tray/olstray.c" "${OLS_TOP}/win/tray/olstray.rc")
target_include_directories(olstray PRIVATE "${OLS_TOP}/win/common")
target_compile_definitions(olstray PRIVATE "OLS_RELEASE=\"${OLS_RELEASE}\"")
target_link_libraries(olstray user32 gdi32 shell32 advapi32 ole32 version comdlg32 winhttp bcrypt)
add_executable(lsrecaptcha "${OLS_TOP}/win/recaptcha/lsrecaptcha.c")
target_link_libraries(lsrecaptcha winhttp)
add_executable(setup WIN32 "${OLS_TOP}/win/setup/setup.c" "${OLS_TOP}/win/setup/setup.rc")
target_include_directories(setup PRIVATE "${OLS_TOP}/win/common")
target_compile_definitions(setup PRIVATE "OLS_RELEASE=\"${OLS_RELEASE}\"")
target_link_libraries(setup user32 gdi32 shell32 ole32 advapi32 comctl32 cabinet uuid ws2_32 version)
target_link_options(setup PRIVATE /MANIFEST:NO)
add_executable(olspack "${OLS_TOP}/win/setup/setup.c")
target_compile_definitions(olspack PRIVATE OLS_PACK_TOOL)
target_include_directories(olspack PRIVATE "${OLS_TOP}/win/common")
target_link_libraries(olspack user32 gdi32 shell32 ole32 advapi32 comctl32 cabinet uuid ws2_32 version)
if(TARGET mod_acme)
    set_target_properties(mod_acme PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
target_sources(lsquic PRIVATE "${LSQUIC_TOP}/src/liblsquic/lsquic_stock_shi.c")
