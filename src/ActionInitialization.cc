#include "ActionInitialization.hh"

#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"

ActionInitialization::ActionInitialization()
    : G4VUserActionInitialization()
{
}

ActionInitialization::~ActionInitialization() = default;

void ActionInitialization::BuildForMaster() const
{
  auto *runAction = new RunAction(nullptr);
  SetUserAction(runAction);
}

void ActionInitialization::Build() const
{
  auto *primaryAction = new PrimaryGeneratorAction();
  SetUserAction(primaryAction);

  auto *runAction = new RunAction(primaryAction);
  SetUserAction(runAction);

  auto *eventAction = new EventAction(runAction, primaryAction);
  SetUserAction(eventAction);

  auto *steppingAction = new SteppingAction(eventAction, primaryAction);
  SetUserAction(steppingAction);
}