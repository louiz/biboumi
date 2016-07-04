#pragma once


/**
 * Reload the server's configuration, and close the logger (so that it
 * closes its files etc, to take into account the new configuration)
 */
void reload_process();


