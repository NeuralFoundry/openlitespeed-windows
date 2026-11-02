cmake_minimum_required(VERSION 3.16)
project(modsecurity C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(B ${MSC_SRC})
set(MBEDTLS_DIR ${B}/others/mbedtls)
set(TF ${MBEDTLS_DIR}/tf-psa-crypto)

add_compile_definitions(WIN32 _CRT_SECURE_NO_WARNINGS PCRE2_STATIC NOMINMAX WIN32_LEAN_AND_MEAN
                        MBEDTLS_CONFIG_FILE="mbedtls/mbedtls_config.h")
add_compile_options(/Zc:preprocessor /permissive- /utf-8 /bigobj)

add_library(injection STATIC ${B}/others/libinjection/src/libinjection_sqli.c
            ${B}/others/libinjection/src/libinjection_xss.c
            ${B}/others/libinjection/src/libinjection_html5.c)
target_compile_definitions(injection PRIVATE LIBINJECTION_VERSION="4.0.0")

add_library(mbedcrypto STATIC
    ${TF}/utilities/base64.c ${TF}/utilities/constant_time.c ${TF}/platform/platform_util.c
    ${TF}/extras/md.c ${TF}/drivers/builtin/src/md5.c ${TF}/drivers/builtin/src/sha1.c
    ${TF}/drivers/builtin/src/sha256.c ${TF}/drivers/builtin/src/sha512.c
    ${TF}/drivers/builtin/src/sha3.c ${TF}/drivers/builtin/src/ripemd160.c
    ${TF}/drivers/builtin/src/psa_util_internal.c)
target_include_directories(mbedcrypto PRIVATE ${MBEDTLS_DIR}/include ${TF}/include ${TF}/core
    ${TF}/extras ${TF}/library ${TF}/utilities ${TF}/drivers/builtin/include ${TF}/drivers/builtin/src)

set(PACKAGE_BUGREPORT "security@modsecurity.org")
set(PACKAGE_NAME "modsecurity")
set(PACKAGE_VERSION "3.0.17")
set(PACKAGE_STRING "modsecurity 3.0.17")
set(PACKAGE_TARNAME "modsecurity")
set(HAVE_YAJL 1)
set(HAVE_MAXMIND 1)
set(HAVE_GEOIP 0)
set(HAVE_SSDEEP 0)
set(HAVE_LMDB 0)
set(HAVE_LUA 0)
set(HAVE_LIBXML2 1)
set(HAVE_CURL 0)
include(${B}/build/win32/ConfigureChecks.cmake)
configure_file(${B}/build/win32/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/gen/src/config.h)

file(GLOB_RECURSE SRCS ${B}/src/*.cc)
add_library(modsecurity STATIC ${SRCS})
target_compile_definitions(modsecurity PRIVATE WITH_PCRE2 WITH_YAJL WITH_MAXMIND WITH_LIBXML2 LIBXML_STATIC)
target_include_directories(modsecurity PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen
    ${CMAKE_CURRENT_SOURCE_DIR}/shim ${B} ${B}/headers ${B}/others ${MBEDTLS_DIR}/include
    ${TF}/include ${TF}/drivers/builtin/include ${OLS_DEPS}/include ${OLS_DEPS}/include/libxml2)

install(TARGETS modsecurity injection mbedcrypto ARCHIVE DESTINATION lib)
install(DIRECTORY ${B}/headers/modsecurity DESTINATION include)
