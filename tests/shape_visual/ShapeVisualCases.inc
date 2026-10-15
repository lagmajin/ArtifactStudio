// Included by the Render Queue integration-test translation unit so the
// existing application environment, PNG boundary and queue waiter are shared.
// These are production layer/Composition tests, not a replacement rasterizer.
namespace {

Artifact::ShapeContent shapeVisualRectangle(double x, double y, double w, double h)
{
    Artifact::ShapeContent content;
    content.geometry.pathVertices = {
        {{x, y}, {}, {}, false}, {{x + w, y}, {}, {}, false},
        {{x + w, y + h}, {}, {}, false}, {{x, y + h}, {}, {}, false}};
    content.fill.color = ArtifactCore::FloatColor(1, 0, 0, 1);
    return content;
}

Artifact::ShapeContent shapeVisualCurve()
{
    auto content = shapeVisualRectangle(0, 0, 1, 1);
    content.geometry.pathVertices = {
        {{64, 256}, {}, {0, -200}, true},
        {{256, 64}, {-200, 0}, {200, 0}, true},
        {{448, 256}, {0, -200}, {0, 200}, true},
        {{256, 448}, {200, 0}, {-200, 0}, true}};
    return content;
}

class ShapeVisualTest : public testing::Test {
protected:
    Artifact::ArtifactProjectManager& projects =
        Artifact::ArtifactProjectManager::getInstance();
    Artifact::ArtifactRenderQueueService* queue = nullptr;
    ArtifactCore::CompositionID compositionId;
    QTemporaryDir projectRoot;
    QByteArray previousProjectRoot;
    bool hadProjectRoot = false;
    bool previousTestMode = false;
    bool environmentInstalled = false;

    void SetUp() override
    {
        ASSERT_TRUE(projectRoot.isValid());
        hadProjectRoot = qEnvironmentVariableIsSet("ARTIFACT_UI_TEST_PROJECT_ROOT");
        previousProjectRoot = qgetenv("ARTIFACT_UI_TEST_PROJECT_ROOT");
        previousTestMode = QStandardPaths::isTestModeEnabled();
        qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
        QStandardPaths::setTestModeEnabled(true);
        environmentInstalled = true;
        queue = Artifact::ArtifactRenderQueueService::instance();
        ASSERT_NE(queue, nullptr);
        queue->removeAllRenderQueues();
        queue->clearCompletedJobHistory();
        projects.closeCurrentProject();
        projects.createProject(QStringLiteral("ShapeVisualTest"), true);
        ASSERT_TRUE(projects.isProjectCreated());
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(QStringLiteral("ShapeVisual")));
        params.setResolution(512, 512);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        params.setBackgroundColor(ArtifactCore::FloatColor(0, 0, 0, 0));
        const auto result = projects.createComposition(params);
        ASSERT_TRUE(result.success);
        compositionId = result.id;
    }

    void TearDown() override
    {
        if (queue) queue->removeAllRenderQueues();
        if (!environmentInstalled) return;
        projects.closeCurrentProject();
        if (hadProjectRoot) qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", previousProjectRoot);
        else qunsetenv("ARTIFACT_UI_TEST_PROJECT_ROOT");
        QStandardPaths::setTestModeEnabled(previousTestMode);
    }

    Artifact::ArtifactShapeLayer* addLayer()
    {
        const Artifact::ArtifactLayerInitParams params(
            QStringLiteral("Shape"), Artifact::LayerType::Shape);
        const auto result = projects.addLayerToComposition(compositionId, params);
        if (!result.success) return nullptr;
        auto* layer = dynamic_cast<Artifact::ArtifactShapeLayer*>(result.layer.get());
        if (layer) layer->transform2D().setPosition(0, 0);
        return layer;
    }

    bool addContent(Artifact::ArtifactShapeLayer& layer,
                    const Artifact::ShapeContent& content)
    {
        const bool wasEmpty = layer.shapeContentCount() == 0;
        if (layer.addShapeContent(content) < 0) return false;
        // First insertion snapshots the legacy primitive. Remove that snapshot
        // explicitly, otherwise an unrelated default rectangle masks failures.
        return !wasEmpty || layer.removeShapeContentAt(0);
    }

