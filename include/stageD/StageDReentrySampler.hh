#ifndef StageDReentrySampler_h
#define StageDReentrySampler_h 1

#include "DetectorConstruction.hh"
#include "G4ThreeVector.hh"

class StageDReentrySampler
{
public:
  explicit StageDReentrySampler(const DetectorConstruction *detector);

  G4bool SampleSamePhaseSphereReentry(DetectorConstruction::Phase phase,
                                      const G4ThreeVector &positionBeforeExit,
                                      const std::string &reentryMode,
                                      G4ThreeVector &newPosition) const;

  G4bool SampleMatrixReentry(const std::string &matrixMode,
                             G4ThreeVector &newPosition) const;

private:
  const DetectorConstruction::SphereInfo *FindContainingOrNearestSphere(
      DetectorConstruction::Phase phase,
      const G4ThreeVector &position) const;

  G4ThreeVector RandomUnitVector() const;
  G4ThreeVector RandomPointInMatrixBox() const;
  G4ThreeVector NudgeInsidePhase(const G4ThreeVector &candidate,
                                 DetectorConstruction::Phase phase) const;

private:
  const DetectorConstruction *fDetector;
};

#endif
