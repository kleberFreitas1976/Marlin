FILE(GLOB app_sources ${CMAKE_SOURCE_DIR}/src/*.c)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "www/wifi.html")

# snprintf ja corta com seguranca; os nomes de arquivo sao limitados bem abaixo
# dos 255 caracteres que o compilador supoe para d_name.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)
