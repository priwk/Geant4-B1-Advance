#ifndef ModeSteppingAction_h
#define ModeSteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class G4Step;
class AnalysisConfig;
class ModeRunAction;
class ModePrimaryGeneratorAction;
class EventAction;
class PrimaryGeneratorAction;
class SteppingAction;
class StageASteppingAction;
class StageCOpticalRunAction;
class StageCOpticalPrimaryGeneratorAction;
class StageCOpticalSteppingAction;

class ModeSteppingAction : public G4UserSteppingAction
{
public:
  ModeSteppingAction(ModeRunAction *modeRunAction,
                     AnalysisConfig *config,
                     ModePrimaryGeneratorAction *modePrimaryAction,
                     EventAction *stageBEventAction,
                     PrimaryGeneratorAction *stageBPrimaryAction,
                     StageCOpticalPrimaryGeneratorAction *stageCPrimaryAction);
  ~ModeSteppingAction() override;

  void UserSteppingAction(const G4Step *step) override;

private:
  AnalysisConfig *fConfig;
  ModePrimaryGeneratorAction *fModePrimaryAction;
  StageCOpticalRunAction *fStageCRunAction;
  StageCOpticalPrimaryGeneratorAction *fStageCPrimaryAction;

  SteppingAction *fStageBSteppingAction;
  StageASteppingAction *fStageASteppingAction;
  StageCOpticalSteppingAction *fStageCSteppingAction;
};

#endif
