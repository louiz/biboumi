#pragma once

#include <string>

std::string get_handshake_digest(const std::string& stream_id, const std::string& secret);

