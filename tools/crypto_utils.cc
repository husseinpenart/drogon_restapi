#include "crypto_utils.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <uuid/uuid.h>
#include <iomanip>
#include <sstream>
#include <vector>

static std::string bytes_to_hex(const unsigned char *bytes, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

std::string generate_uuid_hex() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    return bytes_to_hex(uuid, 16);
}

std::string pbkdf2_hash_hex(const std::string &password, const std::string &salt_hex,
                            int iterations, int keylen) {
    // convert salt_hex back to bytes
    std::vector<unsigned char> salt_bytes;
    salt_bytes.reserve(salt_hex.size() / 2);
    for (size_t i = 0; i < salt_hex.size(); i += 2) {
        std::string byte_str = salt_hex.substr(i, 2);
        auto byte = static_cast<unsigned char>(strtol(byte_str.c_str(), nullptr, 16));
        salt_bytes.push_back(byte);
    }

    std::vector<unsigned char> derived(keylen);

    // Use PBKDF2-HMAC-SHA256
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt_bytes.data(), static_cast<int>(salt_bytes.size()),
                          iterations, EVP_sha256(), keylen, derived.data()) != 1) {
        // handle error (here we just return empty string on failure)
        return {};
    }

    return bytes_to_hex(derived.data(), derived.size());
}
