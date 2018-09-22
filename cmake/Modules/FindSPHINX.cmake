find_program(SPHINX_BIN
    NAMES sphinx-build
    HINTS $ENV{SPHINX_DIR}
    PATH_SUFFIXES bin
    DOC "Sphinx executable to build the documentation"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SPHINX REQUIRED_VARS SPHINX_BIN)

mark_as_advanced(SPHINX_EXECUTABLE)
