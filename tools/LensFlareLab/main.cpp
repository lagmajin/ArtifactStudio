#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <complex>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Pixel {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels;
};

struct SourceSample {
    float u = 0.5f;
    float v = 0.5f;
    float energy = 0.0f;
    Pixel color{};
};

struct DetectedLight {
    float u = 0.5f;
    float v = 0.5f;
    float energy = 0.0f;
    Pixel color{};
    std::array<SourceSample, 9> samples{};
};

struct DetectedLightSet {
    std::array<DetectedLight, 8> lights{};
    std::size_t count = 0;
};

struct Settings {
    bool autoPosition = true;
    float positionX = 0.5f;
    float positionY = 0.5f;
    float threshold = 0.85f;
    float gain = 1.0f;
    float jacobianCap = 512.0f;
    int rayGrid = 64;
    int sourceSamples = 9;
    bool useCauchyDispersion = true;
    float apertureRadius = 4.545f;
    int apertureBlades = 6;
    bool antiReflectCoating = true;
    bool diffractionEnabled = true;
    bool reportGhostPaths = false;
    bool reportJacobianCap = false;
    bool reportSourceSamples = false;
    int selectedFirstReflection = -1;
    int selectedSecondReflection = -1;
};

constexpr std::size_t kLensSurfaceCount = 14;
constexpr std::size_t kGhostPathCount = kLensSurfaceCount * (kLensSurfaceCount - 1) / 2;

struct GhostPathDiagnostic {
    int firstReflection = -1;
    int secondReflection = -1;
    double luminance = 0.0;
    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
};

struct Vec3 {
    float x, y, z;
};

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 normalized(Vec3 a) { const float n = length(a); return n > 1e-12f ? a * (1.0f / n) : Vec3{0, 0, 1}; }

struct Surface {
    float radius;
    float z;
    float thickness;
    float refractiveIndexD;
    float abbe;
    float semiAperture;
};

struct LensPrescription {
    const char* name;
    float effectiveFocalLength;
    float sensorWidth;
    float irisZ;
    float irisRadius;
    std::array<Surface, 14> surfaces;
};

struct RayVertex {
    float x = 0.0f;
    float y = 0.0f;
    float energy = 0.0f;
    bool valid = false;
};

struct DiffractionKernel {
    int size = 0;
    float pixelScale = 0.0f;
    std::vector<float> intensity;
};

void fft1D(std::vector<std::complex<float>>& values) {
    const std::size_t size = values.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    constexpr float pi = 3.14159265358979323846f;
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const float angle = -2.0f * pi / static_cast<float>(length);
        const std::complex<float> root(std::cos(angle), std::sin(angle));
        for (std::size_t offset = 0; offset < size; offset += length) {
            std::complex<float> factor(1.0f, 0.0f);
            const std::size_t half = length >> 1;
            for (std::size_t i = 0; i < half; ++i) {
                const auto even = values[offset + i];
                const auto odd = values[offset + i + half] * factor;
                values[offset + i] = even + odd;
                values[offset + i + half] = even - odd;
                factor *= root;
            }
        }
    }
}

DiffractionKernel makeDiffractionKernel(int size, int blades, float wavelengthNm,
                                        const LensPrescription& lens, float apertureRadius,
                                        int imageWidth) {
    DiffractionKernel kernel;
    kernel.size = size;
    const float diameter = apertureRadius * 2.0f;
    kernel.pixelScale = wavelengthNm * 1e-6f * lens.effectiveFocalLength * imageWidth /
                       (diameter * lens.sensorWidth);
    std::vector<std::complex<float>> field(static_cast<std::size_t>(size) * size);
    const float sector = 2.0f * 3.14159265358979323846f / blades;
    std::size_t openSamples = 0;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const float px = ((x + 0.5f) / size * 2.0f - 1.0f) * apertureRadius;
        const float py = ((y + 0.5f) / size * 2.0f - 1.0f) * apertureRadius;
        const float angle = std::atan2(py, px);
        const float polygonRadius = apertureRadius * std::cos(0.5f * sector) /
                                    std::cos(std::remainder(angle, sector));
        if (std::hypot(px, py) <= polygonRadius) {
            field[static_cast<std::size_t>(y) * size + x] = {1.0f, 0.0f};
            ++openSamples;
        }
    }
    if (openSamples == 0) return kernel;
    std::vector<std::complex<float>> line(static_cast<std::size_t>(size));
    for (int y = 0; y < size; ++y) {
        const std::size_t offset = static_cast<std::size_t>(y) * size;
        std::copy_n(field.begin() + static_cast<std::ptrdiff_t>(offset), size, line.begin());
        fft1D(line);
        std::copy(line.begin(), line.end(), field.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) line[static_cast<std::size_t>(y)] = field[static_cast<std::size_t>(y) * size + x];
        fft1D(line);
        for (int y = 0; y < size; ++y) field[static_cast<std::size_t>(y) * size + x] = line[static_cast<std::size_t>(y)];
    }
    kernel.intensity.resize(field.size());
    const float normalization = 1.0f / static_cast<float>(openSamples * openSamples);
    float energySum = 0.0f;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const int shiftedX = (x + size / 2) % size;
        const int shiftedY = (y + size / 2) % size;
        const float intensity = std::norm(field[static_cast<std::size_t>(y) * size + x]) * normalization;
        kernel.intensity[static_cast<std::size_t>(shiftedY) * size + shiftedX] = intensity;
        energySum += intensity;
    }
    if (energySum > 1e-12f) for (float& value : kernel.intensity) value /= energySum;
    return kernel;
}

