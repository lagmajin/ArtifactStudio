#include "FbxSkeletonLoader.h"
#include "ufbx.h"
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cmath>

static QVector3D toQVec3(const ufbx_vec3& v) {
    return QVector3D(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

static QMatrix4x4 toQMat4(const ufbx_matrix& m) {
    // ufbx_matrix is a 4x3 affine transform (cols m.cols[0..3])
    return QMatrix4x4(
        static_cast<float>(m.cols[0].x), static_cast<float>(m.cols[1].x), static_cast<float>(m.cols[2].x), static_cast<float>(m.cols[3].x),
        static_cast<float>(m.cols[0].y), static_cast<float>(m.cols[1].y), static_cast<float>(m.cols[2].y), static_cast<float>(m.cols[3].y),
        static_cast<float>(m.cols[0].z), static_cast<float>(m.cols[1].z), static_cast<float>(m.cols[2].z), static_cast<float>(m.cols[3].z),
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

SkeletonData FbxSkeletonLoader::loadFbx(const QString& filePath) {
    SkeletonData skel;
    QFileInfo checkFile(filePath);
    if (!checkFile.exists() || !checkFile.isFile()) {
        qWarning() << "File does not exist:" << filePath;
        return skel;
    }

    ufbx_load_opts opts = {0};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f; // Standardize to meters
    opts.evaluate_skinning = true;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(filePath.toUtf8().constData(), &opts, &error);
    if (!scene) {
        qWarning() << "ufbx_load_file error:" << error.description.data;
        return skel;
    }

    std::vector<ufbx_node*> boneNodes;
    for (size_t i = 0; i < scene->nodes.count; ++i) {
        ufbx_node* node = scene->nodes.data[i];
        if (node->bone || (node->attrib_type == UFBX_ELEMENT_BONE)) {
            boneNodes.push_back(node);
        }
    }

    // Fallback: If no explicit bone attribute, check skin clusters
    if (boneNodes.empty()) {
        for (size_t i = 0; i < scene->skin_deformers.count; ++i) {
            ufbx_skin_deformer* skin = scene->skin_deformers.data[i];
            for (size_t c = 0; c < skin->clusters.count; ++c) {
                ufbx_skin_cluster* cluster = skin->clusters.data[c];
                if (cluster->bone_node) {
                    if (std::find(boneNodes.begin(), boneNodes.end(), cluster->bone_node) == boneNodes.end()) {
                        boneNodes.push_back(cluster->bone_node);
                    }
                }
            }
        }
    }

    if (boneNodes.empty()) {
        // As a last fallback, import any node that has children or transform hierarchy
        for (size_t i = 0; i < scene->nodes.count; ++i) {
            ufbx_node* node = scene->nodes.data[i];
            if (node->is_root) continue;
            boneNodes.push_back(node);
        }
    }

    skel.bones.resize(boneNodes.size());
    for (size_t i = 0; i < boneNodes.size(); ++i) {
        ufbx_node* node = boneNodes[i];
        BoneNode& b = skel.bones[i];
        b.id = static_cast<int>(i);
        b.name = QString::fromUtf8(node->name.data);
        b.worldMatrix = toQMat4(node->geometry_to_world);
        b.headPosition = b.worldMatrix.column(3).toVector3D();
        b.localTranslation = toQVec3(node->local_transform.translation);
        ufbx_vec3 rotEuler = ufbx_quat_to_euler(node->local_transform.rotation, node->rotation_order);
        b.localRotation = toQVec3(rotEuler);
        b.localScaling = toQVec3(node->local_transform.scale);
    }

    // Resolve parent/child IDs
    for (size_t i = 0; i < boneNodes.size(); ++i) {
        ufbx_node* node = boneNodes[i];
        if (node->parent) {
            for (size_t j = 0; j < boneNodes.size(); ++j) {
                if (boneNodes[j] == node->parent) {
                    skel.bones[i].parentId = static_cast<int>(j);
                    skel.bones[j].children.push_back(static_cast<int>(i));
                    break;
                }
            }
        }
    }

    // Calculate tail positions and bone lengths
    QVector3D bMin(1e9f, 1e9f, 1e9f);
    QVector3D bMax(-1e9f, -1e9f, -1e9f);

    for (size_t i = 0; i < skel.bones.size(); ++i) {
        BoneNode& b = skel.bones[i];
        bMin.setX(std::min(bMin.x(), b.headPosition.x()));
        bMin.setY(std::min(bMin.y(), b.headPosition.y()));
        bMin.setZ(std::min(bMin.z(), b.headPosition.z()));
        bMax.setX(std::max(bMax.x(), b.headPosition.x()));
        bMax.setY(std::max(bMax.y(), b.headPosition.y()));
        bMax.setZ(std::max(bMax.z(), b.headPosition.z()));

        if (!b.children.empty()) {
            // Tail is average of children heads
            QVector3D avgChildHead(0, 0, 0);
            for (int childId : b.children) {
                avgChildHead += skel.bones[childId].headPosition;
            }
            avgChildHead /= static_cast<float>(b.children.size());
            b.tailPosition = avgChildHead;
            b.boneLength = (b.tailPosition - b.headPosition).length();
            b.hasTail = true;
        } else {
            // Leaf bone: project along parent's direction or local axis
            if (b.parentId >= 0) {
                QVector3D parentDir = b.headPosition - skel.bones[b.parentId].headPosition;
                float parentLen = parentDir.length();
                if (parentLen > 0.001f) {
                    b.boneLength = parentLen * 0.5f;
                    b.tailPosition = b.headPosition + parentDir.normalized() * b.boneLength;
                    b.hasTail = true;
                }
            }
            if (!b.hasTail) {
                b.boneLength = 0.1f;
                b.tailPosition = b.headPosition + QVector3D(0.0f, 0.1f, 0.0f);
                b.hasTail = true;
            }
        }
    }

    ufbx_free_scene(scene);
    skel.boundsMin = bMin;
    skel.boundsMax = bMax;
    skel.valid = !skel.bones.empty();
    return skel;
}

SkeletonData FbxSkeletonLoader::createTestHumanoidSkeleton() {
    SkeletonData skel;
    // Build a standard biped humanoid skeleton for testing
    auto addBone = [&](const QString& name, int parent, const QVector3D& head, const QVector3D& tail) {
        int id = static_cast<int>(skel.bones.size());
        BoneNode b;
        b.id = id;
        b.name = name;
        b.parentId = parent;
        b.headPosition = head;
        b.tailPosition = tail;
        b.boneLength = (tail - head).length();
        b.hasTail = true;
        b.worldMatrix.setToIdentity();
        b.worldMatrix.translate(head);
        if (parent >= 0) {
            skel.bones[parent].children.push_back(id);
        }
        skel.bones.push_back(b);
        return id;
    };

    // Spine hierarchy
    int root = addBone("Hips", -1, QVector3D(0, 1.0f, 0), QVector3D(0, 1.2f, 0));
    int spine = addBone("Spine", root, QVector3D(0, 1.2f, 0), QVector3D(0, 1.45f, 0));
    int chest = addBone("Chest", spine, QVector3D(0, 1.45f, 0), QVector3D(0, 1.6f, 0));
    int neck = addBone("Neck", chest, QVector3D(0, 1.6f, 0), QVector3D(0, 1.7f, 0));
    addBone("Head", neck, QVector3D(0, 1.7f, 0), QVector3D(0, 1.95f, 0));

    // Left Arm
    int lClav = addBone("LeftCollar", chest, QVector3D(0.05f, 1.55f, 0), QVector3D(0.2f, 1.55f, 0));
    int lArm = addBone("LeftUpperArm", lClav, QVector3D(0.2f, 1.55f, 0), QVector3D(0.5f, 1.55f, 0));
    int lForearm = addBone("LeftForearm", lArm, QVector3D(0.5f, 1.55f, 0), QVector3D(0.8f, 1.55f, 0));
    addBone("LeftHand", lForearm, QVector3D(0.8f, 1.55f, 0), QVector3D(0.95f, 1.55f, 0));

    // Right Arm
    int rClav = addBone("RightCollar", chest, QVector3D(-0.05f, 1.55f, 0), QVector3D(-0.2f, 1.55f, 0));
    int rArm = addBone("RightUpperArm", rClav, QVector3D(-0.2f, 1.55f, 0), QVector3D(-0.5f, 1.55f, 0));
    int rForearm = addBone("RightForearm", rArm, QVector3D(-0.5f, 1.55f, 0), QVector3D(-0.8f, 1.55f, 0));
    addBone("RightHand", rForearm, QVector3D(-0.8f, 1.55f, 0), QVector3D(-0.95f, 1.55f, 0));

    // Left Leg
    int lUpLeg = addBone("LeftUpperLeg", root, QVector3D(0.12f, 1.0f, 0), QVector3D(0.12f, 0.5f, 0));
    int lLowLeg = addBone("LeftLowerLeg", lUpLeg, QVector3D(0.12f, 0.5f, 0), QVector3D(0.12f, 0.1f, 0));
    addBone("LeftFoot", lLowLeg, QVector3D(0.12f, 0.1f, 0), QVector3D(0.12f, 0.0f, 0.2f));

    // Right Leg
    int rUpLeg = addBone("RightUpperLeg", root, QVector3D(-0.12f, 1.0f, 0), QVector3D(-0.12f, 0.5f, 0));
    int rLowLeg = addBone("RightLowerLeg", rUpLeg, QVector3D(-0.12f, 0.5f, 0), QVector3D(-0.12f, 0.1f, 0));
    addBone("RightFoot", rLowLeg, QVector3D(-0.12f, 0.1f, 0), QVector3D(-0.12f, 0.0f, 0.2f));

    skel.boundsMin = QVector3D(-1.0f, 0.0f, -0.3f);
    skel.boundsMax = QVector3D(1.0f, 2.0f, 0.3f);
    skel.valid = true;
    return skel;
}
