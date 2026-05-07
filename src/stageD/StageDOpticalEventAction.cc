#include "StageDOpticalEventAction.hh"

#include "AnalysisConfig.hh"
#include "DetectorConstruction.hh"
#include "StageDOpticalPrimaryGeneratorAction.hh"
#include "StageDOpticalRunAction.hh"

#include "G4Event.hh"
#include "G4RunManager.hh"

#include <filesystem>
#include <sstream>

namespace
{
  std::string RatioTag(G4double bnWt, G4double znsWt)
  {
    auto toTag = [](G4double value)
    {
      const G4double rounded = std::round(value);
      std::ostringstream oss;
      if (std::abs(value - rounded) < 1.0e-9)
        oss << static_cast<long long>(rounded);
      else
        oss << value;
      return oss.str();
    };
    return toTag(bnWt) + "-" + toTag(znsWt);
  }
}

StageDOpticalEventAction::StageDOpticalEventAction(
    StageDOpticalRunAction *runAction,
    const StageDOpticalPrimaryGeneratorAction *primaryAction,
    AnalysisConfig *config)
    : G4UserEventAction(),
      fConfig(config),
      fRunAction(runAction),
      fPrimaryAction(primaryAction),
      fCurrentEvent()
{
}

StageDOpticalEventAction::~StageDOpticalEventAction() = default;

void StageDOpticalEventAction::BeginOfEventAction(const G4Event *event)
{
  fCurrentEvent = StageDPhotonEventRecord{};

  if (event == nullptr || fConfig == nullptr || fPrimaryAction == nullptr)
    return;

  const auto *detector = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  const StageDPhotonLaunchRecord &launch = fPrimaryAction->GetCurrentPhotonRecord();

  fCurrentEvent.photonID = event->GetEventID();
  fCurrentEvent.ratio = RatioTag(fConfig->bnWt, fConfig->znsWt);
  fCurrentEvent.placement_file =
      detector ? detector->GetLoadedPlacementFileForRecord() : "unknown";
  fCurrentEvent.source_mode = fConfig->stageD_source_mode;
  fCurrentEvent.boundary_mode = fConfig->stageD_boundary_mode;
  fCurrentEvent.reentry_mode = fConfig->stageD_reentry_mode;
  fCurrentEvent.matrix_reentry_mode = fConfig->stageD_matrix_reentry_mode;
  fCurrentEvent.wavelength_nm = launch.wavelength_nm;
  fCurrentEvent.source_phase = launch.source_phase;
  fCurrentEvent.source_x_um = launch.source_position.x() / um;
  fCurrentEvent.source_y_um = launch.source_position.y() / um;
  fCurrentEvent.source_z_um = launch.source_position.z() / um;
  fCurrentEvent.final_status = "in_progress";
  fCurrentEvent.weight = launch.photon_weight;
}

void StageDOpticalEventAction::EndOfEventAction(const G4Event *event)
{
  (void)event;

  if (fCurrentEvent.final_status == "in_progress" ||
      fCurrentEvent.final_status == "continued_reentry")
    fCurrentEvent.final_status = "lost";

  if (fCurrentEvent.num_real_scatter > 0)
  {
    fCurrentEvent.mean_cos_theta_for_this_photon =
        fCurrentEvent.sum_cos_theta /
        static_cast<G4double>(fCurrentEvent.num_real_scatter);
  }
  else
  {
    fCurrentEvent.mean_cos_theta_for_this_photon = 0.0;
  }

  if (fRunAction != nullptr)
    fRunAction->RecordPhotonEvent(fCurrentEvent);
}

void StageDOpticalEventAction::SetFinalStatus(
    const std::string &status,
    G4bool absorbed)
{
  fCurrentEvent.final_status = status;
  fCurrentEvent.absorbed = absorbed;
}

void StageDOpticalEventAction::MarkAbsorbed()
{
  SetFinalStatus("absorbed", true);
}
