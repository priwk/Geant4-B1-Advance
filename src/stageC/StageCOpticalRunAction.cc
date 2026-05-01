#include "StageCOpticalRunAction.hh"

#include "AnalysisConfig.hh"
#include "StageCOpticalPrimaryGeneratorAction.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Exception.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <filesystem>
#include <cmath>

namespace
{
  std::string SourcePathForLog(const AnalysisConfig *config,
                               const StageCOpticalPrimaryGeneratorAction *primaryAction)
  {
    if (primaryAction != nullptr && !primaryAction->GetLoadedSourceFile().empty())
      return AnalysisConfig::PathForRecord(primaryAction->GetLoadedSourceFile());
    if (config != nullptr && !config->opticalSourcePath.empty())
      return AnalysisConfig::PathForRecord(config->opticalSourcePath);
    return "unknown";
  }

  std::string DisplayPath(const std::filesystem::path &path)
  {
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path cwd = fs::current_path();
    const fs::path relative = fs::relative(path, cwd, ec);
    if (!ec && !relative.empty())
      return relative.string();

    return path.string();
  }

  std::string SourceFileStem(const std::string &sourcePath)
  {
    const std::string stem = std::filesystem::path(sourcePath).stem().string();
    const std::string suffix = "_zns_step_sources";
    if (stem.size() >= suffix.size() &&
        stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
      return stem.substr(0, stem.size() - suffix.size());
    }
    return stem.empty() ? "unknown" : stem;
  }

  std::string RatioTagFromSourcePath(const std::string &sourcePath)
  {
    if (sourcePath.empty())
      return "";

    std::filesystem::path previous;
    for (const auto &part : std::filesystem::path(sourcePath))
    {
      if (previous.filename().string() == "alpha_li_steps")
      {
        const std::string ratio = part.string();
        if (!ratio.empty() && ratio != "." && ratio != "..")
          return ratio;
      }
      previous = part;
    }

    const auto parent = std::filesystem::path(sourcePath).parent_path();
    if (!parent.filename().string().empty())
      return parent.filename().string();

    return "";
  }
}

StageCOpticalRunAction::StageCOpticalRunAction(AnalysisConfig *config)
    : G4UserRunAction(),
      fConfig(config),
      fPrimaryAction(nullptr),
      fPhotonCsv(),
      fExitPhotonCsv(),
      fPhotonCsvPath(""),
      fExitPhotonCsvPath(""),
      fSummaryCsvPath(""),
      fKernelCsvPath(""),
      fStepKernelCsvPath(""),
      fKernelEvents(),
      fKernelSteps(),
      fRecordedPhotons(0),
      fRecordedWeight(0.0),
      fFrontWeight(0.0),
      fBackWeight(0.0),
      fSideWeight(0.0),
      fAbsorbedWeight(0.0),
      fOtherWeight(0.0)
{
}

StageCOpticalRunAction::~StageCOpticalRunAction()
{
  if (fPhotonCsv.is_open())
    fPhotonCsv.close();
  if (fExitPhotonCsv.is_open())
    fExitPhotonCsv.close();
}

void StageCOpticalRunAction::SetPrimaryAction(
    const StageCOpticalPrimaryGeneratorAction *primaryAction)
{
  fPrimaryAction = primaryAction;
}

std::string StageCOpticalRunAction::MakeRatioTagFromSourcePath() const
{
  if (fConfig != nullptr && !fConfig->opticalSourcePath.empty())
  {
    const std::string ratio = RatioTagFromSourcePath(fConfig->opticalSourcePath);
    if (!ratio.empty())
      return ratio;
  }

  if (fPrimaryAction != nullptr && !fPrimaryAction->GetLoadedSourceFile().empty())
  {
    const std::string ratio = RatioTagFromSourcePath(fPrimaryAction->GetLoadedSourceFile());
    if (!ratio.empty())
      return ratio;
  }

  return "unknown";
}

std::string StageCOpticalRunAction::MakeOutputStemFromSourcePath() const
{
  if (fPrimaryAction != nullptr)
    return SourceFileStem(fPrimaryAction->GetLoadedSourceFile());

  if (fConfig != nullptr)
    return SourceFileStem(fConfig->opticalSourcePath);

  return "unknown";
}

