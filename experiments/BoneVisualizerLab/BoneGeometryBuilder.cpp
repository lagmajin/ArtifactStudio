#include "BoneGeometryBuilder.h"
#include <cmath>

static const float PI = 3.14159265358979323846f;

void BoneGeometryBuilder::buildSkeletonGeometry(
    const SkeletonData& skeleton,
    BoneMeshBuffer& outBuffer,
    float boneThickness,
    float jointRadius,
    float axisLength
) {
    outBuffer.clear();
    if (!skeleton.valid || skeleton.bones.empty()) return;

    // Palette: DCC standard bone color (e.g. Maya/Blender bone cream/cyan tint)
    const QVector4D boneColor(0.85f, 0.85f, 0.88f, 1.0f);
    const QVector4D jointColor(0.3f, 0.7f, 1.0f, 1.0f);

    for (const auto& bone : skeleton.bones) {
        // 1. Joint Sphere at head
        appendJointSphere(bone.headPosition, jointRadius, jointColor, outBuffer);

        // 2. Axes at joint
        QMatrix4x4 axisMat = bone.worldMatrix;
        axisMat.setColumn(3, QVector4D(bone.headPosition, 1.0f));
        appendAxes(axisMat, axisLength, outBuffer);

        // 3. Octahedral bone body from head to tail
        if (bone.hasTail && bone.boneLength > 0.001f) {
            float effThickness = std::max(0.005f, std::min(boneThickness, bone.boneLength * 0.15f));
            appendOctahedralBone(bone.headPosition, bone.tailPosition, effThickness, boneColor, outBuffer);
        }
    }
}

void BoneGeometryBuilder::appendOctahedralBone(
    const QVector3D& head,
    const QVector3D& tail,
    float thickness,
    const QVector4D& color,
    BoneMeshBuffer& outBuffer
) {
    QVector3D dir = tail - head;
    float len = dir.length();
    if (len < 0.001f) return;
    QVector3D Z = dir / len; // longitudinal axis

    // Compute perpendicular coordinate frame
    QVector3D up = std::abs(Z.y()) < 0.99f ? QVector3D(0, 1, 0) : QVector3D(1, 0, 0);
    QVector3D X = QVector3D::crossProduct(Z, up).normalized();
    QVector3D Y = QVector3D::crossProduct(X, Z).normalized();

    // Intermediate widest ring is around 15% - 20% along the bone
    float ringT = 0.18f;
    QVector3D ringCenter = head + Z * (len * ringT);
    float r = thickness;

    QVector3D P0 = head;
    QVector3D P1 = ringCenter + X * r;
    QVector3D P2 = ringCenter + Y * r;
    QVector3D P3 = ringCenter - X * r;
    QVector3D P4 = ringCenter - Y * r;
    QVector3D P5 = tail;

    // Solid Octahedron (8 triangles)
    auto addTri = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) {
        QVector3D n = QVector3D::crossProduct(b - a, c - a).normalized();
        unsigned int base = static_cast<unsigned int>(outBuffer.solidVertices.size());
        outBuffer.solidVertices.push_back({a, n, color});
        outBuffer.solidVertices.push_back({b, n, color});
        outBuffer.solidVertices.push_back({c, n, color});
        outBuffer.solidIndices.push_back(base);
        outBuffer.solidIndices.push_back(base + 1);
        outBuffer.solidIndices.push_back(base + 2);
    };

    // Head pyramid (4 triangles)
    addTri(P0, P2, P1);
    addTri(P0, P3, P2);
    addTri(P0, P4, P3);
    addTri(P0, P1, P4);

    // Tail pyramid (4 triangles)
    addTri(P5, P1, P2);
    addTri(P5, P2, P3);
    addTri(P5, P3, P4);
    addTri(P5, P4, P1);

    // Wireframe edges (12 edges)
    auto addLine = [&](const QVector3D& a, const QVector3D& b) {
        unsigned int base = static_cast<unsigned int>(outBuffer.wireVertices.size());
        QVector4D wireColor(0.15f, 0.15f, 0.18f, 1.0f);
        outBuffer.wireVertices.push_back({a, QVector3D(0, 0, 1), wireColor});
        outBuffer.wireVertices.push_back({b, QVector3D(0, 0, 1), wireColor});
        outBuffer.wireIndices.push_back(base);
        outBuffer.wireIndices.push_back(base + 1);
    };

    addLine(P0, P1); addLine(P0, P2); addLine(P0, P3); addLine(P0, P4);
    addLine(P1, P2); addLine(P2, P3); addLine(P3, P4); addLine(P4, P1);
    addLine(P5, P1); addLine(P5, P2); addLine(P5, P3); addLine(P5, P4);
}

void BoneGeometryBuilder::appendJointSphere(
    const QVector3D& center,
    float radius,
    const QVector4D& color,
    BoneMeshBuffer& outBuffer
) {
    const int stacks = 6;
    const int slices = 8;
    unsigned int baseIndex = static_cast<unsigned int>(outBuffer.jointVertices.size());

    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * float(i) / float(stacks);
        float y = std::cos(phi);
        float r = std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * PI * float(j) / float(slices);
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            QVector3D n(x, y, z);
            QVector3D p = center + n * radius;
            outBuffer.jointVertices.push_back({p, n, color});
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned int p0 = baseIndex + i * (slices + 1) + j;
            unsigned int p1 = p0 + 1;
            unsigned int p2 = p0 + (slices + 1);
            unsigned int p3 = p2 + 1;

            outBuffer.jointIndices.push_back(p0);
            outBuffer.jointIndices.push_back(p2);
            outBuffer.jointIndices.push_back(p1);

            outBuffer.jointIndices.push_back(p1);
            outBuffer.jointIndices.push_back(p2);
            outBuffer.jointIndices.push_back(p3);
        }
    }
}

void BoneGeometryBuilder::appendAxes(
    const QMatrix4x4& transform,
    float length,
    BoneMeshBuffer& outBuffer
) {
    QVector3D origin = transform.column(3).toVector3D();
    QVector3D xDir = transform.column(0).toVector3D().normalized() * length;
    QVector3D yDir = transform.column(1).toVector3D().normalized() * length;
    QVector3D zDir = transform.column(2).toVector3D().normalized() * length;

    auto addAxisLine = [&](const QVector3D& start, const QVector3D& end, const QVector4D& col) {
        unsigned int base = static_cast<unsigned int>(outBuffer.axisVertices.size());
        outBuffer.axisVertices.push_back({start, QVector3D(0, 0, 1), col});
        outBuffer.axisVertices.push_back({end, QVector3D(0, 0, 1), col});
        outBuffer.axisIndices.push_back(base);
        outBuffer.axisIndices.push_back(base + 1);
    };

    // Red = X, Green = Y, Blue = Z
    addAxisLine(origin, origin + xDir, QVector4D(0.95f, 0.2f, 0.2f, 1.0f));
    addAxisLine(origin, origin + yDir, QVector4D(0.2f, 0.85f, 0.2f, 1.0f));
    addAxisLine(origin, origin + zDir, QVector4D(0.2f, 0.4f, 0.95f, 1.0f));
}
