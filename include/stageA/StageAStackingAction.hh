#ifndef StageAStackingAction_h
#define StageAStackingAction_h 1

#include "G4UserStackingAction.hh"
#include "globals.hh"

class G4Track;
class AnalysisConfig;

class StageAStackingAction : public G4UserStackingAction
{
public:
  explicit StageAStackingAction(AnalysisConfig *config);
  ~StageAStackingAction() override;

  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track *track) override;

private:
  AnalysisConfig *fConfig;
};

#endif