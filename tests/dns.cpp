#include "catch.hpp"

#include <network/dns_handler.hpp>
#include <network/resolver.hpp>
#include <network/poller.hpp>

#include <utils/timed_events.hpp>

TEST_CASE("DNS resolver")
{
    Resolver resolver;
    Resolver resolver2;
    Resolver resolver3;

    /**
     * If we are using cares, we need to run a poller loop until each
     * resolution is finished. Without cares we get the result before
     * resolve() returns because it’s blocking.
     */
#ifdef CARES_FOUND
    auto p = std::make_shared<Poller>();

    const auto loop = [&p]()
      {
        do
          {
            DNSHandler::instance.watch_dns_sockets(p);
          }
        while (p->poll(utils::no_timeout) != -1);
      };
#else
    // We don’t need to do anything if we are not using cares.
    const auto loop = [](){};
#endif

    std::string hostname;
    std::string port = "6667";

    bool success = true;

    const auto error_cb = [&success](const std::string& hostname)
    {
        return [&success, hostname](const char *msg)
        {
            INFO("Failed to resolve " << hostname << ":" << msg);
            success = false;
        };
    };
    const auto success_cb = [&success](const std::string& hostname)
    {
        return [&success, hostname](const struct addrinfo *addr)
        {
            INFO("Successfully resolved " << hostname << ": " << addr_to_string(addr));
            success = true;
        };
    };

    hostname = "example.com";
    resolver.resolve(hostname, port,
                     success_cb(hostname), error_cb(hostname));
    hostname = "poez.io";
    resolver2.resolve(hostname, port,
                      success_cb(hostname), error_cb(hostname));
    hostname = "louiz.org";
    resolver3.resolve(hostname, port,
                      success_cb(hostname), error_cb(hostname));
    loop();
    CHECK(success);

    hostname = "this.should.fail.because.it.is..misformatted";
    resolver.resolve(hostname, port,
        success_cb(hostname), error_cb(hostname));
    loop();
    CHECK(!success);

    hostname = "this.should.fail.because.it.is.does.not.exist.invalid";
    resolver.resolve(hostname, port,
        success_cb(hostname), error_cb(hostname));
    loop();
    CHECK(!success);

    hostname = "localhost";
    resolver.resolve(hostname, port,
        success_cb(hostname), error_cb(hostname));
    loop();
    CHECK(success);

#ifdef CARES_FOUND
    DNSHandler::instance.destroy();
#endif
}
