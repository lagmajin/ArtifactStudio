#pragma once

#include <QString>
#include <QVector3D>
#include <QMatrix4x4>
#include <vector>

struct BoneNode {
    int id = -1;
    QString name;
    int parentId = -1;
    std::vector<int> children;

    QVector3D localTranslation;
    QVector3D localRotation;
    QVector3D localScaling{1.0f, 1.0f, 1.0f};

    QMatrix4x4 worldMatrix;
    QVector3D headPosition;
    QVector3D tailPosition;
    float boneLength = 0.0f;
    bool hasTail = false;
};

struct SkeletonData {
    std::vector<BoneNode> bones;
    QVector3D boundsMin;
    QVector3D boundsMax;
    bool valid = false;

    // Helper: find root bones
    std::vector<int> getRoots() const {
        std::vector<int> roots;
        for (const auto& b : bones) {
            if (b.parentId < 0) {
                roots.push_back(b.id);
            }
        }
        return roots;
    }
};

class FbxSkeletonLoader {
public:
    static SkeletonData loadFbx(const QString& filePath);
    static SkeletonData createTestHumanoidSkeleton();
};