void StageCOpticalRunAction::OpenOutputs()
{
  namespace fs = std::filesystem;

  const std::string ratioTag = MakeRatioTagFromSourcePath();
  const std::string stem = MakeOutputStemFromSourcePath();
  const fs::path outDir = fs::path("..") / "Output" / "stageC" / ratioTag;

  std::error_code ec;
  fs::create_directories(outDir, ec);
  if (ec)
  {
    G4cerr << "[StageCOpticalRunAction] Warning: failed to create output directory: "
           << ec.message() << G4endl;
  }

  fPhotonCsvPath = DisplayPath(outDir / (stem + "_optical_photons.csv"));
  fExitPhotonCsvPath = DisplayPath(outDir / (stem + "_local_optical_exit_photons.csv"));
  fKernelCsvPath = DisplayPath(outDir / (stem + "_local_optical_kernel_events.csv"));
  fStepKernelCsvPath = DisplayPath(outDir / (stem + "_local_optical_kernel_steps.csv"));
  fSummaryCsvPath = DisplayPath(outDir / (stem + "_optical_summary.csv"));

  fPhotonCsv.open(fPhotonCsvPath.c_str(), std::ios::out);
  if (!fPhotonCsv)
  {
    G4Exception("StageCOpticalRunAction::OpenOutputs",
                "BNZS_C_RUN_001", FatalException,
                ("Failed to open Stage C photon CSV: " + fPhotonCsvPath).c_str());
    return;
  }

  WritePhotonHeader();

  fExitPhotonCsv.open(fExitPhotonCsvPath.c_str(), std::ios::out);
  if (!fExitPhotonCsv)
  {
    G4Exception("StageCOpticalRunAction::OpenOutputs",
                "BNZS_C_RUN_002", FatalException,
                ("Failed to open Stage C exit photon CSV: " + fExitPhotonCsvPath).c_str());
    return;
  }

  WriteExitPhotonHeader();
}

void StageCOpticalRunAction::WritePhotonHeader()
{
  fPhotonCsv
      << "geant_eventID,"
      << "source_event_uid,"
      << "source_step_uid,"
      << "source_eventID,"
      << "source_trackID,"
      << "source_stepID,"
      << "source_particle,"
      << "thickness_um,"
      << "bn_wt,"
      << "zns_wt,"
      << "ratio_label,"
      << "depth_um,"
      << "capture_depth_um,"
      << "sourceSampling,"
      << "sample_index,"
      << "n_sample_photons_per_step,"
      << "photon_weight,"
      << "n_photon_step,"
      << "source_x_um,"
      << "source_y_um,"
      << "source_z_um,"
      << "outcome,"
      << "final_x_um,"
      << "final_y_um,"
      << "final_z_um,"
      << "final_phase,"
      << "track_length_um"
      << "\n";
}

void StageCOpticalRunAction::WriteExitPhotonHeader()
{
  fExitPhotonCsv
      << "eventID,"
      << "source_event_uid,"
      << "source_step_uid,"
      << "trackID,"
      << "stepID,"
      << "particle,"
      << "bn_wt,"
      << "zns_wt,"
      << "ratio_label,"
      << "thickness_um,"
      << "placement_file,"
      << "photon_energy_eV,"
      << "wavelength_nm,"
      << "source_x_um,"
      << "source_y_um,"
      << "source_z_um,"
      << "exit_x_um,"
      << "exit_y_um,"
      << "exit_z_um,"
      << "exit_surface,"
      << "exit_theta,"
      << "exit_phi,"
      << "exit_dir_x,"
      << "exit_dir_y,"
      << "exit_dir_z,"
      << "capture_depth_um,"
      << "source_macro_z_um,"
      << "exit_macro_z_um,"
      << "path_length_um,"
      << "weight"
      << "\n";
}

