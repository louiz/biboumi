#include <xmpp/biboumi_component.hpp>
#include <utils/timed_events.hpp>
#include <network/poller.hpp>
#include <config/config.hpp>
#include <logger/logger.hpp>
#include <utils/xdg.hpp>
#include <utils/reload.hpp>

#ifdef UDNS_FOUND
# include <network/dns_handler.hpp>
#endif

#include <atomic>
#include <csignal>

#include <identd/identd_server.hpp>

// A flag set by the SIGINT signal handler.
static std::atomic<bool> stop(false);
// Flag set by the SIGUSR1/2 signal handler.
static std::atomic<bool> reload(false);
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
    log_error("Configuration error: empty value for option ", missing_option, ".");
  log_error("Please provide a configuration file filled like this:\n\n"
            "hostname=irc.example.com\npassword=S3CR3T");
  return 1;
}

int display_help()
{
  std::cout << "Usage: biboumi [configuration_file]" << std::endl;
  return 0;
}

static void sigint_handler(int sig, siginfo_t*, void*)
{
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
    {
      const std::string arg = av[1];
      if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-')
        {
          if (arg == "--help")
            return display_help();
          else
            {
              std::cerr << "Unknow command line option: " << arg << std::endl;
              return 1;
            }
        }
    }
  const std::string conf_filename = ac > 1 ? av[1] : xdg_config_path("biboumi.cfg");
  std::cout << "Using configuration file: " << conf_filename << std::endl;

  if (!Config::read_conf(conf_filename))
    return config_help("");

  const std::string password = Config::get("password", "");
  if (password.empty())
    return config_help("password");
  const std::string hostname = Config::get("hostname", "");
  if (hostname.empty())
    return config_help("hostname");


#ifdef USE_DATABASE
  try {
    open_database();
  } catch (...) {
    return 1;
  }
#endif

  // Block the signals we want to manage. They will be unblocked only during
  // the epoll_pwait or ppoll calls. This avoids some race conditions,
  // explained in man 2 pselect on linux
  sigset_t mask{};
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Install the signals used to exit the process cleanly, or reload the
  // config
  struct sigaction on_sigint;
  on_sigint.sa_sigaction = &sigint_handler;
  // All signals must be blocked while a signal handler is running
  sigfillset(&on_sigint.sa_mask);
  // we want to catch that signal only once.
  // Sending SIGINT again will "force" an exit
  on_sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &on_sigint, nullptr);
  sigaction(SIGTERM, &on_sigint, nullptr);

  // Install a signal to reload the config on SIGUSR1/2
  struct sigaction on_sigusr;
  on_sigusr.sa_sigaction = &sigusr_handler;
  sigfillset(&on_sigusr.sa_mask);
  on_sigusr.sa_flags = 0;
  sigaction(SIGUSR1, &on_sigusr, nullptr);
  sigaction(SIGUSR2, &on_sigusr, nullptr);

  auto p = std::make_shared<Poller>();

#ifdef UDNS_FOUND
  DNSHandler dns_handler(p);
#endif

  auto xmpp_component =
    std::make_shared<BiboumiComponent>(p, hostname, password);
  xmpp_component->start();

  IdentdServer identd(*xmpp_component, p, static_cast<uint16_t>(Config::get_int("identd_port", 113)));

  auto timeout = TimedEventsManager::instance().get_timeout();
  while (p->poll(timeout) != -1)
  {
    TimedEventsManager::instance().execute_expired_events();
    // Check for empty irc_clients (not connected, or with no joined
    // channel) and remove them
    xmpp_component->clean();
    identd.clean();
    if (stop)
    {
      log_info("Signal received, exiting...");
#ifdef SYSTEMD_FOUND
      sd_notify(0, "STOPPING=1");
#endif
      exiting = true;
      stop.store(false);
      xmpp_component->shutdown();
#ifdef UDNS_FOUND
      dns_handler.destroy();
#endif
      identd.shutdown();
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
    if (!exiting &&
        !xmpp_component->is_connected() &&
        !xmpp_component->is_connecting())
    {
      if (xmpp_component->ever_auth)
        {
          static const std::string reconnect_name{"XMPP reconnection"};
          if (xmpp_component->first_connection_try == true)
            { // immediately re-try to connect
              xmpp_component->reset();
              xmpp_component->start();
            }
          else if (!TimedEventsManager::instance().find_event(reconnect_name))
            { // Re-connecting failed, we now try only each few seconds
              auto reconnect_later = [xmpp_component]()
              {
                xmpp_component->reset();
                xmpp_component->start();
              };
              TimedEvent event(std::chrono::steady_clock::now() + 2s, reconnect_later, reconnect_name);
              TimedEventsManager::instance().add_event(std::move(event));
            }
        }
      else
        {
#ifdef UDNS_FOUND
          dns_handler.destroy();
#endif
          identd.shutdown();
        }
    }
    // If the only existing connection is the one to the XMPP component:
    // close the XMPP stream.
    if (exiting && xmpp_component->is_connecting())
      xmpp_component->close();
    if (exiting && p->size() == 1 && xmpp_component->is_document_open())
      xmpp_component->close_document();
    if (exiting) // If we are exiting, do not wait for any timed event
      timeout = utils::no_timeout;
    else
      timeout = TimedEventsManager::instance().get_timeout();
  }
  if (!xmpp_component->ever_auth)
    return 1; // To signal that the process did not properly start
  log_info("All connections cleanly closed, have a nice day.");
  return 0;
}
