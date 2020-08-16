#include <irc/irc_message.hpp>
#include <iostream>

IrcMessage::IrcMessage(std::stringstream ss)
{
  if (ss.peek() == ':')
    {
      ss.ignore();
      ss >> this->prefix;
    }
  ss >> this->command;
  while (ss >> std::ws)
    {
      std::string arg;
      if (ss.peek() == ':')
        {
          ss.ignore();
          std::getline(ss, arg);
        }
      else
        {
          ss >> arg;
          if (arg.empty())
            break;
        }
      this->arguments.push_back(std::move(arg));
    }
}

IrcMessage::IrcMessage(std::string&& prefix,
                       std::string&& command,
                       std::vector<std::string>&& args):
  prefix(std::move(prefix)),
  command(std::move(command)),
  arguments(std::move(args))
{
}

IrcMessage::IrcMessage(std::string&& command,
                       std::vector<std::string>&& args):
  prefix(),
  command(std::move(command)),
  arguments(std::move(args))
{
}

std::ostream& operator<<(std::ostream& os, const IrcMessage& message)
{
  os << "IrcMessage";
  os << "[" << message.command << "]";
  for (const std::string& arg: message.arguments)
    {
      os << "{" << arg << "}";
    }
  if (!message.prefix.empty())
    os << "(from: " << message.prefix << ")";
  return os;
}