void StageCOpticalRunAction::BeginOfRunAction(const G4Run *run)
{
  G4RunManager::GetRunManager()->SetRandomNumberStore(false);

  fRecordedPhotons = 0;
  fRecordedWeight = 0.0;
  fFrontWeight = 0.0;
  fBackWeight = 0.0;
  fSideWeight = 0.0;
  fAbsorbedWeight = 0.0;
  fOtherWeight = 0.0;
  fKernelEvents.clear();
  fKernelSteps.clear();

  OpenOutputs();

  G4cout << "\n[StageCOpticalRunAction] Begin run " << run->GetRunID()
         << "\n  source CSV  = " << SourcePathForLog(fConfig, fPrimaryAction)
         << "\n  photon CSV  = " << fPhotonCsvPath
         << "\n  exit CSV    = " << fExitPhotonCsvPath
         << "\n  kernel CSV  = " << fKernelCsvPath
         << "\n  step kernel = " << fStepKernelCsvPath
         << "\n  summary CSV = " << fSummaryCsvPath
         << G4endl;
}

void StageCOpticalRunAction::AddOutcomeWeight(const std::string &outcome,
                                              G4double weight)
{
  if (outcome == "front")
    fFrontWeight += weight;
  else if (outcome == "back")
    fBackWeight += weight;
  else if (outcome == "side")
    fSideWeight += weight;
  else if (outcome == "absorbed")
    fAbsorbedWeight += weight;
  else
    fOtherWeight += weight;
}

void StageCOpticalRunAction::AddKernelOutcomeWeight(KernelEvent &event,
                                                    const std::string &outcome,
                                                    G4double weight)
{
  if (outcome == "front")
    event.escaped_front_weight += weight;
  else if (outcome == "back")
    event.escaped_back_weight += weight;
  else if (outcome == "side")
    event.escaped_side_weight += weight;
  else if (outcome == "absorbed")
    event.absorbed_weight += weight;
  else
    event.lost_weight += weight;
}

void StageCOpticalRunAction::AddKernelOutcomeWeight(KernelStep &step,
                                                    const std::string &outcome,
                                                    G4double weight)
{
  if (outcome == "front")
    step.escaped_front_weight += weight;
  else if (outcome == "back")
    step.escaped_back_weight += weight;
  else if (outcome == "side")
    step.escaped_side_weight += weight;
  else if (outcome == "absorbed")
    step.absorbed_weight += weight;
  else
    step.lost_weight += weight;
}

void StageCOpticalRunAction::RecordPhotonOutcome(
    const StageCPhotonRecord &photon,
    const std::string &outcome,
    const G4ThreeVector &finalPosition,
    const G4ThreeVector &finalDirection,
    const std::string &finalPhase,
    G4double trackLength)
{
  if (!fPhotonCsv.is_open())
    return;

  const auto &source = photon.source;
  const G4double weight = photon.photonWeight;

  fPhotonCsv
      << photon.geantEventID << ","
      << source.source_event_uid << ","
      << source.source_step_uid << ","
      << source.eventID << ","
      << source.trackID << ","
      << source.stepID << ","
      << source.particle << ","
      << source.thickness_um << ","
      << source.bn_wt << ","
      << source.zns_wt << ","
      << source.ratio_label << ","
      << source.depth_um << ","
      << source.capture_depth_um << ","
      << photon.sourceSampling << ","
      << photon.sampleIndex << ","
      << photon.samplesPerStep << ","
      << weight << ","
      << source.n_photon_step << ","
      << photon.sourcePosition.x() / um << ","
      << photon.sourcePosition.y() / um << ","
      << photon.sourcePosition.z() / um << ","
      << outcome << ","
      << finalPosition.x() / um << ","
      << finalPosition.y() / um << ","
      << finalPosition.z() / um << ","
      << finalPhase << ","
      << trackLength / um
      << "\n";

  ++fRecordedPhotons;
  fRecordedWeight += weight;
  AddOutcomeWeight(outcome, weight);

  const G4bool escaped =
      (outcome == "front" || outcome == "back" || outcome == "side");
  const G4double exitTheta = escaped ? finalDirection.theta() : 0.0;
  AccumulateKernelStep(photon, outcome, weight);
  AccumulateKernelEvent(photon, outcome, weight, exitTheta, trackLength);

  if (escaped)
  {
    WriteExitPhoton(photon,
                    outcome,
                    finalPosition,
                    finalDirection,
                    trackLength,
                    weight);
  }
}