LensPrescription makeAngenieuxPrescription() {
    LensPrescription lens{"Angenieux Double-Gauss 100 mm", 100.0f, 53.96f, 0.0f, 4.545f, {{
        {164.13f, 0,       10.99f, 1.6751f, 32.2f, 27.0f},
        {559.20f, 0,        0.23f, 1.0000f, 0.0f,  27.0f},
        {100.12f, 0,       11.45f, 1.6689f, 47.1f, 25.5f},
        {213.54f, 0,        0.23f, 1.0000f, 0.0f,  25.5f},
        {58.04f,  0,       22.95f, 1.6913f, 54.7f, 20.5f},
        {2551.10f,0,        2.58f, 1.6751f, 32.2f, 20.5f},
        {32.39f,  0,       15.66f, 1.0000f, 0.0f,  13.5f},
        {-40.42f, 0,        2.74f, 1.6992f, 30.1f, 12.5f},
        {192.98f, 0,       27.92f, 1.6204f, 60.3f, 18.0f},
        {-55.53f, 0,        0.23f, 1.0000f, 0.0f,  18.0f},
        {192.98f, 0,        7.98f, 1.6913f, 54.7f, 17.5f},
        {-225.30f,0,        0.23f, 1.0000f, 0.0f,  17.5f},
        {175.08f, 0,        8.48f, 1.6913f, 54.7f, 17.5f},
        {-203.55f,0,       55.742f,1.0000f, 0.0f,  17.5f},
    }}};
    lens.surfaces[0].z = 0.0f;
    for (std::size_t i = 1; i < lens.surfaces.size(); ++i) {
        lens.surfaces[i].z = lens.surfaces[i - 1].z + lens.surfaces[i - 1].thickness;
        if (i == 7) lens.surfaces[i].z += 15.0f; // air gap from the stop to the rear group
    }
    lens.irisZ = lens.surfaces[6].z + lens.surfaces[6].thickness;
    // The cited prescription labels the stop as f/11; derive the clear radius
    // from the stated 100 mm focal length rather than the table's mechanical height.
    lens.irisRadius = lens.effectiveFocalLength / (2.0f * 11.0f);
    return lens;
}

float refractiveIndex(const Surface& surface, float wavelengthNm, bool useCauchyDispersion) {
    if (surface.abbe <= 0.0f) return 1.0f;
    if (useCauchyDispersion) {
        const float slope = ((surface.refractiveIndexD - 1.0f) / surface.abbe) * 0.52345f;
        const float offset = surface.refractiveIndexD - slope * 2.897f;
        const float wavelengthMicrometers = wavelengthNm * 1e-3f;
        return offset + slope / (wavelengthMicrometers * wavelengthMicrometers);
    }
    constexpr float dLine = 587.56f;
    const float slope = (surface.refractiveIndexD - 1.0f) / surface.abbe;
    return surface.refractiveIndexD + slope * (dLine / wavelengthNm - 1.0f);
}

Vec3 surfaceNormal(const Surface& surface, Vec3 point) {
    if (std::abs(surface.radius) > 1e6f) return {0, 0, -1};
    const Vec3 center{0, 0, surface.z + surface.radius};
    return normalized(point - center);
}

bool intersectSurface(Vec3& origin, Vec3 direction, const Surface& surface) {
    float t = std::numeric_limits<float>::infinity();
    if (std::abs(surface.radius) > 1e6f) {
        if (std::abs(direction.z) < 1e-7f) return false;
        t = (surface.z - origin.z) / direction.z;
    } else {
        const Vec3 center{0, 0, surface.z + surface.radius};
        const Vec3 oc = origin - center;
        const float b = dot(oc, direction);
        const float c = dot(oc, oc) - surface.radius * surface.radius;
        const float discriminant = b * b - c;
        if (discriminant < 0.0f) return false;
        const float root = std::sqrt(discriminant);
        const float t0 = -b - root;
        const float t1 = -b + root;
        if (t0 > 1e-5f) t = t0;
        else if (t1 > 1e-5f) t = t1;
        else return false;
    }
    if (!(t > 1e-5f) || !std::isfinite(t)) return false;
    origin = origin + direction * t;
    return std::hypot(origin.x, origin.y) <= surface.semiAperture;
}

float fresnelReflectance(float n1, float n2, Vec3 direction, Vec3 normal,
                         float wavelengthNm, bool antiReflectCoating) {
    if (dot(direction, normal) > 0.0f) normal = normal * -1.0f;
    const float cos1 = std::clamp(-dot(normal, direction), 0.0f, 1.0f);
    const float sin2sq = (n1 / n2) * (n1 / n2) * std::max(0.0f, 1.0f - cos1 * cos1);
    if (sin2sq >= 1.0f) return 1.0f;
    const float cos2 = std::sqrt(1.0f - sin2sq);
    if (antiReflectCoating && std::abs(n1 - n2) > 1e-4f) {
        constexpr double coatingIndex = 1.38;
        constexpr double designWavelengthNm = 550.0;
        const double n0 = n1, n2d = n2, ci = cos1, ct = cos2;
        const double coatingSin2 = (n0 / coatingIndex) * (n0 / coatingIndex) * (1.0 - ci * ci);
        if (coatingSin2 >= 1.0) return 1.0f;
        const double cc = std::sqrt(1.0 - coatingSin2);
        const double thickness = designWavelengthNm / (4.0 * coatingIndex);
        const double phaseAngle = 4.0 * 3.14159265358979323846 *
                                  coatingIndex * thickness * cc / wavelengthNm;
        const std::complex<double> phase = std::polar(1.0, phaseAngle);
        const auto rs = [](double na, double nb, double ca, double cb) {
            return (na * ca - nb * cb) / (na * ca + nb * cb);
        };
        const auto rp = [](double na, double nb, double ca, double cb) {
            return (nb * ca - na * cb) / (nb * ca + na * cb);
        };
        const double rs01 = rs(n0, coatingIndex, ci, cc);
        const double rs12 = rs(coatingIndex, n2d, cc, ct);
        const double rp01 = rp(n0, coatingIndex, ci, cc);
        const double rp12 = rp(coatingIndex, n2d, cc, ct);
        const std::complex<double> reflectedS = (rs01 + rs12 * phase) /
            (1.0 + rs01 * rs12 * phase);
        const std::complex<double> reflectedP = (rp01 + rp12 * phase) /
            (1.0 + rp01 * rp12 * phase);
        return static_cast<float>(std::clamp(
            0.5 * (std::norm(reflectedS) + std::norm(reflectedP)), 0.0, 1.0));
    }
    const float ds = n1 * cos1 + n2 * cos2;
    const float dp = n1 * cos2 + n2 * cos1;
    if (std::abs(ds) < 1e-8f || std::abs(dp) < 1e-8f) return 1.0f;
    const float rs = (n1 * cos1 - n2 * cos2) / ds;
    const float rp = (n1 * cos2 - n2 * cos1) / dp;
    return std::clamp(0.5f * (rs * rs + rp * rp), 0.0f, 1.0f);
}

bool refractRay(Vec3& direction, Vec3 normal, float n1, float n2) {
    if (dot(direction, normal) > 0.0f) normal = normal * -1.0f;
    const float cos1 = std::clamp(-dot(normal, direction), 0.0f, 1.0f);
    const float eta = n1 / n2;
    const float k = 1.0f - eta * eta * (1.0f - cos1 * cos1);
    if (k < 0.0f) return false;
    direction = normalized(direction * eta + normal * (eta * cos1 - std::sqrt(k)));
    return direction.z > 1e-6f || direction.z < -1e-6f;
}

