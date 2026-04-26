#ifndef ModeRunAction_h
#define ModeRunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"

class G4Run;
class AnalysisConfig;
class RunAction;
class StageARunAction;

class ModeRunAction : public G4UserRunAction
{
public:
  explicit ModeRunAction(AnalysisConfig *config);
  ~ModeRunAction() override;

  void BeginOfRunAction(const G4Run *run) override;
  void EndOfRunAction(const G4Run *run) override;

  // Accessors for later dispatcher layers
  RunAction *GetStageBRunAction() const;
  StageARunAction *GetStageARunAction() const;

private:
  AnalysisConfig *fConfig;

  RunAction *fStageBRunAction;
  StageARunAction *fStageARunAction;
};

#endif