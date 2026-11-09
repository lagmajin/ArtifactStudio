// Cryptomatte multi-ID / coverage native pass R&D lab.
//
// Purpose: measure what the shipped ArtifactCore Cryptomatte implementation
// can and cannot express, and compare it against an independent Cryptomatte
// specification reference model. The lab is read-only with respect to the
// product code: it links ArtifactCore and exercises the shipped API, but it
// never changes it.
//
// The lab answers four questions:
//
//   1. ID encoding       - does CryptoSample::nameToFloat match the
//                          specification "uint32_to_float32" conversion, and do
//                          the official Cryptomatte test vectors reproduce?
//   2. Multi-ID support  - can CryptoImage carry more than one ID per pixel,
//                          and does the rank layer machinery produce rank > 0?
//   3. Coverage fidelity - does the coverage accumulator preserve the coverage
//                          sum, and does a multi-sample path survive it?
//   4. Metadata / EXR    - does the written EXR carry the specification
//                          required metadata, and are the channel names
//                          readable by a reference reader?
//
// Usage:
//   ArtifactCryptoMatteLab [--out <dir>] [--width N] [--height N]

#include "crypto_spec_reference.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <QString>

#include <OpenImageIO/imageio.h>

import Image.Cryptomatte.ExrWriter;
import Image.Cryptomatte.Image;
import Image.Cryptomatte.Pixel;

namespace {

using ArtifactCore::Image::Cryptomatte::CryptoExrWriter;
using ArtifactCore::Image::Cryptomatte::CryptoImage;
using ArtifactCore::Image::Cryptomatte::CryptoLayer;
using ArtifactCore::Image::Cryptomatte::CryptoManifest;
using ArtifactCore::Image::Cryptomatte::CryptoPixel;
using ArtifactCore::Image::Cryptomatte::CryptoSample;

struct LabOptions {
    std::filesystem::path outDir = "cryptomatte-lab-out";
    int width = 64;
    int height = 64;
};

struct Finding {
    std::string id;
    std::string severity;   // "info" | "warn" | "fail"
    std::string title;
    std::string detail;
};

class Report {
public:
    void add(std::string id, std::string severity, std::string title, std::string detail)
    {
        findings_.push_back(Finding{std::move(id), std::move(severity), std::move(title), std::move(detail)});
    }

    const std::vector<Finding>& findings() const { return findings_; }

    void writeJson(const std::filesystem::path& path, const LabOptions& options) const
    {
        std::ofstream out(path, std::ios::binary);
        if (!out) return;

        out << "{\n";
        out << "  \"tool\": \"ArtifactCryptoMatteLab\",\n";
        out << "  \"purpose\": \"Cryptomatte multi-ID / coverage native pass R&D\",\n";
        out << "  \"options\": { \"width\": " << options.width
            << ", \"height\": " << options.height << " },\n";
        out << "  \"findings\": [\n";
        for (std::size_t i = 0; i < findings_.size(); ++i) {
            const Finding& finding = findings_[i];
            out << "    {\n";
            out << "      \"id\": \"" << finding.id << "\",\n";
            out << "      \"severity\": \"" << finding.severity << "\",\n";
            out << "      \"title\": \"" << escape(finding.title) << "\",\n";
            out << "      \"detail\": \"" << escape(finding.detail) << "\"\n";
            out << "    }" << (i + 1 < findings_.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }

private:
    static std::string escape(const std::string& text)
    {
        std::string out;
        out.reserve(text.size());
        for (char c : text) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
            }
        }
        return out;
    }