bool traceEntrancePupilRay(const LensPrescription& lens, float wavelengthNm,
                           float stopX, float stopY, Vec3 incidentDirection,
                           Vec3& entranceOrigin, bool useCauchyDispersion) {
    // Trace the ray backwards from the physical stop through the front group.
    // Sampling the entrance pupil this way avoids spending most of a coarse
    // ray grid on rays that the f/11 stop immediately rejects.
    Vec3 origin{stopX, stopY, lens.irisZ};
    Vec3 direction = incidentDirection * -1.0f;
    for (int i = 6; i >= 0; --i) {
        const Surface& surface = lens.surfaces[static_cast<std::size_t>(i)];
        if (!intersectSurface(origin, direction, surface)) return false;
        const float forwardBefore = i == 0 ? 1.0f :
            refractiveIndex(lens.surfaces[static_cast<std::size_t>(i - 1)], wavelengthNm,
                            useCauchyDispersion);
        const float forwardAfter = refractiveIndex(surface, wavelengthNm, useCauchyDispersion);
        if (!refractRay(direction, surfaceNormal(surface, origin), forwardAfter, forwardBefore))
            return false;
    }
    const Vec3 forwardDirection = direction * -1.0f;
    if (forwardDirection.z <= 1e-6f) return false;
    entranceOrigin = origin - forwardDirection * 0.05f;
    return true;
}

void reflectRay(Vec3& direction, Vec3 normal) {
    direction = normalized(direction - normal * (2.0f * dot(direction, normal)));
}

bool clipIris(Vec3& origin, Vec3 direction, const LensPrescription& lens,
              int bladeCount) {
    if (std::abs(direction.z) < 1e-7f) return false;
    const float t = (lens.irisZ - origin.z) / direction.z;
    if (!(t > 1e-5f)) return false;
    origin = origin + direction * t;
    const float angle = std::atan2(origin.y, origin.x);
    const float step = 2.0f * 3.14159265358979323846f / std::max(3, bladeCount);
    const float sector = std::remainder(angle, step);
    const float polygonRadius = lens.irisRadius * std::cos(3.14159265358979323846f / bladeCount) /
        std::cos(sector);
    return std::hypot(origin.x, origin.y) <= polygonRadius;
}

bool traceGhostRay(const LensPrescription& lens, int firstReflection, int secondReflection,
                   float wavelengthNm, Vec3 origin, Vec3 direction, float& energy,
                   Vec3& sensorPoint, int bladeCount, bool antiReflectCoating,
                   bool useCauchyDispersion) {
    energy = 1.0f;
    for (int i = 0; i <= firstReflection; ++i) {
        if (i == 7 && !clipIris(origin, direction, lens, bladeCount)) return false;
        const Surface& surface = lens.surfaces[static_cast<std::size_t>(i)];
        if (!intersectSurface(origin, direction, surface)) return false;
        const Vec3 normal = surfaceNormal(surface, origin);
        const float nBefore = i == 0 ? 1.0f :
            refractiveIndex(lens.surfaces[static_cast<std::size_t>(i - 1)], wavelengthNm,
                            useCauchyDispersion);
        const float nAfter = refractiveIndex(surface, wavelengthNm, useCauchyDispersion);
        if (i == firstReflection) {
            const float r = fresnelReflectance(nBefore, nAfter, direction, normal,
                                                wavelengthNm, antiReflectCoating);
            if (r <= 1e-9f) return false;
            energy *= r;
            reflectRay(direction, normal);
        } else {
            const float r = fresnelReflectance(nBefore, nAfter, direction, normal,
                                                wavelengthNm, antiReflectCoating);
            if (!refractRay(direction, normal, nBefore, nAfter)) return false;
            energy *= 1.0f - r;
        }
    }

    for (int i = firstReflection - 1; i >= secondReflection; --i) {
        if (i == 6 && !clipIris(origin, direction, lens, bladeCount)) return false;
        const Surface& surface = lens.surfaces[static_cast<std::size_t>(i)];
        if (!intersectSurface(origin, direction, surface)) return false;
        const Vec3 normal = surfaceNormal(surface, origin);
        const float nFrom = refractiveIndex(surface, wavelengthNm, useCauchyDispersion);
        const float nTo = i == 0 ? 1.0f :
            refractiveIndex(lens.surfaces[static_cast<std::size_t>(i - 1)], wavelengthNm,
                            useCauchyDispersion);
        if (i == secondReflection) {
            const float r = fresnelReflectance(nFrom, nTo, direction, normal,
                                                wavelengthNm, antiReflectCoating);
            if (r <= 1e-9f) return false;
            energy *= r;
            reflectRay(direction, normal);
        } else {
            const float r = fresnelReflectance(nFrom, nTo, direction, normal,
                                                wavelengthNm, antiReflectCoating);
            if (!refractRay(direction, normal, nFrom, nTo)) return false;
            energy *= 1.0f - r;
        }
    }

    for (int i = secondReflection + 1; i < static_cast<int>(lens.surfaces.size()); ++i) {
        if (i == 7 && !clipIris(origin, direction, lens, bladeCount)) return false;
        const Surface& surface = lens.surfaces[static_cast<std::size_t>(i)];
        if (!intersectSurface(origin, direction, surface)) return false;
        const Vec3 normal = surfaceNormal(surface, origin);
        const float nBefore = i == 0 ? 1.0f :
            refractiveIndex(lens.surfaces[static_cast<std::size_t>(i - 1)], wavelengthNm,
                            useCauchyDispersion);
        const float nAfter = refractiveIndex(surface, wavelengthNm, useCauchyDispersion);
        const float r = fresnelReflectance(nBefore, nAfter, direction, normal,
                                            wavelengthNm, antiReflectCoating);
        if (!refractRay(direction, normal, nBefore, nAfter)) return false;
        energy *= 1.0f - r;
    }
    const float sensorZ = lens.surfaces.back().z + lens.surfaces.back().thickness;
    if (direction.z <= 1e-6f) return false;
    const float t = (sensorZ - origin.z) / direction.z;
    if (!(t > 0.0f)) return false;
    sensorPoint = origin + direction * t;
    return std::isfinite(sensorPoint.x) && std::isfinite(sensorPoint.y) && energy > 1e-10f;
}

