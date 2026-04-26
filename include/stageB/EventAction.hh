#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

#include "PrimaryGeneratorAction.hh"

#include <string>

class G4Event;
class RunAction;
class PrimaryGeneratorAction;

class EventAction : public G4UserEventAction
{
public:
  EventAction(RunAction *runAction, const PrimaryGeneratorAction *primaryAction);
  ~EventAction() override;

  void BeginOfEventAction(const G4Event *event) override;
  void EndOfEventAction(const G4Event *event) override;

  void AddEdep(G4double edep) { fEdep += edep; }

  // ---- cached metadata for current event ----
  const CaptureRecord &GetCurrentRecord() const { return fCurrentRecord; }
  const G4ThreeVector &GetCurrentLocalCapturePosition() const { return fCurrentLocalCapturePosition; }
  const G4ThreeVector &GetCurrentSelectedBNCenter() const { return fCurrentSelectedBNCenter; }
  const std::string &GetCurrentSurfaceMode() const { return fCurrentSurfaceMode; }
  G4double GetCurrentTargetLocalZ() const { return fCurrentTargetLocalZ; }
  G4double GetCurrentUsedLocalZ() const { return fCurrentUsedLocalZ; }

  RunAction *GetRunAction() const { return fRunAction; }

private:
  RunAction *fRunAction;
  const PrimaryGeneratorAction *fPrimaryAction;

  G4double fEdep;

  CaptureRecord fCurrentRecord;
  G4ThreeVector fCurrentLocalCapturePosition;
  G4ThreeVector fCurrentSelectedBNCenter;
  std::string fCurrentSurfaceMode;
  G4double fCurrentTargetLocalZ;
  G4double fCurrentUsedLocalZ;
};

#endif