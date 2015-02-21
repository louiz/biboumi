#include <network/poller.hpp>
#include <logger/logger.hpp>
#include <utils/timed_events.hpp>

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

Poller::Poller()
{
#if POLLER == POLL
  this->nfds = 0;
#elif POLLER == EPOLL
  this->epfd = ::epoll_create1(0);
  if (this->epfd == -1)
    {
      log_error("epoll failed: " << strerror(errno));
      throw std::runtime_error("Could not create epoll instance");
    }
#endif
}

Poller::~Poller()
{
}

void Poller::add_socket_handler(SocketHandler* socket_handler)
{
  // Don't do anything if the socket is already managed
  const auto it = this->socket_handlers.find(socket_handler->get_socket());
  if (it != this->socket_handlers.end())
    return ;

  this->socket_handlers.emplace(socket_handler->get_socket(), socket_handler);

  // We always watch all sockets for receive events
#if POLLER == POLL
  this->fds[this->nfds].fd = socket_handler->get_socket();
  this->fds[this->nfds].events = POLLIN;
  this->nfds++;
#endif
#if POLLER == EPOLL
  struct epoll_event event = {EPOLLIN, {socket_handler}};
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_ADD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      log_error("epoll_ctl failed: " << strerror(errno));
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
      log_error("epoll_ctl failed: " << strerror(errno));
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
  struct epoll_event event = {EPOLLIN|EPOLLOUT, {socket_handler}};
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_MOD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      log_error("epoll_ctl failed: " << strerror(errno));
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
  struct epoll_event event = {EPOLLIN, {socket_handler}};
  const int res = ::epoll_ctl(this->epfd, EPOLL_CTL_MOD, socket_handler->get_socket(), &event);
  if (res == -1)
    {
      log_error("epoll_ctl failed: " << strerror(errno));
      throw std::runtime_error("Could not modify socket flags in epoll");
    }
#endif
}

int Poller::poll(const std::chrono::milliseconds& timeout)
{
  if (this->socket_handlers.empty() && timeout == utils::no_timeout)
    return -1;
#if POLLER == POLL
  int nb_events = ::poll(this->fds, this->nfds, timeout.count());
  if (nb_events < 0)
    {
      if (errno == EINTR)
        return true;
      log_error("poll failed: " << strerror(errno));
      throw std::runtime_error("Poll failed");
    }
  // We cannot possibly have more ready events than the number of fds we are
  // watching
  assert(static_cast<unsigned int>(nb_events) <= this->nfds);
  for (size_t i = 0; i <= this->nfds && nb_events != 0; ++i)
    {
      if (this->fds[i].revents == 0)
        continue;
      else if (this->fds[i].revents & POLLIN)
        {
          auto socket_handler = this->socket_handlers.at(this->fds[i].fd);
          socket_handler->on_recv();
          nb_events--;
        }
      else if (this->fds[i].revents & POLLOUT)
        {
          auto socket_handler = this->socket_handlers.at(this->fds[i].fd);
          if (socket_handler->is_connected())
            socket_handler->on_send();
          else
            socket_handler->connect();
          nb_events--;
        }
    }
  return 1;
#elif POLLER == EPOLL
  static const size_t max_events = 12;
  struct epoll_event revents[max_events];
  const int nb_events = ::epoll_wait(this->epfd, revents, max_events, timeout.count());
  if (nb_events == -1)
    {
      if (errno == EINTR)
        return 0;
      log_error("epoll wait: " << strerror(errno));
      throw std::runtime_error("Epoll_wait failed");
    }
  for (int i = 0; i < nb_events; ++i)
    {
      auto socket_handler = static_cast<SocketHandler*>(revents[i].data.ptr);
      if (revents[i].events & EPOLLIN)
        socket_handler->on_recv();
      else if (revents[i].events & EPOLLOUT)
        {
          if (socket_handler->is_connected())
            socket_handler->on_send();
          else
            socket_handler->connect();
        }
    }
  return nb_events;
#endif
}

size_t Poller::size() const
{
  return this->socket_handlers.size();
}