std::string readToken(std::istream& input) {
    std::string token;
    while (input >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string discard;
            std::getline(input, discard);
            continue;
        }
        return token;
    }
    throw std::runtime_error("unexpected end of image header");
}

std::size_t checkedPixelCount(int width, int height) {
    if (width <= 0 || height <= 0 ||
        static_cast<std::size_t>(width) >
            std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height)) {
        throw std::runtime_error("invalid image dimensions");
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

Image loadImage(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input: " + path);
    const std::string magic = readToken(input);
    Image image;
    image.width = std::stoi(readToken(input));
    image.height = std::stoi(readToken(input));
    const std::size_t count = checkedPixelCount(image.width, image.height);
    image.pixels.resize(count);

    if (magic == "P6") {
        const int maxValue = std::stoi(readToken(input));
        if (maxValue <= 0 || maxValue > 255) throw std::runtime_error("PPM max value must be 1..255");
        input.get();
        std::vector<unsigned char> bytes(count * 3u);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) throw std::runtime_error("truncated PPM pixels");
        const float scale = 1.0f / static_cast<float>(maxValue);
        for (std::size_t i = 0; i < count; ++i) {
            image.pixels[i] = {bytes[i * 3u] * scale, bytes[i * 3u + 1] * scale,
                               bytes[i * 3u + 2] * scale};
        }
    } else if (magic == "PF") {
        const float scale = std::stof(readToken(input));
        if (scale == 0.0f || !std::isfinite(scale)) throw std::runtime_error("invalid PFM scale");
        input.get();
        std::vector<float> values(count * 3u);
        input.read(reinterpret_cast<char*>(values.data()),
                   static_cast<std::streamsize>(values.size() * sizeof(float)));
        if (!input) throw std::runtime_error("truncated PFM pixels");
        const bool fileLittleEndian = scale < 0.0f;
        const std::uint16_t endianProbe = 1;
        const bool hostLittleEndian = *reinterpret_cast<const unsigned char*>(&endianProbe) == 1;
        const float magnitude = std::abs(scale);
        for (int fileY = 0; fileY < image.height; ++fileY) {
            const int y = image.height - 1 - fileY;
            for (int x = 0; x < image.width; ++x) {
                const std::size_t fileIndex = (static_cast<std::size_t>(fileY) * image.width + x) * 3u;
                std::array<float, 3> rgb{values[fileIndex], values[fileIndex + 1], values[fileIndex + 2]};
                if (fileLittleEndian != hostLittleEndian) {
                    for (float& value : rgb) {
                        std::uint32_t bits;
                        std::memcpy(&bits, &value, sizeof(bits));
                        bits = ((bits & 0x000000ffu) << 24u) | ((bits & 0x0000ff00u) << 8u) |
                               ((bits & 0x00ff0000u) >> 8u) | ((bits & 0xff000000u) >> 24u);
                        std::memcpy(&value, &bits, sizeof(bits));
                    }
                }
                image.pixels[static_cast<std::size_t>(y) * image.width + x] =
                    {rgb[0] * magnitude, rgb[1] * magnitude, rgb[2] * magnitude};
            }
        }
    } else {
        throw std::runtime_error("input must be binary PPM (P6) or RGB PFM (PF)");
    }
    return image;
}

float finiteClamp(float value, float minimum, float maximum) {
    return std::clamp(std::isfinite(value) ? value : minimum, minimum, maximum);
}

DetectedLight estimateLight(const Image& image, int centerX, int centerY, float threshold) {
    double totalFlux = 0.0;
    double red = 0.0, green = 0.0, blue = 0.0;
    std::array<double, 9> flux{};
    std::array<double, 9> weightedX{}, weightedY{};
    std::array<double, 9> sampleRed{}, sampleGreen{}, sampleBlue{};
    for (int y = std::max(0, centerY - 24); y <= std::min(image.height - 1, centerY + 24); ++y) {
        for (int x = std::max(0, centerX - 24); x <= std::min(image.width - 1, centerX + 24); ++x) {
            const float dx = static_cast<float>(x - centerX);
            const float dy = static_cast<float>(y - centerY);
            if (dx * dx + dy * dy > 24.0f * 24.0f) continue;
            const Pixel& pixel = image.pixels[static_cast<std::size_t>(y) * image.width + x];
            const float luminance = pixel.r * 0.2126f + pixel.g * 0.7152f + pixel.b * 0.0722f;
            const float excess = std::max(0.0f, luminance - threshold);
            if (excess <= 0.0f || luminance <= 1e-8f) continue;
            const double chromaWeight = static_cast<double>(excess / luminance);
            const int binX = std::clamp((x - (centerX - 24)) / 16, 0, 2);
            const int binY = std::clamp((y - (centerY - 24)) / 16, 0, 2);
            const std::size_t bin = static_cast<std::size_t>(binY * 3 + binX);
            totalFlux += excess;
            red += pixel.r * chromaWeight;
            green += pixel.g * chromaWeight;
            blue += pixel.b * chromaWeight;
            flux[bin] += excess;
            weightedX[bin] += static_cast<double>(x) * excess;
            weightedY[bin] += static_cast<double>(y) * excess;
            sampleRed[bin] += pixel.r * chromaWeight;
            sampleGreen[bin] += pixel.g * chromaWeight;
            sampleBlue[bin] += pixel.b * chromaWeight;
        }
    }
    DetectedLight light;
    light.energy = static_cast<float>(totalFlux);
    if (totalFlux > 1e-8) {
        // Keep chromaticity separate from integrated source flux so intensity
        // enters the optical energy calculation exactly once.
        light.color = {static_cast<float>(red / totalFlux),
                       static_cast<float>(green / totalFlux),
                       static_cast<float>(blue / totalFlux)};
        for (std::size_t i = 0; i < light.samples.size(); ++i) {
            SourceSample& sample = light.samples[i];
            sample.energy = static_cast<float>(flux[i]);
            if (flux[i] <= 1e-8) continue;
            sample.u = static_cast<float>(weightedX[i] / flux[i]) / std::max(1, image.width - 1);
            sample.v = static_cast<float>(weightedY[i] / flux[i]) / std::max(1, image.height - 1);
            sample.color = {static_cast<float>(sampleRed[i] / flux[i]),
                            static_cast<float>(sampleGreen[i] / flux[i]),
                            static_cast<float>(sampleBlue[i] / flux[i])};
        }
    }
    return light;
}

