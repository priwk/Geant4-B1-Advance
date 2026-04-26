#include "ModeEventAction.hh"

#include "AnalysisConfig.hh"
#include "ModeRunAction.hh"
#include "ModePrimaryGeneratorAction.hh"

#include "RunAction.hh"
#include "PrimaryGeneratorAction.hh"
#include "EventAction.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4ios.hh"

ModeEventAction::ModeEventAction(ModeRunAction *modeRunAction,
                                 ModePrimaryGeneratorAction *modePrimaryAction,
                                 AnalysisConfig *config)
    : G4UserEventAction(),
      fConfig(config),
      fStageBEventAction(nullptr)
{
  if (fConfig == nullptr)
  {
    G4Exception("ModeEventAction::ModeEventAction",
                "BNZS_MODE_EVT_001", FatalException,
                "AnalysisConfig pointer is null.");
    return;
  }

  if (modeRunAction == nullptr)
  {
    G4Exception("ModeEventAction::ModeEventAction",
                "BNZS_MODE_EVT_002", FatalException,
                "ModeRunAction pointer is null.");
    return;
  }

  if (modePrimaryAction == nullptr)
  {
    G4Exception("ModeEventAction::ModeEventAction",
                "BNZS_MODE_EVT_003", FatalException,
                "ModePrimaryGeneratorAction pointer is null.");
    return;
  }

  // Reuse the existing Stage B event implementation
  RunAction *stageBRunAction = modeRunAction->GetStageBRunAction();
  PrimaryGeneratorAction *stageBPrimaryAction = modePrimaryAction->GetStageBPrimaryAction();

  if (stageBRunAction != nullptr && stageBPrimaryAction != nullptr)
  {
    fStageBEventAction = new EventAction(stageBRunAction, stageBPrimaryAction);
  }

  G4cout << "[ModeEventAction] Dispatcher initialized."
         << " current runMode = "
         << AnalysisConfig::RunModeName(fConfig->runMode)
         << G4endl;
}

ModeEventAction::~ModeEventAction()
{
  delete fStageBEventAction;
}

void ModeEventAction::BeginOfEventAction(const G4Event *event)
{
  if (fConfig == nullptr)
  {
    G4Exception("ModeEventAction::BeginOfEventAction",
                "BNZS_MODE_EVT_004", FatalException,
                "AnalysisConfig pointer is null.");
    return;
  }

  switch (fConfig->runMode)
  {
  case RunMode::StageA_NeutronPatch:
    // Stage A currently does not need a dedicated event-layer implementation.
    return;

  case RunMode::StageB_ReplayAlphaLi:
    if (fStageBEventAction == nullptr)
    {
      G4Exception("ModeEventAction::BeginOfEventAction",
                  "BNZS_MODE_EVT_005", FatalException,
                  "Stage B event action is null.");
      return;
    }
    fStageBEventAction->BeginOfEventAction(event);
    return;

  case RunMode::StageC_OpticalStub:
    G4Exception("ModeEventAction::BeginOfEventAction",
                "BNZS_MODE_EVT_006", FatalException,
                "RunMode StageC_OpticalStub is selected, but Stage C event action is not implemented yet.");
    return;

  default:
    G4Exception("ModeEventAction::BeginOfEventAction",
                "BNZS_MODE_EVT_007", FatalException,
                "Unknown run mode.");
    return;
  }
}

void ModeEventAction::EndOfEventAction(const G4Event *event)
{
  if (fConfig == nullptr)
  {
    G4Exception("ModeEventAction::EndOfEventAction",
                "BNZS_MODE_EVT_008", FatalException,
                "AnalysisConfig pointer is null.");
    return;
  }

  switch (fConfig->runMode)
  {
  case RunMode::StageA_NeutronPatch:
    // Stage A currently does not need a dedicated event-layer implementation.
    return;

  case RunMode::StageB_ReplayAlphaLi:
    if (fStageBEventAction == nullptr)
    {
      G4Exception("ModeEventAction::EndOfEventAction",
                  "BNZS_MODE_EVT_009", FatalException,
                  "Stage B event action is null.");
      return;
    }
    fStageBEventAction->EndOfEventAction(event);
    return;

  case RunMode::StageC_OpticalStub:
    G4Exception("ModeEventAction::EndOfEventAction",
                "BNZS_MODE_EVT_010", FatalException,
                "RunMode StageC_OpticalStub is selected, but Stage C event action is not implemented yet.");
    return;

  default:
    G4Exception("ModeEventAction::EndOfEventAction",
                "BNZS_MODE_EVT_011", FatalException,
                "Unknown run mode.");
    return;
  }
}

EventAction *ModeEventAction::GetStageBEventAction() const
{
  return fStageBEventAction;
}