    template<class Check>
    void capture(const char* name, Check check)
    {
        QString failure;
        const QString captureRoot = qEnvironmentVariable("ARTIFACT_UI_TEST_CAPTURE_ROOT");
        const QString outputRoot = captureRoot.isEmpty()
            ? QDir::current().filePath(QStringLiteral("shape-visual-output"))
            : QDir(captureRoot).filePath(QStringLiteral("shape_visual"));
        const QString framePath = QDir(outputRoot).filePath(
            QString::fromLatin1(name) + QStringLiteral("_0000.png"));
        ASSERT_TRUE(!QFileInfo::exists(framePath) || QFile::remove(framePath))
            << "Could not remove stale output: " << framePath.toStdString();
        ASSERT_TRUE(addAndRender(queue, compositionId, QStringLiteral("ShapeVisual"),
            QDir(outputRoot).filePath(QString::fromLatin1(name) + QStringLiteral(".png")),
            512, 512, check, &failure, 1, QStringLiteral("gpu"), 0))
            << failure.toStdString();
    }
};

TEST_F(ShapeVisualTest, CompoundHoleViaSubtract)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    ASSERT_TRUE(addContent(*layer, shapeVisualRectangle(64, 64, 384, 384)));
    auto hole = shapeVisualRectangle(192, 192, 128, 128);
    hole.merge = Artifact::ShapeContentMerge::Subtract;
    ASSERT_TRUE(addContent(*layer, hole));
    capture("compound_hole", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(100, 100)), 240);
        EXPECT_LT(qAlpha(image.pixel(256, 256)), 10);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, SelfIntersectingPath)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    auto content = shapeVisualRectangle(0, 0, 1, 1);
    content.geometry.pathVertices = {
        {{64, 64}, {}, {}, false}, {{448, 448}, {}, {}, false},
        {{64, 448}, {}, {}, false}, {{448, 64}, {}, {}, false}};
    ASSERT_TRUE(addContent(*layer, content));
    capture("self_intersection", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 100)), 240);
        EXPECT_GT(qAlpha(image.pixel(256, 412)), 240);
        EXPECT_LT(qAlpha(image.pixel(100, 256)), 10);
        EXPECT_LT(qAlpha(image.pixel(412, 256)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, CompoundFillRulesAndContourDirection)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    auto content = shapeVisualRectangle(0, 0, 1, 1);
    // Two closed contours joined by a retraced bridge. Both initially have
    // the same winding, so EvenOdd and Winding must differ at the center.
    content.geometry.pathVertices = {
        {{64, 64}, {}, {}, false}, {{448, 64}, {}, {}, false},
        {{448, 448}, {}, {}, false}, {{64, 448}, {}, {}, false},
        {{64, 64}, {}, {}, false}, {{192, 192}, {}, {}, false},
        {{320, 192}, {}, {}, false}, {{320, 320}, {}, {}, false},
        {{192, 320}, {}, {}, false}, {{192, 192}, {}, {}, false},
        {{64, 64}, {}, {}, false}};
    content.geometry.fillRule = ArtifactCore::PathFillRule::Winding;
    ASSERT_TRUE(addContent(*layer, content));
    capture("compound_winding", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        EXPECT_GT(qAlpha(image.pixel(128, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    content.geometry.fillRule = ArtifactCore::PathFillRule::EvenOdd;
    ASSERT_TRUE(layer->setShapeContentAt(0, content));
    capture("compound_evenodd", [](const auto& image) {
        EXPECT_LT(qAlpha(image.pixel(256, 256)), 10);
        EXPECT_GT(qAlpha(image.pixel(128, 256)), 240);
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    content.geometry.fillRule = ArtifactCore::PathFillRule::Winding;
    std::reverse(content.geometry.pathVertices.begin() + 6,
                 content.geometry.pathVertices.begin() + 9);
    ASSERT_TRUE(layer->setShapeContentAt(0, content));
    capture("compound_reversed_inner", [](const auto& image) {
        EXPECT_LT(qAlpha(image.pixel(256, 256)), 10);
        EXPECT_GT(qAlpha(image.pixel(128, 256)), 240);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, BezierOverlap)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    ASSERT_TRUE(addContent(*layer, shapeVisualCurve()));
    auto second = shapeVisualCurve();
    second.transform.position = QPointF(80, 0);
    second.fill.color = ArtifactCore::FloatColor(0, 0, 1, 1);
    ASSERT_TRUE(addContent(*layer, second));
    capture("bezier_overlap", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        EXPECT_GT(qBlue(image.pixel(336, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, TranslucentContents)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    auto red = shapeVisualRectangle(64, 128, 256, 256);
    red.fill.color = ArtifactCore::FloatColor(1, 0, 0, 0.5f);
    auto blue = shapeVisualRectangle(192, 128, 256, 256);
    blue.fill.color = ArtifactCore::FloatColor(0, 0, 1, 0.5f);
    ASSERT_TRUE(addContent(*layer, red));
    ASSERT_TRUE(addContent(*layer, blue));
    capture("translucent_contents", [](const auto& image) {
        EXPECT_NEAR(qAlpha(image.pixel(100, 256)), 128, 5);
        EXPECT_NEAR(qAlpha(image.pixel(256, 256)), 191, 5);
        EXPECT_NEAR(qAlpha(image.pixel(400, 256)), 128, 5);
        EXPECT_GT(qBlue(image.pixel(256, 256)), qRed(image.pixel(256, 256)));
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, TranslucentLayers)
{
    auto* first = addLayer();
    auto* second = addLayer();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    auto red = shapeVisualRectangle(64, 128, 256, 256);
    red.fill.color = ArtifactCore::FloatColor(1, 0, 0, 0.5f);
    auto blue = shapeVisualRectangle(192, 128, 256, 256);
    blue.fill.color = ArtifactCore::FloatColor(0, 0, 1, 0.5f);
    ASSERT_TRUE(addContent(*first, red));
    ASSERT_TRUE(addContent(*second, blue));
    capture("translucent_layers", [](const auto& image) {
        EXPECT_NEAR(qAlpha(image.pixel(100, 256)), 128, 5);
        EXPECT_NEAR(qAlpha(image.pixel(256, 256)), 191, 5);
        EXPECT_NEAR(qAlpha(image.pixel(400, 256)), 128, 5);
        EXPECT_GT(qBlue(image.pixel(256, 256)), qRed(image.pixel(256, 256)));
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    // Swap the two layer contents without changing the composition's layer
    // order: the top color must change while overlap alpha stays identical.
    ASSERT_TRUE(first->setShapeContentAt(0, blue));
    ASSERT_TRUE(second->setShapeContentAt(0, red));
    capture("translucent_layers_reversed", [](const auto& image) {
        EXPECT_NEAR(qAlpha(image.pixel(256, 256)), 191, 5);
        EXPECT_GT(qRed(image.pixel(256, 256)), qBlue(image.pixel(256, 256)));
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, DenseStrokes)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    for (int index = 0; index < 12; ++index) {
        auto content = shapeVisualRectangle(64 + index * 6, 64 + index * 6,
                                             384 - index * 12, 384 - index * 12);
        content.fill.enabled = false;
        content.stroke.enabled = true;
        content.stroke.width = 2;
        content.stroke.color = ArtifactCore::FloatColor(1, 1, 1, 1);
        ASSERT_TRUE(addContent(*layer, content));
    }
    capture("dense_strokes", [](const auto& image) {
        EXPECT_LT(qAlpha(image.pixel(256, 256)), 10);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        const int pixels = countPixelsMatching(image, [](QRgb p) { return qAlpha(p) > 32; });
        EXPECT_GT(pixels, 10000);
        EXPECT_LT(pixels, 60000);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, TransformedCurve)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    auto base = shapeVisualRectangle(224, 224, 64, 64);
    base.fill.color = ArtifactCore::FloatColor(0, 0, 1, 1);
    ASSERT_TRUE(addContent(*layer, base));
    auto content = shapeVisualCurve();
    content.transform.anchor = QPointF(256, 256);
    content.transform.rotation = 30;
    content.transform.scale = QPointF(0.7, 0.4);
    ASSERT_TRUE(addContent(*layer, content));
    capture("transformed_overlap", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(64, 64)), 10);
        const int pixels = countPixelsMatching(image, [](QRgb p) { return qAlpha(p) > 128; });
        EXPECT_GT(pixels, 15000);
        EXPECT_LT(pixels, 80000);
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    layer->transform2D().setScale(0.8f, 1.1f);
    layer->transform2D().setPosition(64, 32);
    capture("transformed_layer_and_content", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(269, 314)), 240);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        EXPECT_LT(qAlpha(image.pixel(480, 480)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, IntersectingContents)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    auto input = shapeVisualRectangle(64, 128, 256, 256);
    // Intersect clips this content against earlier geometry; earlier contents
    // retain their own paint. Keep the input geometry but disable its fill.
    input.fill.enabled = false;
    ASSERT_TRUE(addContent(*layer, input));
    auto content = shapeVisualRectangle(192, 128, 256, 256);
    content.merge = Artifact::ShapeContentMerge::Intersect;
    ASSERT_TRUE(addContent(*layer, content));
    capture("merge_intersect", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(100, 256)), 10);
        EXPECT_LT(qAlpha(image.pixel(400, 256)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, MergePathsOperatorModes)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    ASSERT_TRUE(addContent(*layer, shapeVisualRectangle(64, 128, 256, 256)));
    ASSERT_TRUE(addContent(*layer, shapeVisualRectangle(192, 128, 256, 256)));
    struct ModeCase {
        int mode;
        const char* name;
        bool left;
        bool overlap;
        bool right;
    };
    // Persisted MergePaths modes: Add, Subtract, Intersect, Difference, Merge.
    const ModeCase cases[] = {
        {0, "operator_add", true, true, true},
        {1, "operator_subtract", true, false, false},
        {2, "operator_intersect", false, true, false},
        {3, "operator_difference", true, false, true},
        {4, "operator_merge", true, true, true}};
    for (const auto& test : cases) {
        SCOPED_TRACE(test.name);
        QJsonObject op;
        op.insert(QStringLiteral("type"),
                  static_cast<int>(ArtifactCore::ShapeOperatorType::MergePaths));
        op.insert(QStringLiteral("mode"), test.mode);
        QJsonArray operators;
        operators.append(op);
        layer->restoreOperatorsFromJson(operators);
        ASSERT_EQ(layer->shapeOperatorCount(), 1);
        layer->clearShapeStackNodes();
        using Node = Artifact::ShapeStackNodeType;
        ASSERT_TRUE(layer->insertShapeStackNode(0, {Node::Path, 0, -1, true}));
        ASSERT_TRUE(layer->insertShapeStackNode(1, {Node::Path, 1, -1, true}));
        ASSERT_TRUE(layer->insertShapeStackNode(2, {Node::Operator, -1, 0, true}));
        ASSERT_TRUE(layer->insertShapeStackNode(3, {Node::Fill, 0, -1, true}));
        capture(test.name, [&](const auto& image) {
            EXPECT_NEAR(qAlpha(image.pixel(100, 256)), test.left ? 255 : 0, 10);
            EXPECT_NEAR(qAlpha(image.pixel(256, 256)), test.overlap ? 255 : 0, 10);
            EXPECT_NEAR(qAlpha(image.pixel(400, 256)), test.right ? 255 : 0, 10);
            EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
            return !testing::Test::HasFailure();
        });
        ASSERT_FALSE(HasFailure());
    }
}

TEST_F(ShapeVisualTest, OpenAndClosedStrokeStyles)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    for (int index = 0; index < 3; ++index) {
        auto path = shapeVisualRectangle(0, 0, 1, 1);
        const double y = 96 + index * 140;
        path.geometry.pathVertices = {
            {{80, y + 64}, {}, {}, false},
            {{160, y}, {}, {}, false},
            {{240, y + 64}, {}, {}, false}};
        path.geometry.pathClosed = false;
        path.fill.enabled = false;
        path.stroke.enabled = true;
        path.stroke.width = 12;
        path.stroke.color = ArtifactCore::FloatColor(1, 1, 1, 1);
        path.stroke.cap = static_cast<Artifact::StrokeCap>(index);
        path.stroke.join = static_cast<Artifact::StrokeJoin>(index);
        ASSERT_TRUE(addContent(*layer, path));
        path.transform.position = QPointF(220, 0);
        path.geometry.pathClosed = true;
        ASSERT_TRUE(addContent(*layer, path));
    }
    capture("stroke_styles", [](const auto& image) {
        for (int index = 0; index < 3; ++index) {
            const int y = 160 + index * 140;
            EXPECT_LT(qAlpha(image.pixel(160, y)), 10)
                << "open path must not draw a closing edge";
            EXPECT_GT(qAlpha(image.pixel(380, y)), 200)
                << "closed path must draw its closing edge";
        }
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, TwentyNestedBooleanBandsAndSnapshotRestore)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    for (int index = 0; index < 20; ++index) {
        auto content = shapeVisualRectangle(64 + index * 8, 64 + index * 8,
                                             384 - index * 16, 384 - index * 16);
        content.merge = index % 2 == 0 ? Artifact::ShapeContentMerge::Add
                                       : Artifact::ShapeContentMerge::Subtract;
        ASSERT_TRUE(addContent(*layer, content));
    }
    const auto snapshot = layer->shapeContentsSnapshot();
    const auto checkBands = [](const auto& image) {
        for (int index = 0; index < 20; ++index) {
            EXPECT_NEAR(qAlpha(image.pixel(68 + index * 8, 256)),
                        index % 2 == 0 ? 255 : 0, 10) << "band=" << index;
        }
        EXPECT_LT(qAlpha(image.pixel(256, 256)), 10);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        return !testing::Test::HasFailure();
    };
    capture("nested_twenty_bands", checkBands);
    ASSERT_FALSE(HasFailure());
    // Mutate an inner subtractor so the image must actually change before
    // restoration. This catches a stale geometry/cache result, too.
    ASSERT_TRUE(layer->removeShapeContentAt(19));
    capture("nested_inner_revealed", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    ASSERT_TRUE(layer->restoreShapeContentsSnapshot(snapshot));
    ASSERT_EQ(layer->shapeContentCount(), 20);
    capture("nested_snapshot_restored", checkBands);
}

TEST_F(ShapeVisualTest, NearTangentBezierOverlapAndGap)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    const auto circle = [](double cx) {
        auto content = shapeVisualRectangle(0, 0, 1, 1);
        constexpr double r = 64;
        constexpr double tangent = r * 0.5522847498307936;
        content.geometry.pathVertices = {
            {{cx, 192}, {-tangent, 0}, {tangent, 0}, true},
            {{cx + r, 256}, {0, -tangent}, {0, tangent}, true},
            {{cx, 320}, {tangent, 0}, {-tangent, 0}, true},
            {{cx - r, 256}, {0, tangent}, {0, -tangent}, true}};
        return content;
    };
    ASSERT_TRUE(addContent(*layer, circle(192)));
    ASSERT_TRUE(addContent(*layer, circle(312)));
    capture("bezier_near_tangent_overlap", [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(252, 256)), 240);
        EXPECT_GT(qAlpha(image.pixel(260, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(256, 170)), 10);
        return !testing::Test::HasFailure();
    });
    ASSERT_FALSE(HasFailure());
    ASSERT_TRUE(layer->setShapeContentAt(1, circle(328)));
    capture("bezier_near_tangent_gap", [](const auto& image) {
        EXPECT_LT(qAlpha(image.pixel(260, 256)), 10);
        EXPECT_GT(qAlpha(image.pixel(192, 256)), 240);
        EXPECT_GT(qAlpha(image.pixel(328, 256)), 240);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, ThirtyTwoTranslucentLayers)
{
    constexpr int count = 32;
    constexpr float opacity = 0.1f;
    for (int index = 0; index < count; ++index) {
        auto* layer = addLayer();
        ASSERT_NE(layer, nullptr);
        auto content = shapeVisualRectangle(64 + index * 2, 96, 256, 320);
        content.fill.color = ArtifactCore::FloatColor(
            index % 2 == 0 ? 1.0f : 0.0f, 0, index % 2 == 0 ? 0.0f : 1.0f,
            opacity);
        ASSERT_TRUE(addContent(*layer, content));
    }
    capture("thirty_two_translucent_layers", [](const auto& image) {
        const double expected = 255.0 * (1.0 - std::pow(0.9, count));
        EXPECT_NEAR(qAlpha(image.pixel(256, 256)), expected, 6);
        // x=65 intersects exactly the first rectangle, while the center
        // intersects all 32: a saturated/opaque fallback cannot pass both.
        EXPECT_NEAR(qAlpha(image.pixel(65, 256)), 25.5, 4);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        EXPECT_GT(qRed(image.pixel(256, 256)), 20);
        EXPECT_GT(qBlue(image.pixel(256, 256)), 20);
        return !testing::Test::HasFailure();
    });
}

TEST_F(ShapeVisualTest, ConcaveRosetteWith256VerticesAndReflection)
{
    auto* layer = addLayer();
    ASSERT_NE(layer, nullptr);
    Artifact::ShapeContent content;
    content.fill.color = ArtifactCore::FloatColor(1, 0, 0, 1);
    content.geometry.pathVertices.reserve(256);
    for (int index = 0; index < 256; ++index) {
        const double angle = index * (6.2831853071795864769 / 256);
        const double radius = index % 2 == 0 ? 180 : 100;
        content.geometry.pathVertices.push_back({
            QPointF(256 + radius * std::cos(angle), 256 + radius * std::sin(angle)),
            {}, {}, false});
    }
    ASSERT_TRUE(addContent(*layer, content));
    const auto check = [](const auto& image) {
        EXPECT_GT(qAlpha(image.pixel(256, 256)), 240);
        EXPECT_LT(qAlpha(image.pixel(16, 16)), 10);
        EXPECT_LT(qAlpha(image.pixel(490, 256)), 10);
        const int pixels = countPixelsMatching(image, [](QRgb p) { return qAlpha(p) > 128; });
        EXPECT_GT(pixels, 20000);
        EXPECT_LT(pixels, 100000);
        return !testing::Test::HasFailure();
    };
    capture("concave_rosette_256", check);
    ASSERT_FALSE(HasFailure());
    content.transform.anchor = QPointF(256, 256);
    content.transform.scale = QPointF(-0.85, 0.65);
    content.transform.rotation = 17;
    ASSERT_TRUE(layer->setShapeContentAt(0, content));
    capture("concave_rosette_reflected", check);
}

} // namespace
