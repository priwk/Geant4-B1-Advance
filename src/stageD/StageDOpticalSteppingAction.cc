#include "StageDOpticalSteppingAction.hh"

#include "AnalysisConfig.hh"
#include "DetectorConstruction.hh"
#include "StageDOpticalEventAction.hh"
#include "StageDOpticalRunAction.hh"
#include "StageDReentrySampler.hh"

#include "G4EventManager.hh"
#include "G4Exception.hh"
#include "G4DynamicParticle.hh"
#include "G4OpticalPhoton.hh"
#include "G4RunManager.hh"
#include "G4StackManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4TrackStatus.hh"
#include "G4VProcess.hh"

#include <algorithm>
#include <cmath>

namespace
{
  G4bool IsOpticalPhoton(const G4Track *track)
  {
    return track != nullptr &&
           track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition();
  }

  G4double AngleDeg(const G4ThreeVector &a, const G4ThreeVector &b)
  {
    const G4double dot = std::clamp(a.dot(b), -1.0, 1.0);
    return std::acos(dot) / deg;
  }

  std::string ProcessName(const G4StepPoint *point)
  {
    if (point == nullptr)
      return "";
    const auto *process = point->GetProcessDefinedStep();
    if (process == nullptr)
      return "";
    return process->GetProcessName();
  }

  G4bool IsOutsideRve(const G4ThreeVector &position,
                      const DetectorConstruction *detector)
  {
    if (detector == nullptr)
      return false;

    const G4double halfX = detector->GetPatchHalfXUm() * um;
    const G4double halfY = detector->GetPatchHalfYUm() * um;
    const G4double halfZ = detector->GetPatchHalfZUm() * um;
    return (std::abs(position.x()) > halfX ||
            std::abs(position.y()) > halfY ||
            std::abs(position.z()) > halfZ);
  }

  G4bool IsParticlePhase(DetectorConstruction::Phase phase)
  {
    return phase == DetectorConstruction::Phase::BN ||
           phase == DetectorConstruction::Phase::ZnS;
  }
}

StageDOpticalSteppingAction::StageDOpticalSteppingAction(
    StageDOpticalRunAction *runAction,
    StageDOpticalEventAction *eventAction,
    AnalysisConfig *config)
    : G4UserSteppingAction(),
      fConfig(config),
      fRunAction(runAction),
      fEventAction(eventAction),
      fDetector(nullptr),
      fReentrySampler(nullptr)
{
}

StageDOpticalSteppingAction::~StageDOpticalSteppingAction()
{
  delete fReentrySampler;
}