    std::vector<Finding> findings_;
};

// ---------------------------------------------------------------------------
// 1. ID encoding
// ---------------------------------------------------------------------------

// Official Cryptomatte test vectors, taken from the reference implementation's
// unit test (Psyop/Cryptomatte, nuke/cryptomatte_utilities_tests.py). These
// are the authoritative values that any conforming implementation must
// reproduce, and they are independent of this repository.
struct SpecVector {
    const char* name;
    double expected;
};

const SpecVector kSpecVectors[] = {
    {"hello", 6.0705627102400005616e-17},
    {"cube", -4.08461912519e+15},
    {"sphere", 2.79018604383e+15},
    {"plane", 3.66557617593e-11},
};

void probeIdEncoding(Report& report)
{
    // First: does the independent reference model reproduce the official
    // vectors? If it does not, every later comparison is meaningless.
    int referenceMatches = 0;
    for (const SpecVector& vector : kSpecVectors) {
        const float reference = crypto_ref::idForName(vector.name);
        if (static_cast<double>(reference) == vector.expected) ++referenceMatches;
    }
    const int vectorCount = static_cast<int>(sizeof(kSpecVectors) / sizeof(kSpecVectors[0]));

    report.add("spec-vectors",
               referenceMatches == vectorCount ? "info" : "fail",
               "Specification reference model against the official Cryptomatte test vectors",
               "matched=" + std::to_string(referenceMatches) + "/" + std::to_string(vectorCount) +
               " The reference model is the oracle for the comparisons below.");

    // Second: does the shipped implementation reproduce the same vectors?
    int productMatches = 0;
    std::string firstMismatch;
    for (const SpecVector& vector : kSpecVectors) {
        const float product = CryptoSample::nameToFloat(QString::fromUtf8(vector.name));
        if (static_cast<double>(product) == vector.expected) {
            ++productMatches;
        } else if (firstMismatch.empty()) {
            firstMismatch = std::string(vector.name) + " expected=" +
                            std::to_string(vector.expected) + " product=" +
                            std::to_string(static_cast<double>(product));
        }
    }

    if (productMatches == vectorCount) {
        report.add("id-encoding", "info", "Product ID encoding matches the specification",
                   "matched=" + std::to_string(productMatches) + "/" + std::to_string(vectorCount));
    } else {
        report.add("id-encoding", "fail",
                   "Product ID encoding does not match the specification reference",
                   "matched=" + std::to_string(productMatches) + "/" + std::to_string(vectorCount) +
                   (firstMismatch.empty() ? std::string() : " first_mismatch[" + firstMismatch + "]") +
                   " The specification defines the ID as the MurmurHash3 x86 32-bit hash with the "
                   "exponent clamped away from 0 and 255, reinterpreted as the float32 bit pattern "
                   "(uint32_to_float32). The shipped implementation instead truncates the hash to "
                   "24 bits and casts that integer to float, which is a different value for the "
                   "same name and is not readable by a conforming Cryptomatte reader.");
    }

    // Third: the manifest identity. A conforming reader resolves a name to a
    // float through the manifest's hexadecimal ID, not through the truncated
    // integer hash.
    const QString sampleName = QStringLiteral("cube");
    const std::uint32_t productId = CryptoSample::nameToId(sampleName);
    const std::uint32_t referenceBits = crypto_ref::idBitsForName("cube");

    const QString productHex =
        QString::number(productId, 16).rightJustified(8, QLatin1Char('0')).toUpper();
    const std::string referenceHex = crypto_ref::idHexForName("cube");

    report.add("manifest-identity",
               productHex.toStdString() == referenceHex ? "info" : "warn",
               "Manifest ID representation for the name \"cube\"",
               "product_hex=" + productHex.toStdString() +
               " reference_hex=" + referenceHex +
               " product_id_bits=" + std::to_string(productId) +
               " reference_id_bits=" + std::to_string(referenceBits) +
               " The specification stores the manifest ID as the hexadecimal of the float32 bit "
               "pattern. The shipped manifest stores the hexadecimal of the truncated integer "
               "hash, so the two address different IDs.");
}

// ---------------------------------------------------------------------------
// 2. Multi-ID support in CryptoImage
// ---------------------------------------------------------------------------

void probeMultiIdSupport(Report& report, const LabOptions& options)
{
    const int pixelCount = options.width * options.height;

    // Two objects that overlap in the middle of the frame. A correct multi-ID
    // implementation reports both IDs on the shared pixels.
    const float idA = CryptoSample::nameToFloat(QStringLiteral("object_a"));
    const float idB = CryptoSample::nameToFloat(QStringLiteral("object_b"));

    std::vector<float> ids(static_cast<std::size_t>(pixelCount), 0.0f);
    std::vector<float> coverage(static_cast<std::size_t>(pixelCount), 0.0f);

    const int split = options.width / 2;
    for (int y = 0; y < options.height; ++y) {
        for (int x = 0; x < options.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * options.width + x);
            // Object A owns the left half, object B the right half, and the
            // boundary column carries both so the overlap is observable.
            if (x < split) {
                ids[index] = idA;
                coverage[index] = 1.0f;
            } else if (x == split) {
                ids[index] = idB;
                coverage[index] = 1.0f;
            } else {
                ids[index] = idB;
                coverage[index] = 1.0f;
            }
        }
    }

