#ifndef ActionInitialization_h
#define ActionInitialization_h 1

#include "G4VUserActionInitialization.hh"

class AnalysisConfig;

class ActionInitialization : public G4VUserActionInitialization
{
public:
  explicit ActionInitialization(AnalysisConfig *config);
  ~ActionInitialization() override;

  void BuildForMaster() const override;
  void Build() const override;

private:
  AnalysisConfig *fConfig;
};

#endif