#include "crypto_engine.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

namespace nodepanda {

namespace {

constexpr uint32_t kSha256Constants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

inline uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

void compressBlock(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + kSha256Constants[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

}

std::array<uint8_t, 32> CryptoEngine::digestBytes(const uint8_t* data, size_t len) {
    uint32_t state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    size_t offset = 0;
    while (offset + 64 <= len) {
        compressBlock(state, data + offset);
        offset += 64;
    }

    uint8_t tail[128];
    std::memset(tail, 0, sizeof(tail));
    size_t rem = len - offset;
    if (rem > 0) {
        std::memcpy(tail, data + offset, rem);
    }
    tail[rem] = 0x80u;

    size_t paddedLen = (rem < 56) ? 64 : 128;
    uint64_t bitLen = static_cast<uint64_t>(len) * 8ull;
    for (int i = 0; i < 8; ++i) {
        tail[paddedLen - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xffu);
    }

    compressBlock(state, tail);
    if (paddedLen == 128) {
        compressBlock(state, tail + 64);
    }

    std::array<uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>((state[i] >> 24) & 0xffu);
        out[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xffu);
        out[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8) & 0xffu);
        out[i * 4 + 3] = static_cast<uint8_t>(state[i] & 0xffu);
    }
    return out;
}

std::string CryptoEngine::toHex(const uint8_t* data, size_t len) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0x0fu]);
        out.push_back(kHex[data[i] & 0x0fu]);
    }
    return out;
}

std::vector<uint8_t> CryptoEngine::fromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    if (hex.size() % 2 != 0) return out;
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string CryptoEngine::sha256(const std::string& input) {
    auto bytes = digestBytes(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    return toHex(bytes.data(), bytes.size());
}

std::array<uint8_t, 32> CryptoEngine::hmacBytes(const std::string& key, const std::string& message) {
    uint8_t keyBlock[64];
    std::memset(keyBlock, 0, sizeof(keyBlock));
    if (key.size() > 64) {
        auto hashed = digestBytes(reinterpret_cast<const uint8_t*>(key.data()), key.size());
        std::memcpy(keyBlock, hashed.data(), 32);
    } else if (!key.empty()) {
        std::memcpy(keyBlock, key.data(), key.size());
    }

    std::vector<uint8_t> inner(64 + message.size());
    uint8_t outer[64 + 32];
    for (int i = 0; i < 64; ++i) {
        inner[i] = keyBlock[i] ^ 0x36u;
        outer[i] = keyBlock[i] ^ 0x5cu;
    }
    if (!message.empty()) {
        std::memcpy(inner.data() + 64, message.data(), message.size());
    }
    auto innerHash = digestBytes(inner.data(), inner.size());
    std::memcpy(outer + 64, innerHash.data(), 32);
    return digestBytes(outer, sizeof(outer));
}

std::string CryptoEngine::hmacSha256(const std::string& key, const std::string& message) {
    auto bytes = hmacBytes(key, message);
    return toHex(bytes.data(), bytes.size());
}

std::string CryptoEngine::deriveKey(const std::string& password, const std::string& salt, int rounds) {
    int iterations = std::max(1, rounds);
    std::string saltBlock = salt;
    saltBlock.push_back(0);
    saltBlock.push_back(0);
    saltBlock.push_back(0);
    saltBlock.push_back(1);

    auto u = hmacBytes(password, saltBlock);
    auto acc = u;
    for (int i = 1; i < iterations; ++i) {
        std::string prev(reinterpret_cast<const char*>(u.data()), u.size());
        u = hmacBytes(password, prev);
        for (size_t j = 0; j < 32; ++j) {
            acc[j] ^= u[j];
        }
    }
    return toHex(acc.data(), acc.size());
}

std::string CryptoEngine::randomHex(size_t byteCount) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    std::vector<uint8_t> buf(byteCount);
    for (size_t i = 0; i < byteCount; ++i) {
        buf[i] = static_cast<uint8_t>(dist(gen));
    }
    return toHex(buf.data(), buf.size());
}

