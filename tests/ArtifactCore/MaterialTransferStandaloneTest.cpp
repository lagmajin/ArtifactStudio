// Standalone opt-in test; deliberately not registered in the application build.
#include "../../ArtifactCore/include/Simulation/MaterialTransfer.hpp"
#include "../../ArtifactCore/include/Simulation/MaterialAttributes.hpp"
#include "../../ArtifactCore/include/Simulation/MaterialTransferLedger.hpp"
#include "../../ArtifactCore/include/Simulation/MaterialDissolve.hpp"
#include "../../ArtifactCore/include/Simulation/MaterialDust.hpp"
#include "../../ArtifactCore/include/Simulation/MaterialTransferTransaction.hpp"

using namespace ArtifactCore::Experimental::MaterialTransfer;

int main() {
    Request request;
    request.connectionId = 1;
    request.occurrenceId = 2;
    request.sourceSpace = request.targetSpace = {3, 0.01f};
    Sample input[2];
    input[0].id = 10;
    input[0].mass = 2;
    input[0].volume = 3;
    input[0].velocity = {4, 5, 6};
    input[1] = input[0];
    input[1].id = 11;
    Spawn output[2];
    output[0].source.id = 999;
    auto result = prepare(request, input, std::span<Spawn>{output, 1});
    if (result.status != Status::Defer || result.consumeSourceAfterCommit ||
        output[0].source.id != 999) return 1;
    input[1].mass = -1;
    result = prepare(request, input, output);
    if (result.status != Status::InvalidInput ||
        output[0].source.id != 999) return 2;
    input[1].mass = 2;
    request.targetSpace.token = 4;
    if (prepare(request, input, output).status != Status::SpaceMismatch) return 3;
    request.targetSpace = request.sourceSpace;
    result = prepare(request, input, output);
    if (result.status != Status::Ready || result.count != 2 ||
        !result.consumeSourceAfterCommit || output[1].source.id != 11 ||
        output[0].source.mass != 2 || output[0].source.volume != 3 ||
        output[0].source.velocity.y != 5) return 4;
    request.mode = Mode::SpawnSecondary;
    result = prepare(request, input, output);
    if (result.status != Status::Ready || result.consumeSourceAfterCommit) return 5;
    result = prepare(request, {}, {});
    if (result.status != Status::Ready || result.count != 0 ||
        result.consumeSourceAfterCommit) return 6;
    Occurrence entries[1];
    TransferLedger ledger{entries};
    const Occurrence key{1, 2, 3};
    if (ledger.probe(key) != LedgerStatus::Available) return 7;
    if (ledger.recordCommitted(key) != LedgerStatus::Available ||
        ledger.probe(key) != LedgerStatus::Duplicate ||
        ledger.probe({1, 3, 3}) != LedgerStatus::Defer) return 8;
    Occurrence saved[1]{ledger.checkpoint()[0]};
    if (!ledger.restore({}) || !ledger.restore(saved) ||
        ledger.probe(key) != LedgerStatus::Duplicate) return 9;
    Occurrence invalid[1]{};
    if (ledger.restore(invalid) || ledger.probe(key) != LedgerStatus::Duplicate) return 10;
    const float damage[2]{0.2f, 0.8f};
    AttributeColumn columns[1]{{100, AttributeOwner::Fragment,
        AttributeUnit::Dimensionless, std::span<const float>{damage}}};
    AttributeView attributes{columns, 2};
    const auto* values = attributes.find<float>(100, AttributeOwner::Fragment,
                                               AttributeUnit::Dimensionless);
    if (!attributes.valid() || !values || (*values)[1] != 0.8f ||
        attributes.find<Vector3>(100, AttributeOwner::Fragment,
                                 AttributeUnit::Dimensionless)) return 11;
    if (AttributeView{columns, 3}.valid()) return 12;
    AttributeColumn duplicates[2]{columns[0], columns[0]};
    if (AttributeView{duplicates, 2}.valid()) return 13;
    request.mode = Mode::Convert;
    input[1].id = input[0].id;
    if (prepare(request, input, output).status != Status::InvalidInput) return 14;
    input[1].id = 11;
    DissolveSample dissolve[2]{{input[0], 5}, {input[1], 10}};
    request.tick = 5;
    result = prepareDissolve(request, dissolve, 0, output);
    if (result.status != Status::Ready || result.count != 1 ||
        output[0].source.id != 10 || output[0].tick != 5 ||
        sourceVisible(dissolve[0], 5) || !sourceVisible(dissolve[1], 5)) return 15;
    request.tick = 10;
    result = prepareDissolve(request, dissolve, 5, output);
    if (result.status != Status::Ready || result.count != 1 ||
        output[0].source.id != 11) return 16;
    output[0].source.id = 999;
    result = prepareDissolve(request, dissolve, 0, std::span<Spawn>{output, 1});
    if (result.status != Status::Defer || output[0].source.id != 999) return 17;
    if (prepareDissolve(request, dissolve, 11, output).status != Status::InvalidInput)
        return 18;
    const std::size_t sourceIndices[3]{1, 0, 1};
    float inherited[3]{-1, -1, -1};
    auto transfer = inheritAttribute<float>(&columns[0], AttributeOwner::Fragment,
        AttributeUnit::Dimensionless, sourceIndices, inherited,
        MissingAttribute::Reject, 0.0f);
    if (transfer != AttributeTransferStatus::Ready || inherited[0] != 0.8f ||
        inherited[1] != 0.2f || inherited[2] != 0.8f) return 19;
    const std::size_t invalidIndices[2]{0, 2};
    inherited[0] = -1;
    transfer = inheritAttribute<float>(&columns[0], AttributeOwner::Fragment,
        AttributeUnit::Dimensionless, invalidIndices, inherited,
        MissingAttribute::Reject, 0.0f);
    if (transfer != AttributeTransferStatus::InvalidInput || inherited[0] != -1) return 20;
    transfer = inheritAttribute<float>(nullptr, AttributeOwner::Fragment,
        AttributeUnit::Dimensionless, sourceIndices, inherited,
        MissingAttribute::UseDefault, 0.5f);
    if (transfer != AttributeTransferStatus::Ready || inherited[2] != 0.5f) return 21;
    transfer = inheritAttribute<float>(nullptr, AttributeOwner::Fragment,
        AttributeUnit::Dimensionless, sourceIndices, inherited,
        MissingAttribute::Reject, 0.5f);
    if (transfer != AttributeTransferStatus::Missing) return 22;
    transfer = inheritAttribute<float>(&columns[0], AttributeOwner::Fragment,
        AttributeUnit::Seconds, sourceIndices, inherited,
        MissingAttribute::UseDefault, 0.5f);
    if (transfer != AttributeTransferStatus::TypeOrUnitMismatch) return 23;
    request.sourceDomain = Domain::Fragment;
    request.targetDomain = Domain::Particle;
    request.mode = Mode::SpawnSecondary;
    const DustSite sites[2]{{{1, 2, 3}, {0, 1, 0}}, {{4, 5, 6}, {0, -1, 0}}};
    DustBudget budget{0.4, 0.2, 0.5f, 1000};
    DustSpawn dust[2];
    result = prepareFragmentDust(request, input[0], budget, sites, dust);
    if (result.status != Status::Ready || result.count != 2 || result.consumeSourceAfterCommit ||
        dust[0].parentFragmentId != 10 || dust[1].particle.source.id != 1001 ||
        dust[0].particle.source.mass != 0.2 || dust[1].particle.source.volume != 0.1 ||
        dust[0].particle.source.velocity.y != 3.5f ||
        dust[1].particle.source.position.x != 4) return 24;
    dust[0].parentFragmentId = 999;
    result = prepareFragmentDust(request, input[0], budget, sites,
                                 std::span<DustSpawn>{dust, 1});
    if (result.status != Status::Defer || dust[0].parentFragmentId != 999) return 25;
    budget.firstChildId = 10;
    if (prepareFragmentDust(request, input[0], budget, sites, dust).status != Status::InvalidInput)
        return 26;
    request.mode = Mode::Convert;
    request.sourceDomain = Domain::Particle;
    request.targetDomain = Domain::Granular;
    bool active[2]{true, true};
    Occurrence transactionEntries[2];
    TransferLedger transactionLedger{transactionEntries};
    const std::size_t selected[2]{0, 1};
    Spawn target[2];
    std::size_t targetSize = 0;
    auto committed = commitConversion(request, input, active, selected,
        std::span<Spawn>{target, 1}, targetSize, transactionLedger);
    if (committed != CommitStatus::Defer || !active[0] || !active[1] ||
        targetSize != 0 || !transactionLedger.checkpoint().empty()) return 27;
    committed = commitConversion(request, input, active, selected,
        target, targetSize, transactionLedger);
    if (committed != CommitStatus::Committed || active[0] || active[1] ||
        targetSize != 2 || target[1].source.id != 11 ||
        transactionLedger.checkpoint().size() != 1) return 28;
    committed = commitConversion(request, input, active, selected,
        target, targetSize, transactionLedger);
    if (committed != CommitStatus::AlreadyCommitted || targetSize != 2) return 29;
    return 0;
}