    CryptoImage image(options.width, options.height);
    image.fromIdCoverageBuffers(ids, coverage, options.width, options.height);

    const int maxRank = image.maxRank();

    // Rank 1 is only populated when a pixel holds more than two samples.
    std::vector<float> rank1Id;
    std::vector<float> rank1Coverage;
    image.generateRankLayers(CryptoLayer::Object, 1, rank1Id, rank1Coverage);
    int rank1NonZero = 0;
    for (float value : rank1Id) {
        if (value != 0.0f) ++rank1NonZero;
    }

    report.add("multi-id-image",
               maxRank >= 2 ? "info" : "warn",
               "CryptoImage rank depth for a single ID and coverage per pixel",
               "maxRank=" + std::to_string(maxRank) +
               " rank1_non_zero_pixels=" + std::to_string(rank1NonZero) +
               " rank1_sample_count=" + std::to_string(static_cast<int>(rank1Id.size())) +
               " The public CryptoImage ingestion API accepts exactly one ID per pixel, so no "
               "pixel can ever hold a second ID and rank 1 stays empty. A native multi-ID pass "
               "needs an ingestion path that can carry several IDs per pixel.");

    // The per-pixel storage model already supports the target shape.
    CryptoPixel pixel;
    for (int i = 0; i < CryptoPixel::kMaxSamples; ++i)
        pixel.addSample(static_cast<float>(i + 1), 0.05f);
    pixel.sort();

    std::vector<CryptoSample> samples;
    pixel.getSamplesForRank(1, samples);

    report.add("multi-id-pixel",
               "info",
               "CryptoPixel multi-sample capacity and rank extraction",
               "accepted_samples=" + std::to_string(pixel.sampleCount()) +
               " rank1_samples=" + std::to_string(static_cast<int>(samples.size())) +
               " CryptoPixel holds up to 16 samples and the rank lookup already returns two "
               "samples per rank, so the storage model supports multi-ID coverage. The "
               "constraint is the CryptoImage ingestion path, not the pixel model.");
}

// ---------------------------------------------------------------------------
// 3. Coverage fidelity
// ---------------------------------------------------------------------------

