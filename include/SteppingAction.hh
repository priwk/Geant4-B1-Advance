#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class EventAction;
class PrimaryGeneratorAction;
class G4Step;

class SteppingAction : public G4UserSteppingAction
{
public:
  SteppingAction(EventAction *eventAction,
                 const PrimaryGeneratorAction *primaryAction);
  ~SteppingAction() override;

  void UserSteppingAction(const G4Step *step) override;

private:
  EventAction *fEventAction;
  const PrimaryGeneratorAction *fPrimaryAction;
};

#endif