void StageCOpticalRunAction::AccumulateKernelStep(
    const StageCPhotonRecord &photon,
    const std::string &outcome,
    G4double weight)
{
  const auto &source = photon.source;
  auto &step = fKernelSteps[source.source_step_uid];

  if (step.source_step_uid.empty())
  {
    step.source_step_uid = source.source_step_uid;
    step.source_event_uid = source.source_event_uid;
    step.eventID = source.eventID;
    step.trackID = source.trackID;
    step.stepID = source.stepID;
    step.particle = source.particle;
    step.placement_file = source.placement_file;
    step.bn_wt = source.bn_wt;
    step.zns_wt = source.zns_wt;
    step.ratio_label = source.ratio_label;
    step.thickness_um = source.thickness_um;
    step.depth_um = source.depth_um;
    step.capture_depth_um = source.capture_depth_um;
    step.edep_keV = source.edep_keV;
    step.visible_edep_keV = source.visible_edep_keV;
    step.n_photon_step = source.n_photon_step;
    step.step_length_um = source.step_len_um;
  }

  if (photon.sampleIndex == 1)
    step.initial_weight += source.n_photon_step;

  ++step.n_sampled;
  step.source_x_sum_um += photon.sourcePosition.x() / um;
  step.source_y_sum_um += photon.sourcePosition.y() / um;
  step.source_z_sum_um += photon.sourcePosition.z() / um;
  step.source_weight += 1.0;

  AddKernelOutcomeWeight(step, outcome, weight);
}

void StageCOpticalRunAction::AccumulateKernelEvent(
    const StageCPhotonRecord &photon,
    const std::string &outcome,
    G4double weight,
    G4double exitTheta,
  G4double trackLength)
{
  const auto &source = photon.source;
  auto &event = fKernelEvents[source.source_event_uid];

  if (event.eventID < 0)
  {
    event.source_event_uid = source.source_event_uid;
    event.eventID = source.eventID;
    event.placement_file = source.placement_file;
    event.bn_wt = source.bn_wt;
    event.zns_wt = source.zns_wt;
    event.ratio_label = source.ratio_label;
    event.thickness_um = source.thickness_um;
    event.depth_um = source.depth_um;
    event.capture_depth_um = source.capture_depth_um;
  }

  if (photon.sampleIndex == 1)
  {
    event.edep_ZnS_keV += source.edep_keV;
    event.visible_edep_ZnS_keV += source.visible_edep_keV;
    event.initial_photon_weight += source.n_photon_step;
    ++event.n_zns_steps;
  }

  ++event.n_sampled_photons;
  AddKernelOutcomeWeight(event, outcome, weight);

  event.weighted_path_length += weight * (trackLength / um);
  event.path_length_weight += weight;

  if (outcome == "front" || outcome == "back" || outcome == "side")
  {
    event.weighted_exit_angle += weight * exitTheta;
    event.exit_angle_weight += weight;
  }
}

void StageCOpticalRunAction::WriteExitPhoton(
    const StageCPhotonRecord &photon,
    const std::string &outcome,
    const G4ThreeVector &finalPosition,
    const G4ThreeVector &finalDirection,
    G4double trackLength,
    G4double weight)
{
  if (!fExitPhotonCsv.is_open())
    return;

  const auto &source = photon.source;
  fExitPhotonCsv
      << source.eventID << ","
      << source.source_event_uid << ","
      << source.source_step_uid << ","
      << source.trackID << ","
      << source.stepID << ","
      << source.particle << ","
      << source.bn_wt << ","
      << source.zns_wt << ","
      << source.ratio_label << ","
      << source.thickness_um << ","
      << source.placement_file << ","
      << 2.7552 << ","
      << 450.0 << ","
      << photon.sourcePosition.x() / um << ","
      << photon.sourcePosition.y() / um << ","
      << photon.sourcePosition.z() / um << ","
      << finalPosition.x() / um << ","
      << finalPosition.y() / um << ","
      << finalPosition.z() / um << ","
      << outcome << ","
      << finalDirection.theta() << ","
      << finalDirection.phi() << ","
      << finalDirection.x() << ","
      << finalDirection.y() << ","
      << finalDirection.z() << ","
      << source.capture_depth_um << ","
      << source.capture_depth_um + photon.sourcePosition.z() / um << ","
      << source.capture_depth_um + finalPosition.z() / um << ","
      << trackLength / um << ","
      << weight
      << "\n";
}

