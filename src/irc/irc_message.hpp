#pragma once


#include <vector>
#include <string>
#include <ostream>

class IrcMessage
{
public:
  IrcMessage(std::string&& line);
  IrcMessage(std::string&& prefix, std::string&& command, std::vector<std::string>&& args);
  IrcMessage(std::string&& command, std::vector<std::string>&& args);
  ~IrcMessage() = default;

  IrcMessage(const IrcMessage&) = delete;
  IrcMessage(IrcMessage&&) = delete;
  IrcMessage& operator=(const IrcMessage&) = delete;
  IrcMessage& operator=(IrcMessage&&) = delete;

  std::string prefix;
  std::string command;
  std::vector<std::string> arguments;
};

std::ostream& operator<<(std::ostream& os, const IrcMessage& message);


