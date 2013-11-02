#ifndef IRC_MESSAGE_INCLUDED
# define IRC_MESSAGE_INCLUDED

#include <vector>
#include <string>
#include <ostream>

class IrcMessage
{
public:
  explicit IrcMessage(std::string&& line);
  explicit IrcMessage(std::string&& prefix, std::string&& command, std::vector<std::string>&& args);
  explicit IrcMessage(std::string&& command, std::vector<std::string>&& args);
  ~IrcMessage();

  std::string prefix;
  std::string command;
  std::vector<std::string> arguments;

  IrcMessage(const IrcMessage&) = delete;
  IrcMessage(IrcMessage&&) = delete;
  IrcMessage& operator=(const IrcMessage&) = delete;
  IrcMessage& operator=(IrcMessage&&) = delete;
};

std::ostream& operator<<(std::ostream& os, const IrcMessage& message);

#endif // IRC_MESSAGE_INCLUDED
