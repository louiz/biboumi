#ifndef XDG_HPP_INCLUDED
#define XDG_HPP_INCLUDED

#include <string>

/**
 * Returns a path for the given filename, according to the XDG base
 * directory specification, see
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */
std::string xdg_config_path(const std::string& filename);

#endif /* XDG_HPP_INCLUDED */