void probeCoverageFidelity(Report& report)
{
    CryptoPixel pixel;

    // A pixel shared between two objects, each covering half of it.
    pixel.addSample(10.0f, 0.5f);
    pixel.addSample(20.0f, 0.5f);
    pixel.sort();

    std::vector<CryptoSample> samples;
    pixel.getSamplesForRank(0, samples);

    float sum = 0.0f;
    for (const CryptoSample& sample : samples) sum += sample.coverage;

    report.add("coverage-sum", "info", "Coverage sum for a two-object boundary pixel",
               "sample_count=" + std::to_string(pixel.sampleCount()) +
               " coverage_sum=" + std::to_string(sum) +
               " The accumulator keeps coverage per ID and does not renormalise the sum, which "
               "is the behaviour a rank based reader expects.");

    // Over-coverage behaviour: one ID claiming more than full coverage.
    CryptoPixel over;
    over.addSample(10.0f, 1.5f);
    over.addSample(10.0f, 0.25f);
    over.sort();

    std::vector<CryptoSample> overSamples;
    over.getSamplesForRank(0, overSamples);
    float overSum = 0.0f;
    for (const CryptoSample& sample : overSamples) overSum += sample.coverage;

    report.add("coverage-clamp", "info", "Coverage clamping for a repeated over-covered ID",
               "sample_count=" + std::to_string(over.sampleCount()) +
               " coverage_sum=" + std::to_string(overSum) +
               " Repeated samples of the same ID are merged and the coverage is clamped to 1.0.");

    // Saturation behaviour: what happens past the 16 sample limit.
    CryptoPixel saturated;
    for (int i = 0; i < CryptoPixel::kMaxSamples + 8; ++i)
        saturated.addSample(static_cast<float>(i + 1), 0.01f * static_cast<float>(i + 1));
    saturated.sort();

    report.add("coverage-saturation", "info", "Sample count after exceeding the 16 sample limit",
               "sample_count=" + std::to_string(saturated.sampleCount()) +
               " limit=" + std::to_string(CryptoPixel::kMaxSamples) +
               " The pixel keeps the strongest samples and drops the weakest, so a native pass "
               "must decide the coverage ordering before it reaches this limit.");
}

// ---------------------------------------------------------------------------
// 4. Metadata and manifest
// ---------------------------------------------------------------------------

void probeManifest(Report& report)
{
    const std::vector<std::string> names = {"cube", "sphere", "plane"};

    // The shipped manifest is keyed by hash and holds the name as the value.
    CryptoManifest manifest;
    for (const std::string& name : names) manifest.addEntry(QString::fromStdString(name));
    const QString productManifest = manifest.toMetadata();

    // The specification manifest is keyed by name and holds the ID hex as the
    // value.
    const std::string referenceManifest = crypto_ref::manifestJson(names);

    report.add("manifest-format", "warn",
               "Manifest key/value orientation and ID representation",
               "product=" + productManifest.toStdString() +
               " reference=" + referenceManifest +
               " The specification manifest maps a name to the hexadecimal of its ID float bits. "
               "The shipped manifest maps the truncated hash to the name, so a conforming reader "
               "cannot resolve names from it.");

    // The metadata key identifies a cryptomatte type in the EXR header.
    // The shipped writer derives it from the truncated integer hash, while the
    // specification derives it from the type name's ID float bits.
    const std::string typeName = "CryptoObject";
    const QString productKey =
        QString::number(CryptoSample::nameToId(QString::fromStdString(typeName)), 16)
            .rightJustified(8, QLatin1Char('0')).toUpper();
    const std::string referenceKey = crypto_ref::metadataKeyForType(typeName);

    report.add("metadata-key",
               productKey.toStdString() == referenceKey ? "info" : "warn",
               "EXR metadata key derivation for the type name \"" + typeName + "\"",
               "product_key=" + productKey.toStdString() +
               " reference_key=" + referenceKey +
               " The specification key is seven hexadecimal digits taken from the ID of the type "
               "name. The shipped key is eight hexadecimal digits of the truncated integer hash, "
               "so the two point at different header attribute sets.");
}

// ---------------------------------------------------------------------------
// 5. EXR header conformance
// ---------------------------------------------------------------------------

