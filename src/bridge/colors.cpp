#include <bridge/colors.hpp>
#include <algorithm>

#include <iostream>

void remove_irc_colors(std::string& str)
{
  auto it = std::remove_if(str.begin(), str.end(),
                           [](const char c)
                           {
                             if (c == IRC_COLOR_BOLD_CHAR || c == IRC_COLOR_COLOR_CHAR ||
                                 c == IRC_COLOR_FIXED_CHAR || c == IRC_COLOR_RESET_CHAR ||
                                 c == IRC_COLOR_REVERSE_CHAR || c == IRC_COLOR_REVERSE2_CHAR ||
                                 c == IRC_COLOR_UNDERLINE_CHAR || c == IRC_COLOR_ITALIC_CHAR)
                               return true;
                             return false;
                           }
                           );
  str.erase(it, str.end());
}
