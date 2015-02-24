#ifndef RELOAD_HPP_INCLUDED
#define RELOAD_HPP_INCLUDED

/**
 * Reload the server's configuration, and close the logger (so that it
 * closes its files etc, to take into account the new configuration)
 */
void reload_process();

#endif /* RELOAD_HPP_INCLUDED */