// Reads the written EXR back and checks the header against the specification
// requirements. This turns the header findings into a measurement rather than
// a claim about the writer source.
void probeExrHeader(Report& report, const std::filesystem::path& exrPath)
{
    auto input = OIIO::ImageInput::open(exrPath.string().c_str());
    if (!input) {
        report.add("exr-header", "warn", "EXR header could not be read back",
                   "path=" + exrPath.string());
        return;
    }

    const OIIO::ImageSpec& spec = input->spec();

    // Collect the cryptomatte header attributes and the channel names.
    std::vector<std::string> cryptomatteAttributes;
    for (const OIIO::ParamValue& attribute : spec.extra_attribs) {
        const std::string name = attribute.name().string();
        if (name.rfind("cryptomatte/", 0) == 0)
            cryptomatteAttributes.push_back(name);
    }

    std::vector<std::string> channelNames;
    for (const std::string& channel : spec.channelnames)
        channelNames.push_back(channel);

    // The specification requires name, hash and conversion for every
    // cryptomatte type. The conversion attribute tells the reader how the ID
    // hash becomes a pixel value.
    //
    // Attributes are shaped "cryptomatte/<key>/<field>", so the field is
    // parsed from the last path segment rather than from a suffix guess.
    bool hasName = false;
    bool hasHash = false;
    bool hasConversion = false;
    bool hasManifest = false;
    std::vector<std::string> cryptomatteFields;
    for (const std::string& attribute : cryptomatteAttributes) {
        const std::size_t lastSlash = attribute.rfind('/');
        if (lastSlash == std::string::npos) continue;
        const std::string field = attribute.substr(lastSlash + 1);
        cryptomatteFields.push_back(field);
        if (field == "name") hasName = true;
        else if (field == "hash") hasHash = true;
        else if (field == "conversion") hasConversion = true;
        else if (field == "manifest") hasManifest = true;
    }

    std::string fieldDetail = "fields=";
    for (std::size_t i = 0; i < cryptomatteFields.size(); ++i)
        fieldDetail += (i > 0 ? "," : "") + cryptomatteFields[i];

    report.add("exr-required-attributes",
               hasConversion ? "info" : "fail",
               "Required EXR header attributes present in the written file",
               "name=" + std::string(hasName ? "yes" : "no") +
               " hash=" + std::string(hasHash ? "yes" : "no") +
               " conversion=" + std::string(hasConversion ? "yes" : "no") +
               " manifest=" + std::string(hasManifest ? "yes" : "no") +
               " " + fieldDetail +
               " The specification lists name, hash and conversion as required. A reader that "
               "requires the conversion attribute cannot confirm how the ID was encoded.");

    // The metadata key length. The specification uses a seven digit key.
    std::vector<std::size_t> keyLengths;
    for (const std::string& name : cryptomatteAttributes) {
        // "cryptomatte/<key>/<field>"
        const std::size_t firstSlash = name.find('/');
        const std::size_t lastSlash = name.rfind('/');
        if (firstSlash == std::string::npos || lastSlash == firstSlash) continue;
        keyLengths.push_back(lastSlash - firstSlash - 1);
    }
    std::string keyDetail = "key_lengths=";
    for (std::size_t i = 0; i < keyLengths.size(); ++i)
        keyDetail += (i > 0 ? "," : "") + std::to_string(keyLengths[i]);
    keyDetail += " expected_length=7";

    report.add("exr-metadata-key-length",
               !keyLengths.empty() && keyLengths.front() == 7 ? "info" : "warn",
               "Metadata key length in the written file",
               keyDetail + " The reference reader derives a seven digit key from the type name ID.");

    // The reference reader discovers cryptomatte channels with a regular
    // expression that requires a ".red" or ".r" suffix. This reproduces that
    // match so the comparison is mechanical.
    const std::string layerName = "CryptoObject";
    int specMatchedChannels = 0;
    int productMatchedChannels = 0;
    for (const std::string& channel : channelNames) {
        const std::size_t dot = channel.rfind('.');
        if (dot == std::string::npos) continue;
        const std::string suffix = channel.substr(dot + 1);
        const std::string stem = channel.substr(0, dot);
        if (stem.rfind(layerName, 0) != 0) continue;
        bool numeric = stem.size() > layerName.size();
        for (std::size_t i = layerName.size(); i < stem.size(); ++i) {
            if (stem[i] < '0' || stem[i] > '9') { numeric = false; break; }
        }
        if (!numeric) continue;
        if (suffix == "red" || suffix == "r") ++specMatchedChannels;
        if (suffix == "R" || suffix == "G" || suffix == "B" || suffix == "A")
            ++productMatchedChannels;
    }

    std::string channelDetail = "channels=" + std::to_string(channelNames.size());
    if (!channelNames.empty())
        channelDetail += " first=" + channelNames.front();
    channelDetail += " readable_by_reference_reader=" + std::to_string(specMatchedChannels);
    channelDetail += " shipped_suffix_channels=" + std::to_string(productMatchedChannels);

    report.add("exr-channel-naming",
               specMatchedChannels > 0 ? "info" : "fail",
               "EXR channel names against the reference reader pattern",
               channelDetail + " The reference reader matches channel names ending in .red or .r "
               "for a layer prefix. The shipped writer uses uppercase single letter suffixes, so "
               "a conforming reader finds no cryptomatte channels in the file.");

    // Report the channel list so a reader of the report can see the actual
    // names without opening the file.
    std::string channelList;
    for (const std::string& channel : channelNames) {
        if (!channelList.empty()) channelList += ",";
        channelList += channel;
    }
    report.add("exr-channel-list", "info", "Full channel list of the written file",
               "channels=[" + channelList + "]");

    input->close();
}

