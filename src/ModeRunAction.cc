#include "ModeRunAction.hh"

#include "AnalysisConfig.hh"
#include "RunAction.hh"
#include "StageARunAction.hh"

#include "G4Run.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

ModeRunAction::ModeRunAction(AnalysisConfig *config)
    : G4UserRunAction(),
      fConfig(config),
      fStageBRunAction(nullptr),
      fStageARunAction(nullptr)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModeRunAction::ModeRunAction",
                    "BNZS_MODE_RUN_001", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    // Stage B run action暂时先不依赖primary，后面如需恢复其输入CSV派生细节，
    // 再做一个很小的联动，把Stage B primary指针传进来。
    fStageBRunAction = new RunAction(nullptr, fConfig);
    fStageARunAction = new StageARunAction(fConfig);

    G4cout << "[ModeRunAction] Dispatcher initialized."
           << " current runMode = "
           << AnalysisConfig::RunModeName(fConfig->runMode)
           << G4endl;
}

ModeRunAction::~ModeRunAction()
{
    delete fStageARunAction;
    delete fStageBRunAction;
}

void ModeRunAction::BeginOfRunAction(const G4Run *run)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModeRunAction::BeginOfRunAction",
                    "BNZS_MODE_RUN_002", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    switch (fConfig->runMode)
    {
    case RunMode::StageA_NeutronPatch:
        if (fStageARunAction == nullptr)
        {
            G4Exception("ModeRunAction::BeginOfRunAction",
                        "BNZS_MODE_RUN_003", FatalException,
                        "Stage A run action is null.");
            return;
        }
        fStageARunAction->BeginOfRunAction(run);
        return;

    case RunMode::StageB_ReplayAlphaLi:
        if (fStageBRunAction == nullptr)
        {
            G4Exception("ModeRunAction::BeginOfRunAction",
                        "BNZS_MODE_RUN_004", FatalException,
                        "Stage B run action is null.");
            return;
        }
        fStageBRunAction->BeginOfRunAction(run);
        return;

    case RunMode::StageC_OpticalStub:
        G4Exception("ModeRunAction::BeginOfRunAction",
                    "BNZS_MODE_RUN_005", FatalException,
                    "RunMode StageC_OpticalStub is selected, but Stage C run action is not implemented yet.");
        return;

    default:
        G4Exception("ModeRunAction::BeginOfRunAction",
                    "BNZS_MODE_RUN_006", FatalException,
                    "Unknown run mode.");
        return;
    }
}

void ModeRunAction::EndOfRunAction(const G4Run *run)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModeRunAction::EndOfRunAction",
                    "BNZS_MODE_RUN_007", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    switch (fConfig->runMode)
    {
    case RunMode::StageA_NeutronPatch:
        if (fStageARunAction == nullptr)
        {
            G4Exception("ModeRunAction::EndOfRunAction",
                        "BNZS_MODE_RUN_008", FatalException,
                        "Stage A run action is null.");
            return;
        }
        fStageARunAction->EndOfRunAction(run);
        return;

    case RunMode::StageB_ReplayAlphaLi:
        if (fStageBRunAction == nullptr)
        {
            G4Exception("ModeRunAction::EndOfRunAction",
                        "BNZS_MODE_RUN_009", FatalException,
                        "Stage B run action is null.");
            return;
        }
        fStageBRunAction->EndOfRunAction(run);
        return;

    case RunMode::StageC_OpticalStub:
        G4Exception("ModeRunAction::EndOfRunAction",
                    "BNZS_MODE_RUN_010", FatalException,
                    "RunMode StageC_OpticalStub is selected, but Stage C run action is not implemented yet.");
        return;

    default:
        G4Exception("ModeRunAction::EndOfRunAction",
                    "BNZS_MODE_RUN_011", FatalException,
                    "Unknown run mode.");
        return;
    }
}

RunAction *ModeRunAction::GetStageBRunAction() const
{
    return fStageBRunAction;
}

StageARunAction *ModeRunAction::GetStageARunAction() const
{
    return fStageARunAction;
}