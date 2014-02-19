#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>
#include <config/config.hpp>
#include <logger/logger.hpp>

#include <iostream>
#include <memory>
#include <atomic>

#include <signal.h>

// A flag set by the SIGINT signal handler.
static volatile std::atomic<bool> stop(false);
// Flag set by the SIGUSR1/2 signal handler.
static volatile std::atomic<bool> reload(false);
// A flag indicating that we are wanting to exit the process. i.e: if this
// flag is set and all connections are closed, we can exit properly.
static bool exiting = false;

/**
 * Provide an helpful message to help the user write a minimal working
 * configuration file.
 */
int config_help(const std::string& missing_option)
{
  if (!missing_option.empty())
    std::cerr << "Error: empty value for option " << missing_option << "." << std::endl;
  std::cerr <<
    "Please provide a configuration file filled like this:\n\n"
    "hostname=irc.example.com\npassword=S3CR3T"
            << std::endl;
  return 1;
}

static void sigint_handler(int, siginfo_t*, void*)
{
  stop = true;
}

static void sigusr_handler(int, siginfo_t*, void*)
{
  reload = true;
}

int main(int ac, char** av)
{
  if (ac > 1)
    Config::filename = av[1];
  Config::file_must_exist = true;
  std::cerr << "Using configuration file: " << Config::filename << std::endl;

  std::string password;
  try { // The file must exist
    password = Config::get("password", "");
  }
  catch (const std::ios::failure& e) {
    return config_help("");
  }
  const std::string hostname = Config::get("hostname", "");
  if (password.empty())
    return config_help("password");
  if (hostname.empty())
    return config_help("hostname");
  std::shared_ptr<XmppComponent> xmpp_component =
    std::make_shared<XmppComponent>(hostname, password);

  Poller p;
  p.add_socket_handler(xmpp_component);
  if (!xmpp_component->start())
  {
    log_info("Exiting");
    return -1;
  }

  // Install the signals used to exit the process cleanly, or reload the
  // config
  sigset_t mask;
  sigemptyset(&mask);
  struct sigaction on_sigint;
  on_sigint.sa_sigaction = &sigint_handler;
  on_sigint.sa_mask = mask;
  // we want to catch that signal only once.
  // Sending SIGINT again will "force" an exit
  on_sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &on_sigint, nullptr);
  sigaction(SIGTERM, &on_sigint, nullptr);

  // Install a signal to reload the config on SIGUSR1/2
  struct sigaction on_sigusr;
  on_sigusr.sa_sigaction = &sigusr_handler;
  on_sigusr.sa_mask = mask;
  sigaction(SIGUSR1, &on_sigusr, nullptr);
  sigaction(SIGUSR2, &on_sigusr, nullptr);

  const std::chrono::milliseconds timeout(-1);
  while (p.poll(timeout) != -1 || !exiting)
  {
    // Check for empty irc_clients (not connected, or with no joined
    // channel) and remove them
    xmpp_component->clean();
    if (stop)
    {
      log_info("Signal received, exiting...");
      exiting = true;
      stop = false;
      xmpp_component->shutdown();
    }
    if (reload)
    {
      // Closing the config will just force it to be reopened the next time
      // a configuration option is needed
      log_info("Signal received, reloading the config...");
      Config::close();
      // Destroy the logger instance, to be recreated the next time a log
      // line needs to be written
      Logger::instance().reset();
      reload = false;
    }
    // If the only existing connection is the one to the XMPP component:
    // close the XMPP stream.
    if (exiting && p.size() == 1 && xmpp_component->is_document_open())
      xmpp_component->close_document();
  }
  log_info("All connection cleanely closed, have a nice day.");
  return 0;
}
