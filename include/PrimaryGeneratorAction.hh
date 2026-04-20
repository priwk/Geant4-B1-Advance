#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

#include <fstream>
#include <string>
#include <vector>

class G4Event;

struct CaptureRecord
{
  G4int eventID = -1;
  G4double thickness_um = 0.0;
  G4double bn_wt = 0.0;
  G4double zns_wt = 0.0;
  G4double capture_x_um = 0.0;
  G4double capture_y_um = 0.0;
  G4double corr_x_um = 0.0;
  G4double corr_y_um = 0.0;
  G4double depth_um = 0.0;
};

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event *event) override;

  G4ParticleGun *GetParticleGun() const { return fParticleGun; }

  // ---- current event metadata for later use in EventAction / SteppingAction ----
  const CaptureRecord &GetCurrentRecord() const { return fCurrentRecord; }
  const G4ThreeVector &GetCurrentLocalCapturePosition() const { return fCurrentLocalCapturePosition; }
  const G4ThreeVector &GetCurrentSelectedBNCenter() const { return fCurrentSelectedBNCenter; }
  const std::string &GetCurrentSurfaceMode() const { return fCurrentSurfaceMode; }
  G4double GetCurrentTargetLocalZ() const { return fCurrentTargetLocalZ; }
  G4double GetCurrentUsedLocalZ() const { return fCurrentUsedLocalZ; }
  G4int GetTotalLoadedEvents() const { return static_cast<G4int>(fTotalStreamedRecords); }
  const std::string &GetLoadedInputFile() const { return fCurrentInputFile; }

private:
  // ---- input handling ----
  void InitializeInputStreaming();
  std::vector<std::string> FindInputCsvFiles() const;
  G4bool OpenNextInputFile();
  G4bool ReadNextRecord(CaptureRecord &rec);
  G4bool ReadFirstValidRecordFromFile(const std::string &path, CaptureRecord &rec) const;
  G4bool ParseOneRecordLine(const std::string &line, CaptureRecord &rec) const;
  void ConfigureDetectorFromInput();

  // ---- event classification ----
  std::string DetermineSurfaceMode(const CaptureRecord &rec) const;
  G4double DetermineTargetLocalZ(const CaptureRecord &rec, const std::string &mode) const;

  // ---- BN selection and point sampling ----
  G4bool SelectBNSphereForTargetZ(
      G4double targetZ,
      const std::string &mode,
      G4ThreeVector &chosenCenter,
      G4double &usedZ,
      G4bool &usedFallback) const;

  G4ThreeVector SamplePointInSphereSlice(
      const G4ThreeVector &center,
      G4double zSlice,
      G4double sphereRadius) const;

  // ---- reaction generation ----
  void GenerateReactionProducts(
      G4Event *event,
      const G4ThreeVector &position,
      G4bool useGroundStateBranch) const;

private:
  G4ParticleGun *fParticleGun;

  // ---- multi-file streaming state ----
  std::vector<std::string> fInputFiles;
  std::size_t fCurrentFileIndex = 0;
  std::ifstream fCurrentInputStream;
  std::string fCurrentInputFile;

  CaptureRecord fFirstRecordForGeometry;
  G4bool fHasFirstRecordForGeometry = false;
  G4bool fNoMoreInput = false;
  std::size_t fTotalStreamedRecords = 0;

  // current event cache
  CaptureRecord fCurrentRecord;
  G4ThreeVector fCurrentLocalCapturePosition;
  G4ThreeVector fCurrentSelectedBNCenter;
  std::string fCurrentSurfaceMode;
  G4double fCurrentTargetLocalZ;
  G4double fCurrentUsedLocalZ;
};

#endif