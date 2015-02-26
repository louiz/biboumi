#ifndef LIST_ELEMENT_HPP_INCLUDED
#define LIST_ELEMENT_HPP_INCLUDED

#include <string>

struct ListElement
{
  ListElement(const std::string& channel, const std::string& nb_users,
                 const std::string& topic):
    channel(channel),
    nb_users(nb_users),
    topic(topic){}

  std::string channel;
  std::string nb_users;
  std::string topic;
};

#endif /* LIST_ELEMENT_HPP_INCLUDED */