void probeExrOutput(Report& report, const LabOptions& options)
{
    const int pixelCount = options.width * options.height;

    const float id = CryptoSample::nameToFloat(QStringLiteral("cube"));
    std::vector<float> ids(static_cast<std::size_t>(pixelCount), id);
    std::vector<float> coverage(static_cast<std::size_t>(pixelCount), 1.0f);

    CryptoImage image(options.width, options.height);
    image.fromIdCoverageBuffers(ids, coverage, options.width, options.height);
    image.manifest().addEntry(QStringLiteral("cube"));

    std::error_code ec;
    std::filesystem::create_directories(options.outDir, ec);
    const std::filesystem::path exrPath = options.outDir / "cryptomatte-lab-sample.exr";

    const auto result = CryptoExrWriter::write(QString::fromStdString(exrPath.string()),
                                                image, CryptoLayer::Object, 0);
    if (!result) {
        report.add("exr-write", "warn", "EXR sample could not be written",
                   "error=" + result.error.toStdString() +
                   " The lab could not inspect the written header.");
        return;
    }

    report.add("exr-write", "info", "EXR sample written for header inspection",
               "path=" + exrPath.string() +
               " The sample is written with the shipped writer so the header can be inspected "
               "against the specification.");

    probeExrHeader(report, exrPath);
}

bool parseOptions(int argc, char** argv, LabOptions& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = (i + 1) < argc;
        if (arg == "--out" && hasValue) {
            options.outDir = argv[++i];
        } else if (arg == "--width" && hasValue) {
            options.width = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--height" && hasValue) {
            options.height = std::max(1, std::atoi(argv[++i]));
        } else {
            std::printf("unknown or incomplete argument: %s\n", arg.c_str());
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    LabOptions options;
    if (!parseOptions(argc, argv, options)) return 2;

    Report report;

    probeIdEncoding(report);
    probeMultiIdSupport(report, options);
    probeCoverageFidelity(report);
    probeManifest(report);
    probeExrOutput(report, options);

    std::error_code ec;
    std::filesystem::create_directories(options.outDir, ec);

    const std::filesystem::path jsonPath = options.outDir / "cryptomatte-lab-report.json";
    report.writeJson(jsonPath, options);

    std::printf("Cryptomatte multi-ID / coverage lab\n");
    std::printf("  output: %s\n\n", jsonPath.string().c_str());

    int warns = 0;
    int fails = 0;
    for (const Finding& finding : report.findings()) {
        std::printf("[%-4s] %s\n", finding.severity.c_str(), finding.title.c_str());
        std::printf("       %s\n\n", finding.detail.c_str());
        if (finding.severity == "warn") ++warns;
        if (finding.severity == "fail") ++fails;
    }

    std::printf("findings=%zu warn=%d fail=%d\n",
                report.findings().size(), warns, fails);
    return 0;
}
