#ifndef ModePrimaryGeneratorAction_h
#define ModePrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4Event;
class AnalysisConfig;
class PrimaryGeneratorAction;
class StageAPrimaryGeneratorAction;
class StageCOpticalPrimaryGeneratorAction;

class ModePrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
public:
  explicit ModePrimaryGeneratorAction(AnalysisConfig *config);
  ~ModePrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event *event) override;

  // For ModeEventAction / ModeSteppingAction to reuse the existing Stage B chain
  PrimaryGeneratorAction *GetStageBPrimaryAction() const;
  StageCOpticalPrimaryGeneratorAction *GetStageCPrimaryAction();

private:
  AnalysisConfig *fConfig;

  // Stage-specific implementations owned by this dispatcher
  PrimaryGeneratorAction *fStageBPrimary;
  StageAPrimaryGeneratorAction *fStageAPrimary;
  StageCOpticalPrimaryGeneratorAction *fStageCPrimary;
};

#endif
