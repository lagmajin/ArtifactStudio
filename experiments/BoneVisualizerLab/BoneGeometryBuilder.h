#pragma once

#include "FbxSkeletonLoader.h"
#include <vector>
#include <QVector3D>
#include <QVector4D>

struct VertexPN {
    QVector3D position;
    QVector3D normal;
    QVector4D color;
};

struct BoneMeshBuffer {
    std::vector<VertexPN> solidVertices;
    std::vector<unsigned int> solidIndices;

    std::vector<VertexPN> wireVertices;
    std::vector<unsigned int> wireIndices;

    std::vector<VertexPN> jointVertices;
    std::vector<unsigned int> jointIndices;

    std::vector<VertexPN> axisVertices;
    std::vector<unsigned int> axisIndices;

    void clear() {
        solidVertices.clear();
        solidIndices.clear();
        wireVertices.clear();
        wireIndices.clear();
        jointVertices.clear();
        jointIndices.clear();
        axisVertices.clear();
        axisIndices.clear();
    }
};

class BoneGeometryBuilder {
public:
    static void buildSkeletonGeometry(
        const SkeletonData& skeleton,
        BoneMeshBuffer& outBuffer,
        float boneThickness = 0.04f,
        float jointRadius = 0.025f,
        float axisLength = 0.05f
    );

private:
    static void appendOctahedralBone(
        const QVector3D& head,
        const QVector3D& tail,
        float thickness,
        const QVector4D& color,
        BoneMeshBuffer& outBuffer
    );

    static void appendJointSphere(
        const QVector3D& center,
        float radius,
        const QVector4D& color,
        BoneMeshBuffer& outBuffer
    );

    static void appendAxes(
        const QMatrix4x4& transform,
        float length,
        BoneMeshBuffer& outBuffer
    );
};
