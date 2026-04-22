#ifndef ModeSteppingAction_h
#define ModeSteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class G4Step;
class AnalysisConfig;
class ModeRunAction;
class EventAction;
class PrimaryGeneratorAction;
class SteppingAction;
class StageASteppingAction;

class ModeSteppingAction : public G4UserSteppingAction
{
public:
  ModeSteppingAction(ModeRunAction *modeRunAction,
                     AnalysisConfig *config,
                     EventAction *stageBEventAction,
                     PrimaryGeneratorAction *stageBPrimaryAction);
  ~ModeSteppingAction() override;

  void UserSteppingAction(const G4Step *step) override;

private:
  AnalysisConfig *fConfig;

  SteppingAction *fStageBSteppingAction;
  StageASteppingAction *fStageASteppingAction;
};

#endif