#ifndef StageARunAction_h
#define StageARunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"

#include <string>
#include <chrono>

class G4Run;
class AnalysisConfig;

class StageARunAction : public G4UserRunAction
{
public:
  explicit StageARunAction(AnalysisConfig *config);
  ~StageARunAction() override;

  void BeginOfRunAction(const G4Run *run) override;
  void EndOfRunAction(const G4Run *run) override;

  // ---- accumulation interfaces for Stage A ----
  void AddIncident(G4int n = 1);
  void AddCapture(G4int n = 1);
  void AddTrackLength(G4double len);

  // ---- query helpers ----
  G4int GetIncidentCount() const;
  G4int GetCaptureCount() const;
  G4double GetTotalTrackLength() const;
  G4double GetSigmaEff() const; // internal Geant4 unit: 1/length

  void MaybePrintProgress(const char *reason);
  void ForcePrintProgress(const char *reason) const;

private:
  void ResetAccumulators();

  // ---- CSV helpers ----
  void EnsureOutputDirectories() const;
  void EnsureSummaryCsvReady();
  void AppendSummaryRow(const G4Run *run) const;

  std::string BuildRatioTag() const;
  std::string BuildStageARatioDirectory() const;
  std::string BuildSummaryCsvPath() const;

private:
  AnalysisConfig *fConfig;

  G4int fIncidentCount;
  G4int fCaptureCount;
  G4double fTotalTrackLength;

  std::string fSummaryCsvPath;

  std::chrono::steady_clock::time_point fWallStart;
  G4int fProgressPrintEvery;
  G4int fLastPrintedIncidentCount;
};

#endif
