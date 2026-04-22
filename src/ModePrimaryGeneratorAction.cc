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

    // 两套实现都先建好，运行时按 runMode 转发。
    // 这样 /cfg/setRunMode 在 .mac 中修改后，GeneratePrimaries() 能立即感知。
    fStageBPrimary = new PrimaryGeneratorAction(fConfig);
    fStageAPrimary = new StageAPrimaryGeneratorAction(fConfig);

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
            G4Exception("ModePrimaryGeneratorAction::GeneratePrimaries",
                        "BNZS_MODE_PRI_003", FatalException,
                        "Stage A primary generator is null.");
            return;
        }
        fStageAPrimary->GeneratePrimaries(event);
        return;

    case RunMode::StageB_ReplayAlphaLi:
        if (fStageBPrimary == nullptr)
        {
            G4Exception("ModePrimaryGeneratorAction::GeneratePrimaries",
                        "BNZS_MODE_PRI_004", FatalException,
                        "Stage B primary generator is null.");
            return;
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