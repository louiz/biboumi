#ifndef STRING_UTILS_HPP_INCLUDED
#define STRING_UTILS_HPP_INCLUDED

#include <vector>
#include <string>

bool to_bool(const std::string& val);
std::vector<std::string> cut(const std::string& val, const std::size_t size);

#endif /* STRING_UTILS_HPP_INCLUDED */
