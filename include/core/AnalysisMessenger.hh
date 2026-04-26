#ifndef AnalysisMessenger_h
#define AnalysisMessenger_h 1

#include "G4UImessenger.hh"

class AnalysisConfig;
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithABool;

class AnalysisMessenger : public G4UImessenger
{
public:
  explicit AnalysisMessenger(AnalysisConfig *config);
  ~AnalysisMessenger() override;

  void SetNewValue(G4UIcommand *command, G4String newValue) override;

private:
  AnalysisConfig *fConfig;

  G4UIdirectory *fCfgDir;
  G4UIcmdWithAString *fRunModeCmd;
  G4UIcmdWithAString *fCaptureCsvCmd;
  G4UIcmdWithAString *fPlacementFileCmd;
  G4UIcmdWithABool *fUseRandomPlacementCmd;
  G4UIcmdWithABool *fAllowThicknessEqualCmd;
  G4UIcommand *fWeightRatioCmd;
};

#endif
