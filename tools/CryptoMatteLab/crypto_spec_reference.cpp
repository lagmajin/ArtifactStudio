#include "crypto_spec_reference.h"

#include <cstring>

namespace crypto_ref {
namespace {

constexpr std::uint32_t kC1 = 0xcc9e2d51u;
constexpr std::uint32_t kC2 = 0x1b873593u;

} // namespace

std::uint32_t murmurHash3X8632(const void* data, std::size_t length, std::uint32_t seed)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t hash = seed;

    const std::size_t blocks = length / 4;
    for (std::size_t i = 0; i < blocks; ++i) {
        std::uint32_t k = 0;
        std::memcpy(&k, bytes + i * 4, sizeof(k));
        k *= kC1;
        k = (k << 15) | (k >> 17);
        k *= kC2;
        hash ^= k;
        hash = (hash << 13) | (hash >> 19);
        hash = hash * 5u + 0xe6546b64u;
    }

    const std::uint8_t* tail = bytes + blocks * 4;
    std::uint32_t k = 0;
    switch (length & 3u) {
    case 3: k ^= static_cast<std::uint32_t>(tail[2]) << 16; [[fallthrough]];
    case 2: k ^= static_cast<std::uint32_t>(tail[1]) << 8; [[fallthrough]];
    case 1:
        k ^= tail[0];
        k *= kC1;
        k = (k << 15) | (k >> 17);
        k *= kC2;
        hash ^= k;
        break;
    default: break;
    }

    hash ^= static_cast<std::uint32_t>(length);
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35bu;
    hash ^= hash >> 16;
    return hash;
}

std::uint32_t adjustExponent(std::uint32_t hashBits)
{
    const std::uint32_t exponent = (hashBits >> 23) & 0xffu;
    // A zero exponent yields denormals, an all-ones exponent yields NaN or
    // infinity. Toggling bit 23 moves the exponent to 1 or 254 respectively.
    if (exponent == 0u || exponent == 255u) hashBits ^= (1u << 23);
    return hashBits;
}

float hashBitsToFloat(std::uint32_t hashBits)
{
    float value = 0.0f;
    std::memcpy(&value, &hashBits, sizeof(value));
    return value;
}

std::uint32_t idBitsForName(const std::string& name)
{
    return adjustExponent(murmurHash3X8632(name.data(), name.size(), 0u));
}

float idForName(const std::string& name)
{
    return hashBitsToFloat(idBitsForName(name));
}

std::string idHexForName(const std::string& name)
{
    // The specification stores the manifest ID as the hexadecimal of the
    // float32 bit pattern, formatted as eight lowercase hexadecimal digits.
    const float value = idForName(name);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    static constexpr char kDigits[] = "0123456789abcdef";
    std::string hex(8, '0');
    for (int i = 0; i < 8; ++i)
        hex[static_cast<std::size_t>(i)] = kDigits[(bits >> ((7 - i) * 4)) & 0xfu];
    return hex;
}

std::string metadataKeyForType(const std::string& typeName)
{
    // The reference reader derives the metadata key by hashing the type name
    // and dropping the last hexadecimal digit of the resulting eight digit ID.
    std::string hex = idHexForName(typeName);
    if (hex.size() > 7) hex.resize(7);
    return hex;
}

std::string manifestJson(const std::vector<std::string>& names)
{
    // The specification defines the manifest as a JSON dictionary whose keys
    // are the object names and whose values are the ID hex strings.
    std::string json = "{";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + names[i] + "\":\"" + idHexForName(names[i]) + "\"";
    }
    json += "}";
    return json;
}

} // namespace crypto_ref
