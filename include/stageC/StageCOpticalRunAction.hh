#ifndef StageCOpticalRunAction_h
#define StageCOpticalRunAction_h 1

#include "G4UserRunAction.hh"
#include "StageCOpticalSource.hh"

#include <fstream>
#include <map>
#include <string>

class G4Run;
class AnalysisConfig;
class StageCOpticalPrimaryGeneratorAction;

class StageCOpticalRunAction : public G4UserRunAction
{
public:
  explicit StageCOpticalRunAction(AnalysisConfig *config);
  ~StageCOpticalRunAction() override;

  void SetPrimaryAction(const StageCOpticalPrimaryGeneratorAction *primaryAction);

  void BeginOfRunAction(const G4Run *run) override;
  void EndOfRunAction(const G4Run *run) override;

  void RecordPhotonOutcome(const StageCPhotonRecord &photon,
                           const std::string &outcome,
                           const G4ThreeVector &finalPosition,
                           const G4ThreeVector &finalDirection,
                           const std::string &finalPhase,
                           G4double trackLength);

  const std::string &GetPhotonCsvPath() const { return fPhotonCsvPath; }
  const std::string &GetSummaryCsvPath() const { return fSummaryCsvPath; }
  const std::string &GetKernelCsvPath() const { return fKernelCsvPath; }
  const std::string &GetExitPhotonCsvPath() const { return fExitPhotonCsvPath; }

private:
  struct KernelEvent
  {
    std::string source_event_uid;
    G4int eventID = -1;
    std::string placement_file;
    G4double bn_wt = 0.0;
    G4double zns_wt = 0.0;
    std::string ratio_label;
    G4double thickness_um = 0.0;
    G4double depth_um = 0.0;
    G4double capture_depth_um = 0.0;
    G4int n_zns_steps = 0;
    G4int n_sampled_photons = 0;
    G4double edep_ZnS_keV = 0.0;
    G4double visible_edep_ZnS_keV = 0.0;
    G4double initial_photon_weight = 0.0;
    G4double escaped_front_weight = 0.0;
    G4double escaped_back_weight = 0.0;
    G4double escaped_side_weight = 0.0;
    G4double absorbed_weight = 0.0;
    G4double lost_weight = 0.0;
    G4double weighted_exit_angle = 0.0;
    G4double exit_angle_weight = 0.0;
    G4double weighted_path_length = 0.0;
    G4double path_length_weight = 0.0;
  };

  struct KernelStep
  {
    std::string source_step_uid;
    std::string source_event_uid;
    G4int eventID = -1;
    G4int trackID = -1;
    G4int stepID = -1;
    std::string particle;
    std::string placement_file;
    G4double bn_wt = 0.0;
    G4double zns_wt = 0.0;
    std::string ratio_label;
    G4double thickness_um = 0.0;
    G4double depth_um = 0.0;
    G4double capture_depth_um = 0.0;
    G4double edep_keV = 0.0;
    G4double visible_edep_keV = 0.0;
    G4double n_photon_step = 0.0;
    G4int n_sampled = 0;
    G4double initial_weight = 0.0;
    G4double escaped_front_weight = 0.0;
    G4double escaped_back_weight = 0.0;
    G4double escaped_side_weight = 0.0;
    G4double absorbed_weight = 0.0;
    G4double lost_weight = 0.0;
    G4double source_x_sum_um = 0.0;
    G4double source_y_sum_um = 0.0;
    G4double source_z_sum_um = 0.0;
    G4double source_weight = 0.0;
    G4double step_length_um = 0.0;
  };

  std::string MakeOutputStemFromSourcePath() const;
  std::string MakeRatioTagFromSourcePath() const;
  void OpenOutputs();
  void WritePhotonHeader();
  void WriteExitPhotonHeader();
  void WriteSummary();
  void WriteKernelEvents();
  void WriteKernelSteps();
  void AddOutcomeWeight(const std::string &outcome, G4double weight);
  void AddKernelOutcomeWeight(KernelEvent &event, const std::string &outcome, G4double weight);
  void AddKernelOutcomeWeight(KernelStep &step, const std::string &outcome, G4double weight);
  void AccumulateKernelEvent(const StageCPhotonRecord &photon,
                             const std::string &outcome,
                             G4double weight,
                             G4double exitTheta,
                             G4double trackLength);
  void AccumulateKernelStep(const StageCPhotonRecord &photon,
                            const std::string &outcome,
                            G4double weight);
  void WriteExitPhoton(const StageCPhotonRecord &photon,
                       const std::string &outcome,
                       const G4ThreeVector &finalPosition,
                       const G4ThreeVector &finalDirection,
                       G4double trackLength,
                       G4double weight);

private:
  AnalysisConfig *fConfig;
  const StageCOpticalPrimaryGeneratorAction *fPrimaryAction;

  std::ofstream fPhotonCsv;
  std::ofstream fExitPhotonCsv;
  std::string fPhotonCsvPath;
  std::string fExitPhotonCsvPath;
  std::string fSummaryCsvPath;
  std::string fKernelCsvPath;
  std::string fStepKernelCsvPath;
  std::map<std::string, KernelEvent> fKernelEvents;
  std::map<std::string, KernelStep> fKernelSteps;

  G4long fRecordedPhotons;
  G4double fRecordedWeight;
  G4double fFrontWeight;
  G4double fBackWeight;
  G4double fSideWeight;
  G4double fAbsorbedWeight;
  G4double fOtherWeight;
};

#endif
