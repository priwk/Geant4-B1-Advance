#include "ModeSteppingAction.hh"

#include "AnalysisConfig.hh"
#include "ModeRunAction.hh"
#include "ModePrimaryGeneratorAction.hh"

#include "EventAction.hh"
#include "PrimaryGeneratorAction.hh"
#include "SteppingAction.hh"
#include "StageARunAction.hh"
#include "StageASteppingAction.hh"
#include "StageCOpticalPrimaryGeneratorAction.hh"
#include "StageCOpticalRunAction.hh"
#include "StageCOpticalSteppingAction.hh"

#include "G4Step.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

ModeSteppingAction::ModeSteppingAction(ModeRunAction *modeRunAction,
                                       AnalysisConfig *config,
                                       ModePrimaryGeneratorAction *modePrimaryAction,
                                       EventAction *stageBEventAction,
                                       PrimaryGeneratorAction *stageBPrimaryAction,
                                       StageCOpticalPrimaryGeneratorAction *stageCPrimaryAction)
    : G4UserSteppingAction(),
      fConfig(config),
      fModePrimaryAction(modePrimaryAction),
      fStageCRunAction(nullptr),
      fStageCPrimaryAction(stageCPrimaryAction),
      fStageBSteppingAction(nullptr),
      fStageASteppingAction(nullptr),
      fStageCSteppingAction(nullptr)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModeSteppingAction::ModeSteppingAction",
                    "BNZS_MODE_STEP_001", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    if (modeRunAction == nullptr)
    {
        G4Exception("ModeSteppingAction::ModeSteppingAction",
                    "BNZS_MODE_STEP_002", FatalException,
                    "ModeRunAction pointer is null.");
        return;
    }

    StageARunAction *stageARunAction = modeRunAction->GetStageARunAction();
    if (stageARunAction == nullptr)
    {
        G4Exception("ModeSteppingAction::ModeSteppingAction",
                    "BNZS_MODE_STEP_003", FatalException,
                    "Stage A run action from ModeRunAction is null.");
        return;
    }

    // Stage A stepping implementation
    fStageASteppingAction = new StageASteppingAction(stageARunAction, fConfig);

    // Stage B stepping implementation
    if (stageBEventAction != nullptr && stageBPrimaryAction != nullptr)
    {
        fStageBSteppingAction = new SteppingAction(stageBEventAction, stageBPrimaryAction);
    }

    fStageCRunAction = modeRunAction->GetStageCRunAction();
    if (fStageCRunAction != nullptr && fStageCPrimaryAction != nullptr)
    {
        fStageCSteppingAction = new StageCOpticalSteppingAction(
            fStageCRunAction,
            fStageCPrimaryAction,
            fConfig);
    }

    G4cout << "[ModeSteppingAction] Dispatcher initialized."
           << " current runMode = "
           << AnalysisConfig::RunModeName(fConfig->runMode)
           << G4endl;
}

ModeSteppingAction::~ModeSteppingAction()
{
    delete fStageASteppingAction;
    delete fStageBSteppingAction;
    delete fStageCSteppingAction;
}

void ModeSteppingAction::UserSteppingAction(const G4Step *step)
{
    if (fConfig == nullptr)
    {
        G4Exception("ModeSteppingAction::UserSteppingAction",
                    "BNZS_MODE_STEP_004", FatalException,
                    "AnalysisConfig pointer is null.");
        return;
    }

    switch (fConfig->runMode)
    {
    case RunMode::StageA_NeutronPatch:
        if (fStageASteppingAction == nullptr)
        {
            G4Exception("ModeSteppingAction::UserSteppingAction",
                        "BNZS_MODE_STEP_005", FatalException,
                        "Stage A stepping action is null.");
            return;
        }
        fStageASteppingAction->UserSteppingAction(step);
        return;

    case RunMode::StageB_ReplayAlphaLi:
        if (fStageBSteppingAction == nullptr)
        {
            G4Exception("ModeSteppingAction::UserSteppingAction",
                        "BNZS_MODE_STEP_006", FatalException,
                        "Stage B stepping action is null.");
            return;
        }
        fStageBSteppingAction->UserSteppingAction(step);
        return;

    case RunMode::StageC_OpticalStub:
        G4Exception("ModeSteppingAction::UserSteppingAction",
                    "BNZS_MODE_STEP_007", FatalException,
                    "RunMode StageC_OpticalStub is selected, but Stage C stepping action is not implemented yet.");
        return;

    case RunMode::StageC_OpticalRVE:
        if (fStageCSteppingAction == nullptr)
        {
            if (fStageCPrimaryAction == nullptr && fModePrimaryAction != nullptr)
            {
                fStageCPrimaryAction = fModePrimaryAction->GetStageCPrimaryAction();
            }

            if (fStageCRunAction != nullptr && fStageCPrimaryAction != nullptr)
            {
                fStageCSteppingAction = new StageCOpticalSteppingAction(
                    fStageCRunAction,
                    fStageCPrimaryAction,
                    fConfig);
            }

            if (fStageCSteppingAction == nullptr)
            {
                G4Exception("ModeSteppingAction::UserSteppingAction",
                            "BNZS_MODE_STEP_009", FatalException,
                            "Stage C optical stepping action is null after lazy initialization.");
                return;
            }
        }
        fStageCSteppingAction->UserSteppingAction(step);
        return;

    default:
        G4Exception("ModeSteppingAction::UserSteppingAction",
                    "BNZS_MODE_STEP_008", FatalException,
                    "Unknown run mode.");
        return;
    }
}
