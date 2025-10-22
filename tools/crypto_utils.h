#pragma once
#include <string>

std::string generate_uuid_hex();

std::string pbkdf2_hash_hex(const std::string &password, const std::string &salt_hex, int iteration = 1000000,
                            int keylen = 32);
