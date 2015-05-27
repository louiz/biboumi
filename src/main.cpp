#include <xmpp/biboumi_component.hpp>
#include <utils/timed_events.hpp>
#include <network/poller.hpp>
#include <config/config.hpp>
#include <logger/logger.hpp>
#include <utils/reload.hpp>

#include <iostream>
#include <memory>
#include <atomic>
#include <cstdlib>

#include <signal.h>

#ifdef CARES_FOUND
# include <network/dns_handler.hpp>
#endif

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

static void sigint_handler(int sig, siginfo_t*, void*)
{
  // We reset the SIGTERM or SIGINT (the one that didn't trigger this
  // handler) signal handler to its default value.  This avoid calling this
  // handler twice, if the process receive both signals in a quick
  // succession.
  int sig_to_reset = (sig == SIGINT? SIGTERM: SIGINT);
  sigset_t mask;
  sigemptyset(&mask);
  struct sigaction sigreset = {};
  sigreset.sa_handler = SIG_DFL;
  sigreset.sa_mask = mask;
  sigaction(sig_to_reset, &sigreset, nullptr);

  // In 2 seconds, repeat the same signal, to force the exit
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 2s,
                                    [sig]() { raise(sig); }));
  stop.store(true);
}

static void sigusr_handler(int, siginfo_t*, void*)
{
  reload.store(true);
}

int main(int ac, char** av)
{
  if (ac > 1)
    Config::filename = av[1];
  else
  {
    const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home && xdg_config_home[0] == '/')
      Config::filename = std::string{xdg_config_home} + "/" "biboumi" "/" "biboumi.cfg";
    else
    {
      const char* home = getenv("HOME");
      if (home)
        Config::filename = std::string{home} + "/" ".config" "/" "biboumi" "/" "biboumi.cfg";
      else
        Config::filename = "biboumi.cfg";
    }
  }

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

  // Block the signals we want to manage. They will be unblocked only during
  // the epoll_pwait or ppoll calls. This avoids some race conditions,
  // explained in man 2 pselect on linux
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Install the signals used to exit the process cleanly, or reload the
  // config
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
  on_sigusr.sa_flags = 0;
  sigaction(SIGUSR1, &on_sigusr, nullptr);
  sigaction(SIGUSR2, &on_sigusr, nullptr);

  auto p = std::make_shared<Poller>();
  auto xmpp_component =
    std::make_shared<BiboumiComponent>(p, hostname, password);
  xmpp_component->start();

#ifdef CARES_FOUND
  DNSHandler::instance.watch_dns_sockets(p);
#endif
  auto timeout = TimedEventsManager::instance().get_timeout();
  while (p->poll(timeout) != -1)
  {
    TimedEventsManager::instance().execute_expired_events();
    // Check for empty irc_clients (not connected, or with no joined
    // channel) and remove them
    xmpp_component->clean();
    if (stop)
    {
      log_info("Signal received, exiting...");
      exiting = true;
      stop.store(false);
      xmpp_component->shutdown();
      // Cancel the timer for a potential reconnection
      TimedEventsManager::instance().cancel("XMPP reconnection");
    }
    if (reload)
    {
      log_info("Signal received, reloading the config...");
      ::reload_process();
      reload.store(false);
    }
    // Reconnect to the XMPP server if this was not intended.  This may have
    // happened because we sent something invalid to it and it decided to
    // close the connection.  This is a bug that should be fixed, but we
    // still reconnect automatically instead of dropping everything
    if (!exiting && xmpp_component->ever_auth &&
        !xmpp_component->is_connected() &&
        !xmpp_component->is_connecting())
    {
      if (xmpp_component->first_connection_try == true)
      { // immediately re-try to connect
        xmpp_component->reset();
        xmpp_component->start();
      }
      else
      { // Re-connecting failed, we now try only each few seconds
        auto reconnect_later = [xmpp_component]()
        {
          xmpp_component->reset();
          xmpp_component->start();
        };
        TimedEvent event(std::chrono::steady_clock::now() + 2s,
                         reconnect_later, "XMPP reconnection");
        TimedEventsManager::instance().add_event(std::move(event));
      }
    }
    // If the only existing connection is the one to the XMPP component:
    // close the XMPP stream.
    if (exiting && xmpp_component->is_connecting())
      xmpp_component->close();
    if (exiting && p->size() == 1 && xmpp_component->is_document_open())
      xmpp_component->close_document();
#ifdef CARES_FOUND
    if (!exiting)
      DNSHandler::instance.watch_dns_sockets(p);
#endif
    if (exiting) // If we are exiting, do not wait for any timed event
      timeout = utils::no_timeout;
    else
      timeout = TimedEventsManager::instance().get_timeout();
  }
#ifdef CARES_FOUND
  DNSHandler::instance.destroy();
#endif
  log_info("All connections cleanly closed, have a nice day.");
  return 0;
}
