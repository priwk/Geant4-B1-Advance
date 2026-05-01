#include "StageCOpticalSteppingAction.hh"

#include "AnalysisConfig.hh"
#include "DetectorConstruction.hh"
#include "StageCOpticalPrimaryGeneratorAction.hh"
#include "StageCOpticalRunAction.hh"

#include "G4LogicalVolume.hh"
#include "G4OpticalPhoton.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4TrackStatus.hh"
#include "G4VPhysicalVolume.hh"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
  G4bool StartsWith(const G4String &s, const char *prefix)
  {
    const std::string value = s;
    const std::string p = prefix;
    return value.rfind(p, 0) == 0;
  }

  std::string PhaseLabel(const G4VPhysicalVolume *pv)
  {
    if (!pv)
      return "outside";

    const auto *lv = pv->GetLogicalVolume();
    if (!lv)
      return "outside";

    const auto &lvName = lv->GetName();

    if (lvName == "BN_LV" || StartsWith(lvName, "BN_ClipLV_"))
      return "BN";
    if (lvName == "ZnS_LV" || StartsWith(lvName, "ZnS_ClipLV_"))
      return "ZnS";
    if (lvName == "MatrixLV")
      return "binder_void";
    if (lvName == "WorldLV")
      return "outside";

    return "other";
  }

  G4bool IsOpticalPhoton(const G4Track *track)
  {
    return track != nullptr &&
           track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition();
  }

  std::string EscapeFace(const G4ThreeVector &pos,
                         const DetectorConstruction *det)
  {
    if (det == nullptr)
      return "other";

    const G4double halfXY = 0.5 * det->GetPatchXYUm() * um;
    const G4double frontZ = det->GetFrontSurfaceZ();
    const G4double backZ = det->GetBackSurfaceZ();

    const G4double dFront = std::abs(pos.z() - frontZ);
    const G4double dBack = std::abs(pos.z() - backZ);
    const G4double dX = std::abs(std::abs(pos.x()) - halfXY);
    const G4double dY = std::abs(std::abs(pos.y()) - halfXY);

    const G4double dSide = std::min(dX, dY);
    const G4double dMin = std::min({dFront, dBack, dSide});

    if (dMin == dFront)
      return "front";
    if (dMin == dBack)
      return "back";
    return "side";
  }
}

StageCOpticalSteppingAction::StageCOpticalSteppingAction(
    StageCOpticalRunAction *runAction,
    const StageCOpticalPrimaryGeneratorAction *primaryAction,
    AnalysisConfig *config)
    : G4UserSteppingAction(),
      fConfig(config),
      fRunAction(runAction),
      fPrimaryAction(primaryAction)
{
}

StageCOpticalSteppingAction::~StageCOpticalSteppingAction() = default;

void StageCOpticalSteppingAction::UserSteppingAction(const G4Step *step)
{
  (void)fConfig;

  if (step == nullptr || fRunAction == nullptr || fPrimaryAction == nullptr)
    return;

  auto *track = step->GetTrack();
  if (!IsOpticalPhoton(track))
    return;

  const auto *prePoint = step->GetPreStepPoint();
  const auto *postPoint = step->GetPostStepPoint();
  if (prePoint == nullptr || postPoint == nullptr)
    return;

  const auto *prePV = prePoint->GetPhysicalVolume();
  const auto *postPV = postPoint->GetPhysicalVolume();
  const std::string phasePre = PhaseLabel(prePV);
  const std::string phasePost = PhaseLabel(postPV);

  const auto *det = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4ThreeVector &postPos = postPoint->GetPosition();

  if (phasePre != "outside" && phasePost == "outside")
  {
    const std::string outcome = EscapeFace(postPos, det);
    fRunAction->RecordPhotonOutcome(
        fPrimaryAction->GetCurrentPhotonRecord(),
        outcome,
        postPos,
        postPoint->GetMomentumDirection(),
        phasePost,
        track->GetTrackLength());
    track->SetTrackStatus(fStopAndKill);
    return;
  }

  if (track->GetTrackStatus() == fStopAndKill)
  {
    fRunAction->RecordPhotonOutcome(
        fPrimaryAction->GetCurrentPhotonRecord(),
        "absorbed",
        postPos,
        postPoint->GetMomentumDirection(),
        phasePost,
        track->GetTrackLength());
  }
}