void StageCOpticalRunAction::WriteSummary()
{
  std::ofstream out(fSummaryCsvPath.c_str(), std::ios::out);
  if (!out)
  {
    G4cerr << "[StageCOpticalRunAction] Warning: failed to write summary CSV: "
           << fSummaryCsvPath << G4endl;
    return;
  }

  const G4double denom = (fRecordedWeight > 0.0) ? fRecordedWeight : 1.0;
  out
      << "source_csv,"
      << "recorded_photons,"
      << "recorded_weight,"
      << "front_weight,"
      << "back_weight,"
      << "side_weight,"
      << "absorbed_weight,"
      << "other_weight,"
      << "front_fraction,"
      << "back_fraction,"
      << "side_fraction,"
      << "absorbed_fraction,"
      << "other_fraction"
      << "\n";

  out
      << SourcePathForLog(fConfig, fPrimaryAction) << ","
      << fRecordedPhotons << ","
      << fRecordedWeight << ","
      << fFrontWeight << ","
      << fBackWeight << ","
      << fSideWeight << ","
      << fAbsorbedWeight << ","
      << fOtherWeight << ","
      << fFrontWeight / denom << ","
      << fBackWeight / denom << ","
      << fSideWeight / denom << ","
      << fAbsorbedWeight / denom << ","
      << fOtherWeight / denom
      << "\n";
}

void StageCOpticalRunAction::WriteKernelEvents()
{
  std::ofstream out(fKernelCsvPath.c_str(), std::ios::out);
  if (!out)
  {
    G4cerr << "[StageCOpticalRunAction] Warning: failed to write kernel CSV: "
           << fKernelCsvPath << G4endl;
    return;
  }

  out
      << "source_event_uid,"
      << "eventID,"
      << "placement_file,"
      << "bn_wt,"
      << "zns_wt,"
      << "ratio_label,"
      << "thickness_um,"
      << "depth_um,"
      << "capture_depth_um,"
      << "n_zns_steps,"
      << "n_sampled_photons,"
      << "edep_ZnS_keV,"
      << "visible_edep_ZnS_keV,"
      << "initial_photon_weight,"
      << "escaped_front_weight,"
      << "escaped_back_weight,"
      << "escaped_side_weight,"
      << "absorbed_weight,"
      << "lost_weight,"
      << "escape_eff_front,"
      << "escape_eff_back,"
      << "escape_eff_side,"
      << "escape_eff_total,"
      << "mean_exit_angle,"
      << "mean_path_length_um"
      << "\n";

  for (const auto &item : fKernelEvents)
  {
    const auto &event = item.second;
    const G4double denom =
        (event.initial_photon_weight > 0.0) ? event.initial_photon_weight : 1.0;
    const G4double escapedTotal =
        event.escaped_front_weight +
        event.escaped_back_weight +
        event.escaped_side_weight;
    const G4double meanExitAngle =
        (event.exit_angle_weight > 0.0)
            ? (event.weighted_exit_angle / event.exit_angle_weight)
            : 0.0;
    const G4double meanPathLength =
        (event.path_length_weight > 0.0)
            ? (event.weighted_path_length / event.path_length_weight)
            : 0.0;

    out
        << event.source_event_uid << ","
        << event.eventID << ","
        << event.placement_file << ","
        << event.bn_wt << ","
        << event.zns_wt << ","
        << event.ratio_label << ","
        << event.thickness_um << ","
        << event.depth_um << ","
        << event.capture_depth_um << ","
        << event.n_zns_steps << ","
        << event.n_sampled_photons << ","
        << event.edep_ZnS_keV << ","
        << event.visible_edep_ZnS_keV << ","
        << event.initial_photon_weight << ","
        << event.escaped_front_weight << ","
        << event.escaped_back_weight << ","
        << event.escaped_side_weight << ","
        << event.absorbed_weight << ","
        << event.lost_weight << ","
        << event.escaped_front_weight / denom << ","
        << event.escaped_back_weight / denom << ","
        << event.escaped_side_weight / denom << ","
        << escapedTotal / denom << ","
        << meanExitAngle << ","
        << meanPathLength
        << "\n";
  }
}