Image render(const Image& source, const Settings& settings) {
    Image output = source;
    const int width = source.width;
    const int height = source.height;
    LensPrescription lens = makeAngenieuxPrescription();
    lens.irisRadius = finiteClamp(settings.apertureRadius, 1.0f, 12.75f);
    DetectedLightSet lights;
    if (settings.autoPosition) {
        constexpr int suppressionRadius = 28;
        while (lights.count < lights.lights.size()) {
            float peak = settings.threshold;
            int peakX = 0, peakY = 0;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                bool suppressed = false;
                for (std::size_t i = 0; i < lights.count; ++i) {
                    const float dx = static_cast<float>(x) - lights.lights[i].u * (width - 1);
                    const float dy = static_cast<float>(y) - lights.lights[i].v * (height - 1);
                    if (dx * dx + dy * dy < suppressionRadius * suppressionRadius) {
                        suppressed = true;
                        break;
                    }
                }
                if (suppressed) continue;
                const Pixel& p = source.pixels[static_cast<std::size_t>(y) * width + x];
                const float luminance = p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f;
                if (luminance > peak) { peak = luminance; peakX = x; peakY = y; }
            }
            if (peak <= settings.threshold) break;
            const float coreThreshold = std::max(settings.threshold, peak * 0.65f);
            double weightedX = 0.0, weightedY = 0.0, weightSum = 0.0;
            for (int y = std::max(0, peakY - 24); y <= std::min(height - 1, peakY + 24); ++y) {
                for (int x = std::max(0, peakX - 24); x <= std::min(width - 1, peakX + 24); ++x) {
                    const Pixel& p = source.pixels[static_cast<std::size_t>(y) * width + x];
                    const float luminance = p.r * 0.2126f + p.g * 0.7152f + p.b * 0.0722f;
                    const float weight = std::max(0.0f, luminance - coreThreshold);
                    weightedX += static_cast<double>(x) * weight;
                    weightedY += static_cast<double>(y) * weight;
                    weightSum += weight;
                }
            }
            const int centerX = std::clamp(static_cast<int>(std::lround(
                weightSum > 0.000001 ? weightedX / weightSum : peakX)), 0, width - 1);
            const int centerY = std::clamp(static_cast<int>(std::lround(
                weightSum > 0.000001 ? weightedY / weightSum : peakY)), 0, height - 1);
            DetectedLight light = estimateLight(source, centerX, centerY, settings.threshold);
            light.u = static_cast<float>(centerX) / std::max(1, width - 1);
            light.v = static_cast<float>(centerY) / std::max(1, height - 1);
            if (light.energy > 0.0f) lights.lights[lights.count++] = light;
        }
    } else {
        DetectedLight light;
        light.u = settings.positionX;
        light.v = settings.positionY;
        const int sx = std::clamp(static_cast<int>(std::lround(light.u * (width - 1))), 0, width - 1);
        const int sy = std::clamp(static_cast<int>(std::lround(light.v * (height - 1))), 0, height - 1);
        light = estimateLight(source, sx, sy, settings.threshold);
        const float shiftU = settings.positionX - static_cast<float>(sx) / std::max(1, width - 1);
        const float shiftV = settings.positionY - static_cast<float>(sy) / std::max(1, height - 1);
        for (SourceSample& sample : light.samples) {
            if (sample.energy <= 0.0f) continue;
            sample.u += shiftU;
            sample.v += shiftV;
        }
        light.u = settings.positionX;
        light.v = settings.positionY;
        if (light.energy > 0.0f) lights.lights[lights.count++] = light;
    }
    if (settings.reportSourceSamples) {
        for (std::size_t i = 0; i < lights.count; ++i) {
            const DetectedLight& light = lights.lights[i];
            float sampledFlux = 0.0f;
            std::size_t activeSamples = 0;
            for (const SourceSample& sample : light.samples) {
                sampledFlux += sample.energy;
                if (sample.energy > 0.0f) ++activeSamples;
            }
            std::cout << "Source (" << std::fixed << std::setprecision(3) << light.u << ',' << light.v
                      << ") integratedFlux=" << std::setprecision(6) << light.energy
                      << " sampledFlux=" << sampledFlux << " activeBins=" << activeSamples << "/9\n";
        }
    }
    if (lights.count == 0) return output;

    const int grid = std::clamp(settings.rayGrid, 8, 192);
    const int rayCount = grid * grid;
    const float sensorHeight = lens.sensorWidth * static_cast<float>(height) / width;
    constexpr std::array<float, 3> wavelengths{610.0f, 550.0f, 460.0f};
    std::uint64_t rasterizedTriangles = 0;
    std::uint64_t jacobianClampedTriangles = 0;
    std::array<DiffractionKernel, 3> diffraction;
    if (settings.diffractionEnabled) {
        for (std::size_t band = 0; band < diffraction.size(); ++band) {
            diffraction[band] = makeDiffractionKernel(256, settings.apertureBlades,
                wavelengths[band], lens, lens.irisRadius, width);
        }
    }
    for (std::size_t lightIndex = 0; lightIndex < lights.count; ++lightIndex) {
        const DetectedLight& light = lights.lights[lightIndex];
        std::array<GhostPathDiagnostic, kGhostPathCount> pathDiagnostics{};
        for (int first = 1; first < static_cast<int>(kLensSurfaceCount); ++first) {
            for (int second = 0; second < first; ++second) {
                const std::size_t index = static_cast<std::size_t>(first * (first - 1) / 2 + second);
                pathDiagnostics[index].firstReflection = first;
                pathDiagnostics[index].secondReflection = second;
            }
        }
        std::size_t consideredPathCount = 0;
        for (int first = 1; first < static_cast<int>(lens.surfaces.size()); ++first) {
            for (int second = 0; second < first; ++second) {
                if (first >= 7 && second <= 6) continue;
                if (settings.selectedFirstReflection >= 0 &&
                    (first != settings.selectedFirstReflection || second != settings.selectedSecondReflection)) continue;
                ++consideredPathCount;
            }
        }
        for (std::size_t sampleIndex = 0; sampleIndex <
             static_cast<std::size_t>(settings.sourceSamples == 1 ? 1 : 9); ++sampleIndex) {
            SourceSample sample;
            if (settings.sourceSamples == 1) {
                sample = {light.u, light.v, light.energy, light.color};
            } else {
                sample = light.samples[sampleIndex];
                if (sample.energy <= 0.0f) continue;
            }
            const float sourceU = sample.u;
            const float sourceV = sample.v;
            const float sourceEnergy = sample.energy;
            const Pixel sourceColor = sample.color;
            const float thetaX = std::atan((sourceU - 0.5f) * lens.sensorWidth / lens.effectiveFocalLength);
            const float thetaY = std::atan((0.5f - sourceV) * lens.sensorWidth *
                                           static_cast<float>(height) / (width * lens.effectiveFocalLength));
            const Vec3 direction = normalized({std::sin(thetaX), std::sin(thetaY),
                                                std::cos(thetaX) * std::cos(thetaY)});
            const float rayGain = settings.gain * sourceEnergy / rayCount;
            const std::array<float, 3> sourceBands{sourceColor.r, sourceColor.g, sourceColor.b};

            for (int firstReflection = 1; firstReflection < static_cast<int>(lens.surfaces.size()); ++firstReflection) {
                for (int secondReflection = 0; secondReflection < firstReflection; ++secondReflection) {
                    // A pair straddling the stop sends the ray through the iris on
                    // the way in, back through it after the first reflection, and
                    // through it a third time on the way to the sensor. The paper's
                    // 42-path Angenieux setup omits these heavily blocked paths and
                    // retains only paths with a single aperture crossing.
                    if (firstReflection >= 7 && secondReflection <= 6) continue;
                    if (settings.selectedFirstReflection >= 0 &&
                        (firstReflection != settings.selectedFirstReflection ||
                         secondReflection != settings.selectedSecondReflection)) continue;
                    const std::size_t pathIndex = static_cast<std::size_t>(
                        firstReflection * (firstReflection - 1) / 2 + secondReflection);
                    GhostPathDiagnostic& pathDiagnostic = pathDiagnostics[pathIndex];
                    for (std::size_t band = 0; band < wavelengths.size(); ++band) {
                        if (sourceBands[band] <= 1e-6f) continue;
                        std::vector<RayVertex> rayGridData(static_cast<std::size_t>(grid) * grid);
                        for (int gy = 0; gy < grid; ++gy) for (int gx = 0; gx < grid; ++gx) {
                            const float stopX = ((gx + 0.5f) / grid * 2.0f - 1.0f) * lens.irisRadius;
                            const float stopY = ((gy + 0.5f) / grid * 2.0f - 1.0f) * lens.irisRadius;
                            const float apertureStep = 2.0f * 3.14159265358979323846f /
                                                       settings.apertureBlades;
                            const float apertureSector = std::remainder(std::atan2(stopY, stopX), apertureStep);
                            const float aperturePolygonRadius = lens.irisRadius *
                                std::cos(3.14159265358979323846f / settings.apertureBlades) /
                                std::cos(apertureSector);
                            if (std::hypot(stopX, stopY) > aperturePolygonRadius) continue;
                            Vec3 origin{};
                            if (!traceEntrancePupilRay(lens, wavelengths[band], stopX, stopY,
                                                       direction, origin, settings.useCauchyDispersion)) continue;
                            float energy = 0.0f;
                            Vec3 sensorPoint{};
                            if (!traceGhostRay(lens, firstReflection, secondReflection,
                                               wavelengths[band], origin, direction, energy,
                                       sensorPoint, settings.apertureBlades,
                                       settings.antiReflectCoating,
                                       settings.useCauchyDispersion)) continue;
                            const float screenX = width * 0.5f + sensorPoint.x / lens.sensorWidth * width;
                            const float screenY = height * 0.5f - sensorPoint.y / sensorHeight * height;
                            RayVertex& vertex = rayGridData[static_cast<std::size_t>(gy) * grid + gx];
                            vertex = {screenX, screenY, energy * sourceBands[band], true};
                        }

                        auto rasterizeTriangle = [&](const RayVertex& a, const RayVertex& b,
                                                     const RayVertex& c) {
                            const float area = (b.x - a.x) * (c.y - a.y) -
                                               (b.y - a.y) * (c.x - a.x);
                            if (std::abs(area) < 1e-5f) return;
                            ++rasterizedTriangles;
                            const float rawIrradiance = 1.0f / std::abs(area);
                            if (rawIrradiance > settings.jacobianCap) ++jacobianClampedTriangles;
                            const int left = std::max(0, static_cast<int>(std::floor(
                                std::min({a.x, b.x, c.x}))));
                            const int right = std::min(width - 1, static_cast<int>(std::ceil(
                                std::max({a.x, b.x, c.x}))));
                            const int top = std::max(0, static_cast<int>(std::floor(
                                std::min({a.y, b.y, c.y}))));
                            const int bottom = std::min(height - 1, static_cast<int>(std::ceil(
                                std::max({a.y, b.y, c.y}))));
                            const float inverseArea = 1.0f / area;
                            const float irradiance = std::min(rawIrradiance, settings.jacobianCap);
                            for (int y = top; y <= bottom; ++y) for (int x = left; x <= right; ++x) {
                                const float px = x + 0.5f, py = y + 0.5f;
                                const float wa = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * inverseArea;
                                const float wb = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * inverseArea;
                                const float wc = 1.0f - wa - wb;
                                if (wa < -1e-4f || wb < -1e-4f || wc < -1e-4f) continue;
                                const float value = std::max(0.0f,
                                    wa * a.energy + wb * b.energy + wc * c.energy) * rayGain * irradiance;
                                if (value > 0.0f) {
                                    pathDiagnostic.luminance += value *
                                        (band == 0 ? 0.2126 : (band == 1 ? 0.7152 : 0.0722));
                                    pathDiagnostic.minX = std::min(pathDiagnostic.minX, static_cast<float>(x));
                                    pathDiagnostic.minY = std::min(pathDiagnostic.minY, static_cast<float>(y));
                                    pathDiagnostic.maxX = std::max(pathDiagnostic.maxX, static_cast<float>(x));
                                    pathDiagnostic.maxY = std::max(pathDiagnostic.maxY, static_cast<float>(y));
                                }
                                Pixel& dst = output.pixels[static_cast<std::size_t>(y) * width + x];
                                if (band == 0) dst.r += value;
                                else if (band == 1) dst.g += value;
                                else dst.b += value;
                            }
                        };

                        for (int gy = 0; gy + 1 < grid; ++gy) for (int gx = 0; gx + 1 < grid; ++gx) {
                            const RayVertex& a = rayGridData[static_cast<std::size_t>(gy) * grid + gx];
                            const RayVertex& b = rayGridData[static_cast<std::size_t>(gy) * grid + gx + 1];
                            const RayVertex& c = rayGridData[static_cast<std::size_t>(gy + 1) * grid + gx];
                            const RayVertex& d = rayGridData[static_cast<std::size_t>(gy + 1) * grid + gx + 1];
                            if (a.valid && b.valid && c.valid && d.valid) {
                                rasterizeTriangle(a, b, d);
                                rasterizeTriangle(a, d, c);
                            }
                        }
                    }
                }
            }

            const float centerX = sourceU * (width - 1);
            const float centerY = sourceV * (height - 1);
            for (std::size_t band = 0; band < diffraction.size(); ++band) {
                const float sourceBand = sourceBands[band];
                const DiffractionKernel& kernel = diffraction[band];
                if (sourceBand <= 1e-6f || kernel.intensity.empty()) continue;
                const int half = kernel.size / 2;
                for (int ky = 0; ky < kernel.size; ++ky) for (int kx = 0; kx < kernel.size; ++kx) {
                    const int fx = kx - half;
                    const int fy = ky - half;
                    const float offsetX = fx * kernel.pixelScale;
                    const float offsetY = fy * kernel.pixelScale;
                    if (offsetX * offsetX + offsetY * offsetY < 0.25f) continue;
                    const float x = centerX + offsetX;
                    const float y = centerY - offsetY;
                    const int x0 = static_cast<int>(std::floor(x));
                    const int y0 = static_cast<int>(std::floor(y));
                    const float tx = x - x0, ty = y - y0;
                    const float value = kernel.intensity[static_cast<std::size_t>(ky) * kernel.size + kx] *
                                        sourceBand * sourceEnergy * settings.gain;
                    for (int oy = 0; oy <= 1; ++oy) for (int ox = 0; ox <= 1; ++ox) {
                        const int px = x0 + ox, py = y0 + oy;
                        if (px < 0 || py < 0 || px >= width || py >= height) continue;
                        const float weight = (ox ? tx : 1.0f - tx) * (oy ? ty : 1.0f - ty);
                        Pixel& dst = output.pixels[static_cast<std::size_t>(py) * width + px];
                        const float addition = value * weight;
                        if (band == 0) dst.r += addition;
                        else if (band == 1) dst.g += addition;
                        else dst.b += addition;
                    }
                }
            }
        }
        if (settings.reportGhostPaths) {
            std::array<std::size_t, kGhostPathCount> order{};
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            const std::size_t visiblePathCount = static_cast<std::size_t>(std::count_if(
                pathDiagnostics.begin(), pathDiagnostics.end(),
                [](const GhostPathDiagnostic& path) { return path.luminance > 0.0; }));
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                return pathDiagnostics[a].luminance > pathDiagnostics[b].luminance;
            });
            std::cout << "Top reflected ghost paths for source (" << std::fixed
                      << std::setprecision(3) << light.u << ',' << light.v
                      << "); " << visiblePathCount << '/' << consideredPathCount
                      << " paths visible, surface indices are zero-based:\n";
            std::size_t printed = 0;
            for (std::size_t index : order) {
                const GhostPathDiagnostic& path = pathDiagnostics[index];
                if (path.luminance <= 0.0 || printed == 16) break;
                std::cout << "  " << path.firstReflection << " -> " << path.secondReflection
                          << "  luminance=" << std::scientific << path.luminance
                          << "  bounds=(" << std::fixed << std::setprecision(1)
                          << path.minX << ',' << path.minY << ")..(" << path.maxX << ',' << path.maxY << ")\n";
                ++printed;
            }
        }
    }
    if (settings.reportJacobianCap && rasterizedTriangles > 0) {
        const double clampedPercent = 100.0 * static_cast<double>(jacobianClampedTriangles) /
                                      static_cast<double>(rasterizedTriangles);
        std::cout << "Jacobian cap=" << settings.jacobianCap << " clipped "
                  << jacobianClampedTriangles << '/' << rasterizedTriangles << " ray triangles ("
                  << std::fixed << std::setprecision(2) << clampedPercent << "%)\n";
    }
    return output;
}

