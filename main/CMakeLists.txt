idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")


# Add compiler flags to handle format specifier warnings
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-format     # Disables format specifier warnings
)