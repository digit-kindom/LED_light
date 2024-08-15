idf_component_register(SRC_DIRS          "." "${MATTER_SDK_PATH}/examples/platform/esp32/mode-support"
                        PRIV_INCLUDE_DIRS  "." "${MATTER_SDK_PATH}/examples/platform/esp32/mode-support")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
