#include <network/poller.hpp>

#include <assert.h>
#include <cstring>
#include <iostream>


Poller::Poller()
{
  std::cout << "Poller()" << std::endl;
#if POLLER == POLL
  memset(this->fds, 0, sizeof(this->fds));
  this->nfds = 0;
#endif
}

Poller::~Poller()
{
  std::cout << "~Poller()" << std::endl;
}

void Poller::add_socket_handler(std::shared_ptr<SocketHandler> socket_handler)
{
  // Raise an error if that socket is already in the list
  const auto it = this->socket_handlers.find(socket_handler->get_socket());
  if (it != this->socket_handlers.end())
    throw std::runtime_error("Trying to insert SocketHandler already managed");

  this->socket_handlers.emplace(socket_handler->get_socket(), socket_handler);
  socket_handler->set_poller(this);

  // We always watch all sockets for receive events
#if POLLER == POLL
  this->fds[this->nfds].fd = socket_handler->get_socket();
  this->fds[this->nfds].events = POLLIN;
  this->nfds++;
#endif
}

void Poller::remove_socket_handler(const socket_t socket)
{
  const auto it = this->socket_handlers.find(socket);
  if (it == this->socket_handlers.end())
    throw std::runtime_error("Trying to remove a SocketHandler that is not managed");
  this->socket_handlers.erase(it);
  for (size_t i = 0; i < this->nfds; i++)
    {
      if (this->fds[i].fd == socket)
        {
          // Move all subsequent pollfd by one on the left, erasing the
          // value of the one we remove
          for (size_t j = i; j < this->nfds - 1; ++j)
            {
              this->fds[j].fd = this->fds[j+1].fd;
              this->fds[j].events= this->fds[j+1].events;
            }
          this->nfds--;
        }
    }
}

void Poller::watch_send_events(const SocketHandler* const socket_handler)
{
#if POLLER == POLL
  for (size_t i = 0; i <= this->nfds; ++i)
    {
      if (this->fds[i].fd == socket_handler->get_socket())
        {
          this->fds[i].events = POLLIN|POLLOUT;
          return;
        }
    }
#endif
  throw std::runtime_error("Cannot watch a non-registered socket for send events");
}

void Poller::stop_watching_send_events(const SocketHandler* const socket_handler)
{
#if POLLER == POLL
  for (size_t i = 0; i <= this->nfds; ++i)
    {
      if (this->fds[i].fd == socket_handler->get_socket())
        {
          this->fds[i].events = POLLIN;
          return;
        }
    }
#endif
  throw std::runtime_error("Cannot watch a non-registered socket for send events");
}

bool Poller::poll()
{
#if POLLER == POLL
  if (this->nfds == 0)
    return false;
  int res = ::poll(this->fds, this->nfds, -1);
  if (res < 0)
    {
      perror("poll");
      throw std::runtime_error("Poll failed");
    }
  // We cannot possibly have more ready events than the number of fds we are
  // watching
  assert(static_cast<unsigned int>(res) <= this->nfds);
  for (size_t i = 0; i <= this->nfds && res != 0; ++i)
    {
      if (this->fds[i].revents == 0)
        continue;
      else if (this->fds[i].revents & POLLIN)
        {
          auto socket_handler = this->socket_handlers.at(this->fds[i].fd);
          socket_handler->on_recv();
          res--;
        }
      else if (this->fds[i].revents & POLLOUT)
        {
          auto socket_handler = this->socket_handlers.at(this->fds[i].fd);
          socket_handler->on_send();
          res--;
        }
    }
#endif
  return true;
}
