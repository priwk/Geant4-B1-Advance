#ifndef StageCOpticalSteppingAction_h
#define StageCOpticalSteppingAction_h 1

#include "G4UserSteppingAction.hh"

class G4Step;
class AnalysisConfig;
class StageCOpticalRunAction;
class StageCOpticalPrimaryGeneratorAction;

class StageCOpticalSteppingAction : public G4UserSteppingAction
{
public:
  StageCOpticalSteppingAction(StageCOpticalRunAction *runAction,
                              const StageCOpticalPrimaryGeneratorAction *primaryAction,
                              AnalysisConfig *config);
  ~StageCOpticalSteppingAction() override;

  void UserSteppingAction(const G4Step *step) override;

private:
  AnalysisConfig *fConfig;
  StageCOpticalRunAction *fRunAction;
  const StageCOpticalPrimaryGeneratorAction *fPrimaryAction;
};

#endif
