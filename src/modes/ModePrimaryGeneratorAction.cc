#include "ModePrimaryGeneratorAction.hh"

#include "AnalysisConfig.hh"
#include "PrimaryGeneratorAction.hh"
#include "StageAPrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

ModePrimaryGeneratorAction::ModePrimaryGeneratorAction(AnalysisConfig *config)
    : G4VUserPrimaryGeneratorAction(),
      fConfig(config),
      fStageBPrimary(nullptr),
      fStageAPrimary(nullptr)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModePrimaryGeneratorAction::ModePrimaryGeneratorAction",
                    "BNZS_MODE_PRI_001", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    // Build only the primary generator needed for the selected mode.
    // Stage B opens capture CSV files in its constructor, so constructing it
    // during Stage A would make neutron-only batches depend on replay inputs.
    if (fConfig->runMode == RunMode::StageA_NeutronPatch)
    {
        fStageAPrimary = new StageAPrimaryGeneratorAction(fConfig);
    }
    else if (fConfig->runMode == RunMode::StageB_ReplayAlphaLi)
    {
        fStageBPrimary = new PrimaryGeneratorAction(fConfig);
    }

    G4cout << "[ModePrimaryGeneratorAction] Dispatcher initialized."
           << " current runMode = "
           << AnalysisConfig::RunModeName(fConfig->runMode)
           << G4endl;
}

ModePrimaryGeneratorAction::~ModePrimaryGeneratorAction()
{
    delete fStageAPrimary;
    delete fStageBPrimary;
}

void ModePrimaryGeneratorAction::GeneratePrimaries(G4Event *event)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModePrimaryGeneratorAction::GeneratePrimaries",
                    "BNZS_MODE_PRI_002", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    switch (fConfig->runMode)
    {
    case RunMode::StageA_NeutronPatch:
        if (fStageAPrimary == nullptr)
        {
            fStageAPrimary = new StageAPrimaryGeneratorAction(fConfig);
        }
        fStageAPrimary->GeneratePrimaries(event);
        return;

    case RunMode::StageB_ReplayAlphaLi:
        if (fStageBPrimary == nullptr)
        {
            fStageBPrimary = new PrimaryGeneratorAction(fConfig);
        }
        fStageBPrimary->GeneratePrimaries(event);
        return;

    case RunMode::StageC_OpticalStub:
        G4Exception("ModePrimaryGeneratorAction::GeneratePrimaries",
                    "BNZS_MODE_PRI_005", FatalException,
                    "RunMode StageC_OpticalStub is selected, but Stage C primary generator is not implemented yet.");
        return;

    default:
        G4Exception("ModePrimaryGeneratorAction::GeneratePrimaries",
                    "BNZS_MODE_PRI_006", FatalException,
                    "Unknown run mode.");
        return;
    }
}

PrimaryGeneratorAction *ModePrimaryGeneratorAction::GetStageBPrimaryAction() const
{
    return fStageBPrimary;
}
