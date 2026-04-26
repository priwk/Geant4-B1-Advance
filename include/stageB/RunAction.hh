#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"

#include <fstream>
#include <string>

class G4Run;
class PrimaryGeneratorAction;
class AnalysisConfig;

class RunAction : public G4UserRunAction
{
public:
  explicit RunAction(PrimaryGeneratorAction *primaryAction, AnalysisConfig *config = nullptr);
  ~RunAction() override;

  void BeginOfRunAction(const G4Run *) override;
  void EndOfRunAction(const G4Run *) override;

  // For multi-input streaming:
  // switch output CSV according to the current input CSV path.
  void SwitchOutputCsvForInputPath(const std::string &inputPath);

  std::ofstream &GetStepCsv() { return fStepCsv; }
  const std::string &GetStepCsvPath() const { return fStepCsvPath; }
  G4bool IsStepCsvOpen() const { return fStepCsv.is_open(); }

private:
  std::string MakeOutputCsvPath() const;
  std::string MakeOutputCsvPathFromInputPath(const std::string &inputPath) const;
  std::string ExtractThicknessTagFromInputPath(const std::string &inputPath) const;
  void EnsureDataDirectory() const;
  void WriteStepCsvHeader();

private:
  const PrimaryGeneratorAction *fPrimaryAction;
  AnalysisConfig *fConfig;
  std::ofstream fStepCsv;
  std::string fStepCsvPath;
};

#endif