void savePfm(const Image& image, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output: " + path);
    out << "PF\n" << image.width << ' ' << image.height << "\n-1.0\n";
    for (int y = image.height - 1; y >= 0; --y) for (int x = 0; x < image.width; ++x) {
        const Pixel& p = image.pixels[static_cast<std::size_t>(y) * image.width + x];
        const float rgb[3]{p.r, p.g, p.b};
        out.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    }
    if (!out) throw std::runtime_error("failed writing output: " + path);
}

unsigned char toSrgb(float value) {
    const float mapped = std::max(0.0f, value) / (1.0f + std::max(0.0f, value));
    const float srgb = mapped <= 0.0031308f ? 12.92f * mapped : 1.055f * std::pow(mapped, 1.0f / 2.4f) - 0.055f;
    return static_cast<unsigned char>(std::lround(finiteClamp(srgb, 0.0f, 1.0f) * 255.0f));
}

void savePpm(const Image& image, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output: " + path);
    out << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (const Pixel& p : image.pixels) {
        const unsigned char rgb[3]{toSrgb(p.r), toSrgb(p.g), toSrgb(p.b)};
        out.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    }
    if (!out) throw std::runtime_error("failed writing output: " + path);
}

void saveImage(const Image& image, const std::string& path) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".pfm") savePfm(image, path);
    else savePpm(image, path);
}

