// Cryptomatte specification reference model.
//
// This model is deliberately independent from the ArtifactCore implementation.
// The lab compares two separate definitions of the same contract instead of
// restating one of them, so a mismatch is evidence rather than a tautology.
//
// References (read 2026-11-10):
//   Cryptomatte Specification 1.2.0, Psyop
//   https://github.com/Psyop/Cryptomatte/blob/master/specification/cryptomatte_specification.pdf
//   Reference reader implementation, Pyop/Cryptomatte nuke/cryptomatte_utilities.py
//
// The specification fixes:
//   hash       = "MurmurHash3_32"
//   conversion = "uint32_to_float32"
// The conversion is a bit-for-bit copy of the 32-bit hash into a float, with the
// exponent clamped into [1:254] so no ID becomes denormal, infinite or NaN.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crypto_ref {

// MurmurHash3 x86 32-bit. Cryptomatte uses seed 0.
std::uint32_t murmurHash3X8632(const void* data, std::size_t length, std::uint32_t seed);

// Clamp the float exponent of a raw hash away from 0 and 255 by toggling bit 23.
std::uint32_t adjustExponent(std::uint32_t hashBits);

// "uint32_to_float32": reinterpret the adjusted hash bits as a float.
float hashBitsToFloat(std::uint32_t hashBits);

// Specification ID of a name, as a float value.
float idForName(const std::string& name);

// Specification ID bits of a name.
std::uint32_t idBitsForName(const std::string& name);

// Eight hexadecimal digits of the ID float bits, which is how a reference reader
// reconstructs an ID from a manifest value.
std::string idHexForName(const std::string& name);

// Seven hexadecimal digits: the metadata key of a cryptomatte type name.
// The reference reader derives it from the eight digit ID of the type name
// with the last digit removed.
std::string metadataKeyForType(const std::string& typeName);

// Specification manifest layout: object names as keys, ID hex as values.
std::string manifestJson(const std::vector<std::string>& names);

} // namespace crypto_ref
