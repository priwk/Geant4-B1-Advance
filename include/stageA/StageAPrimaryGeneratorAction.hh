#ifndef StageAPrimaryGeneratorAction_h
#define StageAPrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4ParticleGun;
class G4Event;
class AnalysisConfig;

class StageAPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
public:
  explicit StageAPrimaryGeneratorAction(AnalysisConfig *config);
  ~StageAPrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event *event) override;

private:
  AnalysisConfig *fConfig;
  G4ParticleGun *fParticleGun;
};

#endif