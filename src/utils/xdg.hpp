#pragma once


#include <string>

/**
 * Returns a path for the given filename, according to the XDG base
 * directory specification, see
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */
std::string xdg_config_path(const std::string& filename);
std::string xdg_data_path(const std::string& filename);
