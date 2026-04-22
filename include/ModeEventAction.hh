#ifndef ModeEventAction_h
#define ModeEventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

class G4Event;
class AnalysisConfig;
class ModeRunAction;
class ModePrimaryGeneratorAction;
class EventAction;

class ModeEventAction : public G4UserEventAction
{
public:
  ModeEventAction(ModeRunAction* modeRunAction,
                  ModePrimaryGeneratorAction* modePrimaryAction,
                  AnalysisConfig* config);
  ~ModeEventAction() override;

  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;

  // For ModeSteppingAction to reuse the existing Stage B event logic
  EventAction* GetStageBEventAction() const;

private:
  AnalysisConfig* fConfig;
  EventAction* fStageBEventAction;
};

#endif