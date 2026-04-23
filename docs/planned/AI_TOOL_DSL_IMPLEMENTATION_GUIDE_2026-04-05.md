# AI Tool DSL Implementation Guide

**Status**: Architecture Defined - Partial Implementation In Progress  
**Date**: 2026-04-05  
**Module**: `Artifact/Tool/AIDSL`

---

## 1. Module Structure

```
Artifact/
└── src/
    └── Tool/
        └── AIDSL/
            ├── include/
            │   └── AIToolDSL/
            │       ├── DSL.h              // Public API (non-module header)
            │       ├── DSLParser.h
            │       ├── DSLExecutor.h
            │       ├── DSLExpression.h
            │       └── Integration.h       // Host app hooks
            ├── src/
            │   ├── DSLParser.cppm
            │   ├── DSLExecutor.cppm
            │   ├── DSLExpression.cppm
            │   └── Integration.cppm
            └── tests/
                └── DSLTest.cpp
```

**Note**: Due to CMake/Visual Studio integration issues with modules for new code, start with traditional header/implementation (.h/.cpp) and migrate to modules later.

---

## 2. Core Public API (`DSL.h`)

Simplified interface for AI tool integration:

```cpp
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>

// Forward declarations from Artifact core
struct ArtifactAbstractLayer;
struct ArtifactAbstractComposition;
class ArtifactCompositionRenderController;

namespace AIToolDSL {

enum class DSLStatus { Ok, Error, DryRun };

struct DSLResult {
    DSLStatus status;
    std::string message;     // Human-readable
    std::string json;        // Machine-readable JSON
    int affectedCount = 0;   // Number of layers/operations
};

using Value = std::variant<bool, int, double, std::string, std::vector<double>>;

class Interpreter {
public:
    using LayerFilter = std::function<std::vector<ArtifactAbstractLayer*>(const std::string& filterExpr)>;
    using CompGetter = std::function<ArtifactAbstractComposition*()>;
    using PropertyAccessor = std::function<Value(ArtifactAbstractLayer*, const std::string&)>;
    using PropertyMutator = std::function<bool(ArtifactAbstractLayer*, const std::string&, const Value&)>;
    using KeyframeWriter = std::function<bool(ArtifactAbstractLayer*, const std::string&, int frame, const Value&)>;
    using UndoableAction = std::function<void()>;  // reversible operation

    Interpreter();
    ~Interpreter();

    // Host app sets these callbacks
    void setCompGetter(CompGetter getter);
    void setLayerFilter(LayerFilter filter);
    void setPropertyAccessor(PropertyAccessor accessor);
    void setPropertyMutator(PropertyMutator mutator);
    void setKeyframeWriter(KeyframeWriter writer);
    void setUndoStack(std::vector<UndoableAction>* stack);  // external undo management

    // Parse and optionally execute
    DSLResult parse(const std::string& script);
    DSLResult execute(const std::string& script);
    DSLResult dryRun(const std::string& script);  // Calculate effects without applying

    // Undo/redo (if internal stack used)
    bool undo();
    bool redo();
    bool canUndo() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Host callbacks (all set by CompositionRenderController or similar)
    CompGetter compGetter_;
    LayerFilter layerFilter_;
    PropertyAccessor propertyAccessor_;
    PropertyMutator propertyMutator_;
    KeyframeWriter keyframeWriter_;
    std::vector<UndoableAction>* undoStack_ = nullptr;
};

} // namespace AIToolDSL
```

---

## 3. Integration with `CompositionRenderController`

Add DSL interpreter to `CompositionRenderController`:

```cpp
// In ArtifactCompositionRenderController.h
#include "AIToolDSL/DSL.h"

class CompositionRenderController {
private:
    std::unique_ptr<AIToolDSL::Interpreter> dslInterpreter_;
};

// In ArtifactCompositionRenderController.cppm (constructor)
CompositionRenderController::CompositionRenderController(...) {
    impl_->dslInterpreter_ = std::make_unique<AIToolDSL::Interpreter>();

    // Install callbacks to real layer/composition APIs
    impl_->dslInterpreter_->setCompGetter([this]() {
        return this->previewPipeline_.composition();
    });

    impl_->dslInterpreter_->setLayerFilter([this](const std::string& filter) {
        // Parse simple filter and return matching layers
        // For now, support: type=="Text", opacity>0.5, name~="*"
        std::vector<ArtifactAbstractLayer*> result;
        auto comp = previewPipeline_.composition();
        if (!comp) return result;
        auto all = comp->allLayers();
        for (auto& layer : all) {
            if (!layer) continue;
            // Very simple filter parser; move to DSLExpression module later
            if (filter == "type==\"Text\"") {
                if (layer->className() == "TextLayer") result.push_back(layer.get());
            } else if (filter.starts_with("opacity>")) {
                float threshold = std::stof(filter.substr(8));
                if (layer->opacity() > threshold) result.push_back(layer.get());
            } else if (filter.starts_with("name~=")) {
                // glob pattern
                // ...
            }
        }
        return result;
    });

    impl_->dslInterpreter_->setPropertyMutator([this](ArtifactAbstractLayer* layer, const std::string& path, const AIToolDSL::Value& value) -> bool {
        if (!layer) return false;
        // Convert Value to QVariant
        QVariant var;
        std::visit([&var](auto&& arg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, bool>) var = arg;
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, int64_t>) var = static_cast<int>(arg);
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, double>) var = arg;
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::string>) var = QString::fromStdString(arg);
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::vector<double>>) {
                // Convert to QPointF/QSizeF/etc based on property path
                // Placeholder: just store as QVariantList
                QVariantList list;
                for (double d : arg) list.append(d);
                var = list;
            }
        }, value);

        // Use existing layer property setter
        bool changed = layer->setLayerPropertyValue(QString::fromStdString(path), var);
        if (changed) {
            // Trigger existing mutation system
            layer->changed();
            // Invalidate render cache (existing code path)
            impl_->invalidateLayerSurfaceCache(layer);
            impl_->invalidateBaseComposite();
            renderOneFrame();
        }
        return changed;
    });

    impl_->dslInterpreter_->setKeyframeWriter([this](ArtifactAbstractLayer* layer, const std::string& path, int frame, const AIToolDSL::Value& value) -> bool {
        // Use existing property/keyframe APIs
        // ...
        return true;
    });
}

// Add public method to controller for AI tool access
DSLResult CompositionRenderController::executeAIToolCommand(const std::string& dsl) {
    if (!impl_->dslInterpreter_) {
        return {DSLStatus::Error, "AI DSL not initialized", "", 0};
    }
    return impl_->dslInterpreter_->execute(dsl);
}

DSLResult CompositionRenderController::dryRunAIToolCommand(const std::string& dsl) {
    if (!impl_->dslInterpreter_) {
        return {DSLStatus::Error, "AI DSL not initialized", "", 0};
    }
    return impl_->dslInterpreter_->dryRun(dsl);
}
```

---

## 4. Parser Implementation (Simplified)

`DSLParser.cppm` - recognizes these patterns:

```
use comp "Main"
begin_transaction "Batch name"
  select layers where [filter]
  set [property] = [value]
  add key at [frame] [property] = [value]
  rename selected with "pattern_{index}"
  delete selected
  group layers into "Group Name"
end_transaction

query selected_layers
query comp_size
query find layers where [filter]
query describe layer "Name"
query list properties of selected
```

Simplified tokenizer (whitespace + quoted strings). For each line:

1. Split into tokens
2. Look at first token to determine command type
3. Parse remainder into command struct
4. Validate property paths against known list

---

## 5. Filter Expression Parser

Support simplified boolean expressions:

```
type == "Text"
opacity > 0.5
name ~= "title_*"
visible == true
```

Implementation:

```cpp
// In DSLExpression.cppm
struct FilterExpr {
    virtual ~FilterExpr() = default;
    virtual bool matches(const ArtifactAbstractLayer* layer) const = 0;
};

class SimpleFilterParser {
public:
    std::unique_ptr<FilterExpr> parse(const std::string& expr);

private:
    // Tokenize and build AST
    // Supported ops: ==, !=, >, <, >=, <=, ~= (glob)
};
```