void printSettings(const Settings& s) {
    std::cout << std::boolalpha << "auto=" << s.autoPosition << " position=" << s.positionX << ',' << s.positionY
              << " threshold=" << s.threshold << " gain=" << s.gain << " rayGrid=" << s.rayGrid
              << " jacobianCap=" << s.jacobianCap << " sourceSamples=" << s.sourceSamples
              << " dispersion=" << (s.useCauchyDispersion ? "cauchy" : "linear-abbe")
              << " apertureRadius=" << s.apertureRadius << " apertureBlades=" << s.apertureBlades
              << " antiReflectCoating=" << s.antiReflectCoating
              << " diffraction=" << s.diffractionEnabled
              << " ghostPair=" << s.selectedFirstReflection << ',' << s.selectedSecondReflection
              << " lens=Angenieux-Double-Gauss-100mm\n";
}

void printHelp() {
    std::cout << "Commands: show | auto on/off | position X Y | set NAME VALUE | render | save PATH | help | quit\n"
              << "Names: threshold gain jacobianCap rayGrid sourceSamples dispersionModel apertureRadius apertureBlades antiReflectCoating diffractionEnabled\n"
              << "CLI diagnostics: --ghost-report | --ghost-pair FIRST SECOND | --jacobian-report | --source-report\n";
}

