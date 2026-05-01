#ifndef StageCOpticalPrimaryGeneratorAction_h
#define StageCOpticalPrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "StageCOpticalSource.hh"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class G4Event;
class AnalysisConfig;

class StageCOpticalPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
public:
  explicit StageCOpticalPrimaryGeneratorAction(AnalysisConfig *config);
  ~StageCOpticalPrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event *event) override;

  const StageCPhotonRecord &GetCurrentPhotonRecord() const { return fCurrentPhoton; }
  const std::string &GetLoadedSourceFile() const { return fSourceCsvPath; }
  G4long GetLoadedSourceSteps() const { return fLoadedSourceSteps; }
  G4long GetGeneratedPhotons() const { return fGeneratedPhotons; }
  G4double GetGeneratedPhotonWeight() const { return fGeneratedPhotonWeight; }

private:
  void OpenSourceCsv();
  void ReadHeader();
  G4bool ReadNextSourceRecord(StageCSourceRecord &record);
  G4bool FillSourceRecord(const std::string &line, StageCSourceRecord &record) const;
  void RequireSinglePlacement(const StageCSourceRecord &record);
  std::string Field(const std::vector<std::string> &fields, const std::string &name) const;
  std::string OptionalField(const std::vector<std::string> &fields,
                            const std::string &name,
                            const std::string &fallback) const;
  G4double DoubleField(const std::vector<std::string> &fields, const std::string &name) const;
  G4double OptionalDoubleField(const std::vector<std::string> &fields,
                               const std::string &name,
                               G4double fallback) const;
  G4int IntField(const std::vector<std::string> &fields, const std::string &name) const;

  G4ThreeVector SampleSourcePosition(const StageCSourceRecord &record) const;
  G4ThreeVector RandomUnitVector() const;
  G4ThreeVector RandomPolarization(const G4ThreeVector &direction) const;

private:
  AnalysisConfig *fConfig;
  G4ParticleGun *fParticleGun;

  std::string fSourceCsvPath;
  std::ifstream fSourceCsv;
  std::unordered_map<std::string, std::size_t> fHeaderIndex;

  StageCSourceRecord fCurrentSource;
  StageCPhotonRecord fCurrentPhoton;
  G4bool fHasCurrentSource;
  G4int fNextSampleIndex;
  G4int fSamplesPerStep;
  std::string fSourceSampling;
  std::string fExpectedPlacementFile;

  G4long fLoadedSourceSteps;
  G4long fGeneratedPhotons;
  G4double fGeneratedPhotonWeight;
};

#endif
