#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nodepanda {

class CryptoEngine {
public:
    static std::array<uint8_t, 32> digestBytes(const uint8_t* data, size_t len);
    static std::string sha256(const std::string& input);
    static std::array<uint8_t, 32> hmacBytes(const std::string& key, const std::string& message);
    static std::string hmacSha256(const std::string& key, const std::string& message);
    static std::string deriveKey(const std::string& password, const std::string& salt, int rounds = 10000);
    static std::string generateUuid();
    static std::string randomHex(size_t byteCount = 16);
    static bool secureEquals(const std::string& a, const std::string& b);
    static std::string base64Encode(const std::string& input);
    static std::string base64Decode(const std::string& encoded);
    static std::string toHex(const uint8_t* data, size_t len);
    static std::vector<uint8_t> fromHex(const std::string& hex);
    static std::string encryptPayload(const std::string& plaintext, const std::string& password);
    static std::string decryptPayload(const std::string& payloadHex, const std::string& password, bool& ok);
};

}