void setValue(Settings& s, const std::string& name, float value) {
    if (name == "threshold") s.threshold = finiteClamp(value, 0.0f, 4.0f);
    else if (name == "gain") s.gain = finiteClamp(value, 0.0f, 10000.0f);
    else if (name == "jacobianCap") s.jacobianCap = finiteClamp(value, 0.01f, 4096.0f);
    else if (name == "rayGrid") s.rayGrid = static_cast<int>(std::lround(finiteClamp(value, 8.0f, 192.0f)));
    else if (name == "sourceSamples") {
        if (value != 1.0f && value != 9.0f)
            throw std::runtime_error("sourceSamples must be 1 or 9");
        s.sourceSamples = static_cast<int>(value);
    }
    else if (name == "dispersionModel") s.useCauchyDispersion = value >= 0.5f;
    else if (name == "apertureRadius") s.apertureRadius = finiteClamp(value, 1.0f, 12.75f);
    else if (name == "apertureBlades") s.apertureBlades = static_cast<int>(std::lround(finiteClamp(value, 3.0f, 12.0f)));
    else if (name == "antiReflectCoating") s.antiReflectCoating = value >= 0.5f;
    else if (name == "diffractionEnabled") s.diffractionEnabled = value >= 0.5f;
    else throw std::runtime_error("unknown parameter: " + name);
}

void parseOption(Settings& settings, int& i, int argc, char** argv) {
    const std::string option = argv[i];
    auto value = [&]() {
        if (++i >= argc) throw std::runtime_error("missing value for " + option);
        return std::stof(argv[i]);
    };
    if (option == "--auto") settings.autoPosition = true;
    else if (option == "--position") {
        settings.autoPosition = false;
        settings.positionX = finiteClamp(value(), 0.0f, 1.0f);
        settings.positionY = finiteClamp(value(), 0.0f, 1.0f);
    } else if (option == "--threshold") setValue(settings, "threshold", value());
    else if (option == "--gain") setValue(settings, "gain", value());
    else if (option == "--jacobian-cap") setValue(settings, "jacobianCap", value());
    else if (option == "--ray-grid") setValue(settings, "rayGrid", value());
    else if (option == "--source-samples") setValue(settings, "sourceSamples", value());
    else if (option == "--dispersion") {
        if (++i >= argc) throw std::runtime_error("missing value for --dispersion");
        const std::string model = argv[i];
        if (model == "cauchy") settings.useCauchyDispersion = true;
        else if (model == "linear") settings.useCauchyDispersion = false;
        else throw std::runtime_error("--dispersion expects cauchy or linear");
    }
    else if (option == "--aperture-radius") setValue(settings, "apertureRadius", value());
    else if (option == "--aperture-blades") setValue(settings, "apertureBlades", value());
    else if (option == "--coating") {
        if (++i >= argc) throw std::runtime_error("missing value for --coating");
        const std::string enabled = argv[i];
        if (enabled == "on") settings.antiReflectCoating = true;
        else if (enabled == "off") settings.antiReflectCoating = false;
        else throw std::runtime_error("--coating expects on or off");
    }
    else if (option == "--diffraction") {
        if (++i >= argc) throw std::runtime_error("missing value for --diffraction");
        const std::string enabled = argv[i];
        if (enabled == "on") settings.diffractionEnabled = true;
        else if (enabled == "off") settings.diffractionEnabled = false;
        else throw std::runtime_error("--diffraction expects on or off");
    }
    else if (option == "--ghost-report") settings.reportGhostPaths = true;
    else if (option == "--source-report") settings.reportSourceSamples = true;
    else if (option == "--jacobian-report") settings.reportJacobianCap = true;
    else if (option == "--ghost-pair") {
        if (i + 2 >= argc) throw std::runtime_error("--ghost-pair expects FIRST SECOND surface indices");
        settings.selectedFirstReflection = std::stoi(argv[++i]);
        settings.selectedSecondReflection = std::stoi(argv[++i]);
        if (settings.selectedSecondReflection < 0 ||
            settings.selectedFirstReflection <= settings.selectedSecondReflection ||
            settings.selectedFirstReflection >= static_cast<int>(kLensSurfaceCount)) {
            throw std::runtime_error("--ghost-pair requires 0 <= SECOND < FIRST < 14");
        }
    }
    else throw std::runtime_error("unknown option: " + option);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: LensFlareLab INPUT.(ppm|pfm) OUTPUT.(ppm|pfm) [--dispersion cauchy|linear] [options]\n";
        return 2;
    }
    try {
        const Image source = loadImage(argv[1]);
        Settings settings;
        for (int i = 3; i < argc; ++i) parseOption(settings, i, argc, argv);
        if (argc == 3) {
            std::cout << "Loaded " << source.width << 'x' << source.height << ". ";
            printHelp();
            std::string line;
            while (std::cout << "> " && std::getline(std::cin, line)) {
                std::istringstream command(line);
                std::string verb;
                command >> verb;
                if (verb.empty()) continue;
                if (verb == "quit" || verb == "exit") break;
                if (verb == "help") printHelp();
                else if (verb == "show") printSettings(settings);
                else if (verb == "auto") {
                    std::string state; command >> state;
                    settings.autoPosition = state == "on";
                    if (state != "on" && state != "off") std::cout << "Use auto on or auto off\n";
                } else if (verb == "position") {
                    float x = 0.0f, y = 0.0f;
                    if (!(command >> x >> y)) std::cout << "Use position X Y\n";
                    else {
                        settings.autoPosition = false;
                        settings.positionX = finiteClamp(x, 0.0f, 1.0f);
                        settings.positionY = finiteClamp(y, 0.0f, 1.0f);
                    }
                } else if (verb == "set") {
                    std::string name; float value = 0.0f;
                    if (!(command >> name >> value)) std::cout << "Use set NAME VALUE\n";
                    else setValue(settings, name, value);
                } else if (verb == "render") {
                    const Image output = render(source, settings);
                    saveImage(output, argv[2]);
                    std::cout << "Rendered to " << argv[2] << '\n';
                } else if (verb == "save") {
                    std::string path; command >> path;
                    if (path.empty()) std::cout << "Use save PATH\n";
                    else saveImage(render(source, settings), path);
                } else std::cout << "Unknown command. Type help.\n";
            }
        } else {
            saveImage(render(source, settings), argv[2]);
            std::cout << "Rendered " << source.width << 'x' << source.height << " to " << argv[2] << '\n';
            printSettings(settings);
        }
    } catch (const std::exception& error) {
        std::cerr << "LensFlareLab: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
