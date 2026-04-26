#include "StageAStackingAction.hh"

#include "AnalysisConfig.hh"

#include "G4Track.hh"
#include "G4ParticleDefinition.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

StageAStackingAction::StageAStackingAction(AnalysisConfig *config)
    : G4UserStackingAction(),
      fConfig(config)
{
    if (fConfig == nullptr)
    {
        G4Exception("StageAStackingAction::StageAStackingAction",
                    "BNZS_A_STACK_001", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    G4cout << "[StageAStackingAction] Initialized: "
           << "secondary neutrons are kept; non-neutron secondaries are killed in Stage A"
           << G4endl;
}

StageAStackingAction::~StageAStackingAction() = default;

G4ClassificationOfNewTrack StageAStackingAction::ClassifyNewTrack(const G4Track *track)
{
    if (track == nullptr)
    {
        return fUrgent;
    }

    const G4ParticleDefinition *particle = track->GetParticleDefinition();
    const G4String particleName = (particle != nullptr) ? particle->GetParticleName() : "unknown";

    // 保留所有 primary
    if (track->GetParentID() == 0)
    {
        return fUrgent;
    }

    // 关键修改：
    // Stage A 允许 secondary neutron 继续输运，
    // 因为 HP 散射后继续传播的中子不能被提前砍掉。
    if (particleName == "neutron")
    {
        return fUrgent;
    }

    // 其它 secondary（alpha / Li / gamma / e- / e+ / 等）全部直接丢弃
    return fKill;
}