const DetectorConstruction *StageDOpticalSteppingAction::ResolveDetector() const
{
  if (fDetector == nullptr)
  {
    fDetector = dynamic_cast<const DetectorConstruction *>(
        G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  }
  return fDetector;
}

G4bool StageDOpticalSteppingAction::HandleBoundaryReentry(
    const G4Step *step,
    G4Track *track,
    const DetectorConstruction *detector)
{
  if (fConfig == nullptr || fEventAction == nullptr || detector == nullptr)
    return false;

  auto &event = fEventAction->MutableCurrentEvent();
  const G4ThreeVector postPos = step->GetPostStepPoint()->GetPosition();
  const G4bool outOfBox = IsOutsideRve(postPos, detector);
  if (!outOfBox)
    return false;

  if (fConfig->stageD_boundary_mode == "escape")
  {
    fEventAction->SetFinalStatus("escaped_debug", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  if (fConfig->stageD_boundary_mode != "same_phase_reentry")
    return false;

  if (event.num_reentry >= fConfig->stageD_max_reentry)
  {
    fEventAction->SetFinalStatus("max_reentry", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  if (fReentrySampler == nullptr)
    fReentrySampler = new StageDReentrySampler(detector);

  const G4ThreeVector insidePos = step->GetPreStepPoint()->GetPosition();
  const auto phase = detector->FindPhaseAtPoint(insidePos);
  const G4ThreeVector oldDir = step->GetPreStepPoint()->GetMomentumDirection();

  G4ThreeVector newPosition;
  G4bool ok = false;
  if (phase == DetectorConstruction::Phase::BN ||
      phase == DetectorConstruction::Phase::ZnS)
  {
    ok = fReentrySampler->SampleSamePhaseSphereReentry(
        phase,
        insidePos,
        fConfig->stageD_reentry_mode,
        newPosition);
  }
  else if (phase == DetectorConstruction::Phase::Matrix)
  {
    ok = fReentrySampler->SampleMatrixReentry(
        fConfig->stageD_matrix_reentry_mode,
        newPosition);
  }

  if (!ok)
  {
    fEventAction->SetFinalStatus("reentry_failed", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  auto *dynamicParticle = new G4DynamicParticle(
      G4OpticalPhoton::OpticalPhotonDefinition(),
      oldDir,
      track->GetKineticEnergy());
  dynamicParticle->SetPolarization(track->GetPolarization());

  auto *continuationTrack = new G4Track(
      dynamicParticle,
      track->GetGlobalTime(),
      newPosition);
  continuationTrack->SetLocalTime(track->GetLocalTime());
  continuationTrack->SetProperTime(track->GetProperTime());
  continuationTrack->SetWeight(track->GetWeight());
  continuationTrack->SetParentID(track->GetParentID());

  auto *stackManager = G4EventManager::GetEventManager()->GetStackManager();
  if (stackManager == nullptr)
  {
    delete continuationTrack;
    fEventAction->SetFinalStatus("reentry_failed", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }
  stackManager->PushOneTrack(continuationTrack);

  ++event.num_reentry;
  if (phase == DetectorConstruction::Phase::BN)
    ++event.num_reentry_BN;
  else if (phase == DetectorConstruction::Phase::ZnS)
    ++event.num_reentry_ZnS;
  else if (phase == DetectorConstruction::Phase::Matrix)
    ++event.num_reentry_matrix;

  track->SetTrackStatus(fStopAndKill);
  return true;
}

G4bool StageDOpticalSteppingAction::HandleLimitKills(const G4Step *step, G4Track *track)
{
  if (fConfig == nullptr || fEventAction == nullptr)
    return false;

  auto &event = fEventAction->MutableCurrentEvent();

  const G4int primaryScatterCount =
      (fConfig->stageD_scatter_metric == "step_angle_threshold")
          ? event.num_real_scatter
          : event.num_particle_scatter;

  if (primaryScatterCount >= fConfig->stageD_target_primary_scatter)
  {
    fEventAction->SetFinalStatus("target_primary_scatter", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  if (event.num_steps >= fConfig->stageD_max_steps)
  {
    fEventAction->SetFinalStatus("max_steps", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  if (event.total_path_length_um >= fConfig->stageD_max_path_length_um)
  {
    fEventAction->SetFinalStatus("max_path_length", false);
    track->SetTrackStatus(fStopAndKill);
    return true;
  }

  const std::string processName = ProcessName(step->GetPostStepPoint());
  if (processName == "OpAbsorption")
  {
    fEventAction->MarkAbsorbed();
    return false;
  }

  if (track->GetTrackStatus() == fStopAndKill &&
      event.final_status == "in_progress")
  {
    fEventAction->SetFinalStatus("lost", false);
  }

  return false;
}

void StageDOpticalSteppingAction::UserSteppingAction(const G4Step *step)
{
  if (step == nullptr || fEventAction == nullptr)
    return;

  G4Track *track = step->GetTrack();
  if (!IsOpticalPhoton(track))
    return;

  auto &event = fEventAction->MutableCurrentEvent();
  ++event.num_steps;
  event.total_path_length_um += step->GetStepLength() / um;

  const auto *prePoint = step->GetPreStepPoint();
  const auto *postPoint = step->GetPostStepPoint();
  if (prePoint == nullptr || postPoint == nullptr)
    return;

  const auto *detector = ResolveDetector();
  const G4bool isOuterRveExit =
      (detector != nullptr && IsOutsideRve(postPoint->GetPosition(), detector));
  const auto prePhase =
      (detector != nullptr) ? detector->FindPhaseAtPoint(prePoint->GetPosition())
                            : DetectorConstruction::Phase::Unknown;
  const auto postPhase =
      (detector != nullptr && !isOuterRveExit)
          ? detector->FindPhaseAtPoint(postPoint->GetPosition())
          : DetectorConstruction::Phase::World;

  const std::string processName = ProcessName(postPoint);
  const G4bool isMaterialBoundary =
      !isOuterRveExit &&
      (processName == "OpBoundary" ||
       (prePhase != DetectorConstruction::Phase::Unknown &&
        postPhase != DetectorConstruction::Phase::Unknown &&
        prePhase != postPhase &&
        prePhase != DetectorConstruction::Phase::World &&
        postPhase != DetectorConstruction::Phase::World));
  if (isMaterialBoundary)
  {
    ++event.num_material_boundary;
  }

  const G4ThreeVector preDir = prePoint->GetMomentumDirection();
  const G4ThreeVector postDir = postPoint->GetMomentumDirection();

  if (IsParticlePhase(prePhase) &&
      !event.in_particle_segment)
  {
    event.in_particle_segment = true;
    event.particle_segment_phase = DetectorConstruction::PhaseName(prePhase);
    event.particle_segment_entry_direction = preDir;
  }

  if (event.in_particle_segment &&
      IsParticlePhase(prePhase) &&
      prePhase != postPhase)
  {
    const G4double thetaDeg =
        AngleDeg(event.particle_segment_entry_direction, postDir);
    if (thetaDeg > fConfig->stageD_theta_threshold_deg)
    {
      const G4double cosTheta = std::clamp(
          event.particle_segment_entry_direction.dot(postDir),
          -1.0, 1.0);
      ++event.num_particle_scatter;
      event.sum_cos_theta_particle += cosTheta;
      if (prePhase == DetectorConstruction::Phase::BN)
      {
        ++event.num_particle_scatter_BN;
        event.sum_cos_theta_particle_BN += cosTheta;
      }
      else if (prePhase == DetectorConstruction::Phase::ZnS)
      {
        ++event.num_particle_scatter_ZnS;
        event.sum_cos_theta_particle_ZnS += cosTheta;
      }
    }
    event.in_particle_segment = false;
    event.particle_segment_phase.clear();
  }

  if (!isOuterRveExit && processName != "OpAbsorption")
  {
    const G4double thetaDeg =
        AngleDeg(preDir, postDir);
    if (thetaDeg > fConfig->stageD_theta_threshold_deg)
    {
      ++event.num_real_scatter;
      const G4double cosTheta = std::clamp(
          preDir.dot(postDir),
          -1.0, 1.0);
      event.sum_cos_theta += cosTheta;
      if (isMaterialBoundary)
      {
        ++event.num_boundary_scatter;
        event.sum_cos_theta_boundary += cosTheta;
        if (prePhase == DetectorConstruction::Phase::BN)
        {
          ++event.num_boundary_scatter_BN;
          event.sum_cos_theta_boundary_BN += cosTheta;
        }
        else if (prePhase == DetectorConstruction::Phase::ZnS)
        {
          ++event.num_boundary_scatter_ZnS;
          event.sum_cos_theta_boundary_ZnS += cosTheta;
        }
      }
      else
      {
        ++event.num_bulk_scatter;
        event.sum_cos_theta_bulk += cosTheta;
      }
    }
  }

  if (detector != nullptr && HandleBoundaryReentry(step, track, detector))
    return;

  HandleLimitKills(step, track);
}
