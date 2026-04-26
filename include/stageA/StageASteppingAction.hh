#ifndef StageASteppingAction_h
#define StageASteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

class G4Step;
class AnalysisConfig;
class StageARunAction;

class StageASteppingAction : public G4UserSteppingAction
{
public:
  StageASteppingAction(StageARunAction *runAction, AnalysisConfig *config);
  ~StageASteppingAction() override;

  void UserSteppingAction(const G4Step *step) override;

private:
  G4bool IsPrimaryNeutronStep(const G4Step *step) const;
  G4bool IsPointInsidePatch(const G4ThreeVector &p) const;

private:
  StageARunAction *fRunAction;
  AnalysisConfig *fConfig;
};

#endif