For MVP, skip full AST and use a simple lambda generation:

```cpp
auto filter = [&](const std::string& expr) -> std::function<bool(ArtifactAbstractLayer*)> {
    if (expr == "type==\"Text\"") {
        return [](auto* l) { return l->className() == "TextLayer"; };
    }
    // Add more patterns as needed
    return [](auto*) { return true; };
};
```

---

## 6. Dry-Run Implementation

`dryRun()` does NOT execute mutations. Instead:

1. Parse script → AST
2. For each command, simulate and count affected layers
3. Return JSON summary:

```json
{
  "dry_run": true,
  "would_affect": 3,
  "operations": [
    "Layer 'Title' (L12): set opacity 1.0 -> 0.5",
    "Layer 'Subtitle' (L15): set opacity 1.0 -> 0.5"
  ],
  "requires_confirmation": true
}
```

Implementation:

```cpp
DSLResult Interpreter::dryRun(const std::string& script) {
    auto ast = parse(script);
    if (!ast.success) return {DSLStatus::Error, ast.error, "", 0};

    int count = 0;
    std::vector<std::string> ops;
    for (auto& cmd : ast.commands) {
        auto preview = cmd->dryRun();  // each command estimates impact
        count += preview.affectedLayers;
        ops.insert(ops.end(), preview.messages.begin(), preview.messages.end());
    }
    Json::Object json;
    json["dry_run"] = true;
    json["would_affect"] = count;
    json["operations"] = ops;
    return {DSLStatus::Ok, "Dry run: " + std::to_string(count) + " layers affected", json.dump(), count};
}
```

---

## 7. Transaction & Undo

- `begin_transaction "name"` ... `end_transaction` blocks are compiled into a single `TransactionAction` that holds multiple sub-actions.
- Each mutation command (`set`, `add key`, `rename`, `delete`) creates an `UndoableAction` (lambda) that captures previous state.
- `undo()` pops from stack and executes the inverse.

Example:

```cpp
struct SetPropertyAction : Action {
    LayerID layer;
    PropertyPath path;
    Value oldValue;
    Value newValue;
    void execute() override { propertySetter_(layer, path, newValue); }
    void undo() override { propertySetter_(layer, path, oldValue); }
};
```

Before `execute()`, call `propertyGetter_` to capture `oldValue`.

---

## 8. Query Results JSON Format

```json
{
  "query": "selected_layers",
  "status": "ok",
  "result": {
    "layers": [
      {
        "id": "L12",
        "name": "Title",
        "type": "Text",
        "comp": "Main",
        "visible": true,
        "opacity": 0.9,
        "transform": { "position": [960, 540], "scale": [1, 1], "rotation": 0 },
        "properties": [
          { "path": "layer.opacity", "value": 0.9, "animatable": true },
          { "path": "text.font_size", "value": 96 }
        ]
      }
    ],
    "count": 1
  }
}
```

Helper: serialize `Value` to JSON (bool, number, string, array).

---

## 9. Error Handling

DSL parser should produce line/column info:

```json
{
  "error": "SYNTAX_ERROR",
  "line": 3,
  "column": 12,
  "message": "Expected '=' after property name"
}
```

At runtime (e.g., `set` on non-existent property):

```json
{
  "error": "PROPERTY_NOT_FOUND",
  "layer": "L12",
  "property": "fill_color",
  "suggestions": ["color", "opacity", "transform.position.x"]
}
```

---

## 10. Testing

Unit tests for:

- Parser: each command line pattern
- Filter: `type=="Text"`, `opacity>0.5`, `name~="bg_*"`
- Dry-run counting
- Undo/redo sequence
- Error cases: ambiguous select, unknown property, missing comp

Example test:

```cpp
TEST(AIToolDSLTest, ParseSelectSet) {
    Interpreter interp;
    auto result = interp.dryRun(R"(use comp "Main"
select layers where type=="Text"
set opacity = 0.5)");
    EXPECT_EQ(result.status, DSLStatus::Ok);
    EXPECT_GT(result.affectedCount, 0);
}
```

---

## 11. Host App Integration Checklist