std::string CryptoEngine::generateUuid() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    uint8_t b[16];
    for (int i = 0; i < 16; ++i) {
        b[i] = static_cast<uint8_t>(dist(gen));
    }
    b[6] = static_cast<uint8_t>((b[6] & 0x0fu) | 0x40u);
    b[8] = static_cast<uint8_t>((b[8] & 0x3fu) | 0x80u);

    std::string hex = toHex(b, 16);
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" +
           hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" + hex.substr(20, 12);
}

bool CryptoEngine::secureEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string CryptoEngine::base64Encode(const std::string& input) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i < input.size()) {
        uint32_t octetA = static_cast<uint8_t>(input[i++]);
        bool hasB = i < input.size();
        uint32_t octetB = hasB ? static_cast<uint8_t>(input[i++]) : 0;
        bool hasC = i < input.size();
        uint32_t octetC = hasC ? static_cast<uint8_t>(input[i++]) : 0;
        uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;
        out.push_back(kTable[(triple >> 18) & 0x3fu]);
        out.push_back(kTable[(triple >> 12) & 0x3fu]);
        out.push_back(hasB ? kTable[(triple >> 6) & 0x3fu] : '=');
        out.push_back(hasC ? kTable[triple & 0x3fu] : '=');
    }
    return out;
}

std::string CryptoEngine::base64Decode(const std::string& encoded) {
    int dec[256];
    std::fill(std::begin(dec), std::end(dec), -1);
    const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) dec[static_cast<unsigned char>(table[i])] = i;

    std::string out;
    int val = 0;
    int valb = -8;
    for (unsigned char c : encoded) {
        if (dec[c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + dec[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xff));
            valb -= 8;
        }
    }
    return out;
}

std::string CryptoEngine::encryptPayload(const std::string& plaintext, const std::string& password) {
    std::string salt = randomHex(16);
    std::string encKey = deriveKey(password, salt + ":enc", 4096);
    std::string macKey = deriveKey(password, salt + ":mac", 4096);

    std::vector<uint8_t> cipher(plaintext.size());
    for (size_t offset = 0; offset < plaintext.size(); offset += 32) {
        std::string counter = encKey + ":" + std::to_string(offset / 32);
        auto stream = digestBytes(reinterpret_cast<const uint8_t*>(counter.data()), counter.size());
        size_t chunk = std::min<size_t>(32, plaintext.size() - offset);
        for (size_t i = 0; i < chunk; ++i) {
            cipher[offset + i] = static_cast<uint8_t>(plaintext[offset + i]) ^ stream[i];
        }
    }

    std::string cipherHex = toHex(cipher.data(), cipher.size());
    std::string tag = hmacSha256(macKey, salt + ":" + cipherHex);
    return salt + ":" + tag + ":" + cipherHex;
}

std::string CryptoEngine::decryptPayload(const std::string& payloadHex, const std::string& password, bool& ok) {
    ok = false;
    size_t p1 = payloadHex.find(':');
    if (p1 == std::string::npos) return {};
    size_t p2 = payloadHex.find(':', p1 + 1);
    if (p2 == std::string::npos) return {};

    std::string salt = payloadHex.substr(0, p1);
    std::string tag = payloadHex.substr(p1 + 1, p2 - p1 - 1);
    std::string cipherHex = payloadHex.substr(p2 + 1);

    std::string encKey = deriveKey(password, salt + ":enc", 4096);
    std::string macKey = deriveKey(password, salt + ":mac", 4096);
    std::string expectedTag = hmacSha256(macKey, salt + ":" + cipherHex);
    if (!secureEquals(tag, expectedTag)) {
        return {};
    }

    auto cipher = fromHex(cipherHex);
    std::string plain(cipher.size(), '\0');
    for (size_t offset = 0; offset < cipher.size(); offset += 32) {
        std::string counter = encKey + ":" + std::to_string(offset / 32);
        auto stream = digestBytes(reinterpret_cast<const uint8_t*>(counter.data()), counter.size());
        size_t chunk = std::min<size_t>(32, cipher.size() - offset);
        for (size_t i = 0; i < chunk; ++i) {
            plain[offset + i] = static_cast<char>(cipher[offset + i] ^ stream[i]);
        }
    }

    ok = true;
    return plain;
}

}
