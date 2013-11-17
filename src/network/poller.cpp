#include <network/poller.hpp>

#include <assert.h>
#include <stdio.h>

#include <cstring>
#include <iostream>

Poller::Poller()
{
  std::cout << "Poller()" << std::endl;
#if POLLER == POLL
  memset(this->fds, 0, sizeof(this->fds));
  this->nfds = 0;
#elif POLLER == EPOLL
  this->epfd = ::epoll_create1(0);
  if (this->epfd == -1)
    {
      perror("epoll");
      throw std::runtime_error("Could not create epoll instance");
    }
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
#if POLLER == EPOLL
  struct epoll_event event;
  event.data.ptr = socket_handler.get();
  event.events = EPOLLIN;
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_ADD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      perror("epoll_ctl");
      throw std::runtime_error("Could not add socket to epoll");
    }
#endif
}

void Poller::remove_socket_handler(const socket_t socket)
{
  const auto it = this->socket_handlers.find(socket);
  if (it == this->socket_handlers.end())
    throw std::runtime_error("Trying to remove a SocketHandler that is not managed");
  this->socket_handlers.erase(it);

#if POLLER == POLL
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
#elif POLLER == EPOLL
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_DEL, socket, nullptr);
  if (res == -1)
    {
      perror("epoll_ctl");
      throw std::runtime_error("Could not remove socket from epoll");
    }
#endif
}

void Poller::watch_send_events(SocketHandler* socket_handler)
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
  throw std::runtime_error("Cannot watch a non-registered socket for send events");
#elif POLLER == EPOLL
  struct epoll_event event;
  event.data.ptr = socket_handler;
  event.events = EPOLLIN|EPOLLOUT;
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_MOD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      perror("epoll_ctl");
      throw std::runtime_error("Could not modify socket flags in epoll");
    }
#endif
}

void Poller::stop_watching_send_events(SocketHandler* socket_handler)
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
  throw std::runtime_error("Cannot watch a non-registered socket for send events");
#elif POLLER == EPOLL
  struct epoll_event event;
  event.data.ptr = socket_handler;
  event.events = EPOLLIN;
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_MOD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      perror("epoll_ctl");
      throw std::runtime_error("Could not modify socket flags in epoll");
    }
#endif
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
#elif POLLER == EPOLL
  static const size_t max_events = 12;
  struct epoll_event revents[max_events];
  const int nb_events = epoll_wait(this->epfd, revents, max_events, -1);
  if (nb_events == -1)
    {
      perror("epoll_wait");
      throw std::runtime_error("Epoll_wait failed");
    }
  for (int i = 0; i < nb_events; ++i)
    {
      auto socket_handler = static_cast<SocketHandler*>(revents[i].data.ptr);
      if (revents[i].events & EPOLLIN)
        socket_handler->on_recv();
      if (revents[i].events & EPOLLOUT)
        socket_handler->on_send();
    }
#endif
  return true;
}