- [ ] Add `AIToolDSL` subdirectory to CMakeLists.txt
- [ ] Include `AIToolDSL.h` in `CompositionRenderController.h`
- [ ] Initialize `dslInterpreter_` in constructor with host callbacks
- [ ] Expose public slots: `executeAIToolCommand(QString)` and `dryRunAIToolCommand(QString)`
- [ ] Wire undo/redo to existing `UndoManager` if available
- [ ] Add AI Tool panel UI with text input and output log (optional)
- [ ] Register available property paths (query `list properties`) for AI validation
- [ ] Implement `layerFilter_` to support `type=="Text"`, `opacity`, `visible`, `name` patterns
- [ ] Test with sample scripts:

```
use comp "Main"
select layers where type=="Text"
set opacity = 0.6
```

---

## 12. Performance Notes

- DSL parsing overhead: negligible (<1ms for typical scripts)
- Filter evaluation: O(N) layers per query; cache filter results if reused
- Dry-run should avoid calling host mutators; use `propertyGetter_` to estimate effects

---

## 13. Future Enhancements

- Full AST with expression optimization
- `for each` loops and conditional `if` statements
- Python-like syntax alternative (more expressive)
- Caching of compiled scripts for repeated AI use
- Integration with AI context manager (conversation history)

---

## 14. Sample Integration Code (CompositionRenderController)

```cpp
// ArtifactCompositionRenderController.cppm (additions)

#include "AIToolDSL/DSL.h"

void CompositionRenderController::initDSL() {
    if (!impl_->dslInterpreter_) {
        impl_->dslInterpreter_ = std::make_unique<AIToolDSL::Interpreter>();
    }

    auto* comp = previewPipeline_.composition();

    impl_->dslInterpreter_->setCompGetter([compPtr = comp]() {
        return compPtr;
    });

    impl_->dslInterpreter_->setLayerFilter([this, compPtr = comp](const std::string& filter) {
        std::vector<ArtifactAbstractLayer*> matches;
        if (!compPtr) return matches;

        // Simplified: use existing layer lookup
        auto all = compPtr->allLayers();
        for (auto& layer : all) {
            if (!layer) continue;

            // Check filter - stub
            if (filter.find("type==") != std::string::npos) {
                std::string type = filter.substr(6, filter.size() - 7); // strip quotes
                if (layer->className() == type) {
                    matches.push_back(layer.get());
                }
            } else if (filter.find("opacity>") != std::string::npos) {
                float thresh = std::stof(filter.substr(8));
                if (layer->opacity() > thresh) {
                    matches.push_back(layer.get());
                }
            } else if (filter == "visible==true") {
                if (layer->isVisible()) {
                    matches.push_back(layer.get());
                }
            }
        }
        return matches;
    });

    impl_->dslInterpreter_->setPropertyMutator([this](ArtifactAbstractLayer* layer, const std::string& path, const AIToolDSL::Value& val) -> bool {
        QString propPath = QString::fromStdString(path);
        QVariant qval;
        // Convert Value to QVariant (simplified)
        if (auto* b = std::get_if<bool>(&val)) qval = *b;
        else if (auto* i = std::get_if<int64_t>(&val)) qval = static_cast<int>(*i);
        else if (auto* d = std::get_if<double>(&val)) qval = *d;
        else if (auto* s = std::get_if<std::string>(&val)) qval = QString::fromStdString(*s);
        else if (auto* vec = std::get_if<std::vector<double>>(&val)) {
            QVariantList list;
            for (double d : *vec) list.append(d);
            qval = list;
        }

        bool changed = layer->setLayerPropertyValue(propPath, qval);
        if (changed) {
            // Notify existing system
            layer->changed();
            impl_->invalidateLayerSurfaceCache(layer);
            impl_->invalidateBaseComposite();
            renderOneFrame();
        }
        return changed;
    });
}

DSLResult CompositionRenderController::executeAICommand(const std::string& dsl) {
    if (!impl_->dslInterpreter_) {
        initDSL();
    }
    return impl_->dslInterpreter_->execute(dsl);
}
```

---

This guide provides a complete roadmap for implementing AI Tool DSL with minimal disruption to existing code.