void StageCOpticalRunAction::WriteKernelSteps()
{
  std::ofstream out(fStepKernelCsvPath.c_str(), std::ios::out);
  if (!out)
  {
    G4cerr << "[StageCOpticalRunAction] Warning: failed to write step kernel CSV: "
           << fStepKernelCsvPath << G4endl;
    return;
  }

  out
      << "source_step_uid,"
      << "source_event_uid,"
      << "eventID,"
      << "trackID,"
      << "stepID,"
      << "particle,"
      << "bn_wt,"
      << "zns_wt,"
      << "ratio_label,"
      << "thickness_um,"
      << "placement_file,"
      << "depth_um,"
      << "capture_depth_um,"
      << "edep_keV,"
      << "visible_edep_keV,"
      << "n_photon_step,"
      << "n_sampled,"
      << "initial_weight,"
      << "escaped_front_weight,"
      << "escaped_back_weight,"
      << "escaped_side_weight,"
      << "absorbed_weight,"
      << "lost_weight,"
      << "front_fraction,"
      << "back_fraction,"
      << "side_fraction,"
      << "absorbed_fraction,"
      << "lost_fraction,"
      << "source_x_mean_um,"
      << "source_y_mean_um,"
      << "source_z_mean_um,"
      << "step_length_um"
      << "\n";

  for (const auto &item : fKernelSteps)
  {
    const auto &step = item.second;
    const G4double denom = (step.initial_weight > 0.0) ? step.initial_weight : 1.0;
    const G4double sourceDenom = (step.source_weight > 0.0) ? step.source_weight : 1.0;

    out
        << step.source_step_uid << ","
        << step.source_event_uid << ","
        << step.eventID << ","
        << step.trackID << ","
        << step.stepID << ","
        << step.particle << ","
        << step.bn_wt << ","
        << step.zns_wt << ","
        << step.ratio_label << ","
        << step.thickness_um << ","
        << step.placement_file << ","
        << step.depth_um << ","
        << step.capture_depth_um << ","
        << step.edep_keV << ","
        << step.visible_edep_keV << ","
        << step.n_photon_step << ","
        << step.n_sampled << ","
        << step.initial_weight << ","
        << step.escaped_front_weight << ","
        << step.escaped_back_weight << ","
        << step.escaped_side_weight << ","
        << step.absorbed_weight << ","
        << step.lost_weight << ","
        << step.escaped_front_weight / denom << ","
        << step.escaped_back_weight / denom << ","
        << step.escaped_side_weight / denom << ","
        << step.absorbed_weight / denom << ","
        << step.lost_weight / denom << ","
        << step.source_x_sum_um / sourceDenom << ","
        << step.source_y_sum_um / sourceDenom << ","
        << step.source_z_sum_um / sourceDenom << ","
        << step.step_length_um
        << "\n";
  }
}

void StageCOpticalRunAction::EndOfRunAction(const G4Run *run)
{
  if (fPhotonCsv.is_open())
  {
    fPhotonCsv.flush();
    fPhotonCsv.close();
  }
  if (fExitPhotonCsv.is_open())
  {
    fExitPhotonCsv.flush();
    fExitPhotonCsv.close();
  }

  WriteKernelEvents();
  WriteKernelSteps();
  WriteSummary();

  G4cout << "\n[StageCOpticalRunAction] End run " << run->GetRunID()
         << "\n  recorded photons = " << fRecordedPhotons
         << "\n  recorded weight  = " << fRecordedWeight
         << "\n  front/back/side/absorbed/other weight = "
         << fFrontWeight << " / "
         << fBackWeight << " / "
         << fSideWeight << " / "
         << fAbsorbedWeight << " / "
         << fOtherWeight
         << "\n  summary CSV = " << fSummaryCsvPath
         << "\n  kernel CSV  = " << fKernelCsvPath
         << "\n  step kernel = " << fStepKernelCsvPath
         << G4endl;
}
