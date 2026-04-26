#include "EventAction.hh"

#include "RunAction.hh"
#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4ios.hh"
#include "G4SystemOfUnits.hh"

// --------------------------------------------------------------------

EventAction::EventAction(RunAction *runAction,
                         const PrimaryGeneratorAction *primaryAction)
    : G4UserEventAction(),
      fRunAction(runAction),
      fPrimaryAction(primaryAction),
      fEdep(0.0),
      fCurrentRecord(),
      fCurrentLocalCapturePosition(),
      fCurrentSelectedBNCenter(),
      fCurrentSurfaceMode(""),
      fCurrentTargetLocalZ(0.0),
      fCurrentUsedLocalZ(0.0)
{
}

// --------------------------------------------------------------------

EventAction::~EventAction() = default;

// --------------------------------------------------------------------

void EventAction::BeginOfEventAction(const G4Event *event)
{
  (void)event;

  fEdep = 0.0;

  if (fPrimaryAction)
  {
    fCurrentRecord = fPrimaryAction->GetCurrentRecord();
    fCurrentLocalCapturePosition = fPrimaryAction->GetCurrentLocalCapturePosition();
    fCurrentSelectedBNCenter = fPrimaryAction->GetCurrentSelectedBNCenter();
    fCurrentSurfaceMode = fPrimaryAction->GetCurrentSurfaceMode();
    fCurrentTargetLocalZ = fPrimaryAction->GetCurrentTargetLocalZ();
    fCurrentUsedLocalZ = fPrimaryAction->GetCurrentUsedLocalZ();
  }
  else
  {
    fCurrentRecord = CaptureRecord{};
    fCurrentLocalCapturePosition = G4ThreeVector();
    fCurrentSelectedBNCenter = G4ThreeVector();
    fCurrentSurfaceMode.clear();
    fCurrentTargetLocalZ = 0.0;
    fCurrentUsedLocalZ = 0.0;
  }
}

// --------------------------------------------------------------------

void EventAction::EndOfEventAction(const G4Event *event)
{
  // keep quiet for production; useful summary hook left here
  if (event->GetEventID() < 3)
  {
    G4cout
        << "[EventAction] End event " << event->GetEventID()
        << "  input eventID=" << fCurrentRecord.eventID
        << "  mode=" << fCurrentSurfaceMode
        << "  total edep=" << fEdep / keV << " keV"
        << G4endl;
  }
}