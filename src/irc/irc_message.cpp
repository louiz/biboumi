#include <irc/irc_message.hpp>
#include <iostream>

IrcMessage::IrcMessage(std::string&& line)
{
  std::string::size_type pos;

  // optional prefix
  if (line[0] == ':')
    {
      pos = line.find(" ");
      this->prefix = line.substr(1, pos);
      line = line.substr(pos + 1, std::string::npos);
    }
  // command
  pos = line.find(" ");
  this->command = line.substr(0, pos);
  line = line.substr(pos + 1, std::string::npos);
  // arguments
  do
    {
      if (line[0] == ':')
        {
          this->arguments.emplace_back(line.substr(1, std::string::npos));
          break ;
        }
      pos = line.find(" ");
      this->arguments.emplace_back(line.substr(0, pos));
      line = line.substr(pos + 1, std::string::npos);
    } while (pos != std::string::npos);
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

IrcMessage::~IrcMessage()
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
