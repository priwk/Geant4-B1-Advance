#include "RunAction.hh"

#include "AnalysisConfig.hh"
#include "PrimaryGeneratorAction.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4ios.hh"

// #include <cerrno>
// #include <cstring>
// #include <sys/stat.h>
// #include <sys/types.h>
#include <filesystem>

// --------------------------------------------------------------------

RunAction::RunAction(PrimaryGeneratorAction *primaryAction, AnalysisConfig *config)
    : G4UserRunAction(),
      fPrimaryAction(primaryAction),
      fStepCsv(),
      fStepCsvPath(""),
      fConfig(config)
{
}

// --------------------------------------------------------------------

RunAction::~RunAction()
{
    if (fStepCsv.is_open())
    {
        fStepCsv.close();
    }
}

// --------------------------------------------------------------------

void RunAction::EnsureDataDirectory() const
{
    namespace fs = std::filesystem;
    fs::path outputDir = fs::current_path().parent_path() / "Output";
    std::error_code ec;

    if (!fs::exists(outputDir))
    {
        fs::create_directories(outputDir, ec);
        if (ec)
        {
            G4cerr << "[RunAction] Warning: failed to create Output directory: "
                   << ec.message() << G4endl;
        }
    }
}

// --------------------------------------------------------------------

std::string RunAction::ExtractThicknessTagFromInputPath(const std::string &inputPath) const
{
    // Example: Input/125_neutron_capture_positions.csv -> 125
    const std::string key = "_neutron_capture_positions.csv";

    std::size_t slashPos = inputPath.find_last_of("/\\");
    std::string fileName = (slashPos == std::string::npos)
                               ? inputPath
                               : inputPath.substr(slashPos + 1);

    std::size_t keyPos = fileName.find(key);
    if (keyPos == std::string::npos)
    {
        return "unknown";
    }

    return fileName.substr(0, keyPos);
}

// --------------------------------------------------------------------

std::string RunAction::MakeOutputCsvPathFromInputPath(const std::string &inputPath) const
{
    std::string tag = ExtractThicknessTagFromInputPath(inputPath);

    namespace fs = std::filesystem;
    fs::path outPath = fs::current_path().parent_path() / "Output" / (tag + "_alpha_li_steps.csv");

    return outPath.string();
}

std::string RunAction::MakeOutputCsvPath() const
{
    if (fPrimaryAction)
    {
        return MakeOutputCsvPathFromInputPath(fPrimaryAction->GetLoadedInputFile());
    }

    return MakeOutputCsvPathFromInputPath("unknown_neutron_capture_positions.csv");
}

// --------------------------------------------------------------------

void RunAction::WriteStepCsvHeader()
{
    if (!fStepCsv.is_open())
        return;

    fStepCsv
        << "eventID,"
        << "thickness_um,"
        << "bn_wt,"
        << "zns_wt,"
        << "capture_x_um,"
        << "capture_y_um,"
        << "corr_x_um,"
        << "corr_y_um,"
        << "depth_um,"
        << "surface_mode,"
        << "target_local_z_um,"
        << "used_local_z_um,"
        << "bn_center_x_um,"
        << "bn_center_y_um,"
        << "bn_center_z_um,"
        << "trackID,"
        << "stepID,"
        << "particle,"
        << "phase_pre,"
        << "phase_post,"
        << "x_pre_um,"
        << "y_pre_um,"
        << "z_pre_um,"
        << "x_post_um,"
        << "y_post_um,"
        << "z_post_um,"
        << "step_len_um,"
        << "edep_keV,"
        << "ekin_pre_keV,"
        << "ekin_post_keV"
        << "\n";
}

// --------------------------------------------------------------------

void RunAction::SwitchOutputCsvForInputPath(const std::string &inputPath)
{
    const std::string newPath = MakeOutputCsvPathFromInputPath(inputPath);

    // already using the correct file
    if (fStepCsv.is_open() && newPath == fStepCsvPath)
    {
        return;
    }

    // close previous file if needed
    if (fStepCsv.is_open())
    {
        fStepCsv.flush();
        fStepCsv.close();

        G4cout << "[RunAction] Closed output CSV: " << fStepCsvPath << G4endl;
    }

    fStepCsvPath = newPath;
    fStepCsv.open(fStepCsvPath.c_str(), std::ios::out);

    if (!fStepCsv.is_open())
    {
        G4Exception("RunAction::SwitchOutputCsvForInputPath",
                    "BNZS101", FatalException,
                    ("Failed to open output CSV: " + fStepCsvPath).c_str());
        return;
    }

    WriteStepCsvHeader();

    G4cout << "[RunAction] Switched output CSV to: " << fStepCsvPath << G4endl;
}

// --------------------------------------------------------------------

void RunAction::BeginOfRunAction(const G4Run *run)
{
    G4RunManager::GetRunManager()->SetRandomNumberStore(false);

    EnsureDataDirectory();

    // Open the output corresponding to the current input file.
    // In streaming mode, this may later be switched automatically
    // when PrimaryGeneratorAction moves to the next input CSV.
    if (fPrimaryAction)
    {
        SwitchOutputCsvForInputPath(fPrimaryAction->GetLoadedInputFile());
    }
    else
    {
        fStepCsvPath = MakeOutputCsvPath();
        fStepCsv.open(fStepCsvPath.c_str(), std::ios::out);

        if (!fStepCsv.is_open())
        {
            G4Exception("RunAction::BeginOfRunAction",
                        "BNZS101", FatalException,
                        ("Failed to open output CSV: " + fStepCsvPath).c_str());
            return;
        }

        WriteStepCsvHeader();
    }

    G4cout
        << "\n[RunAction] Begin run " << run->GetRunID()
        << "\n  output CSV = " << fStepCsvPath;

    if (fPrimaryAction)
    {
        G4cout
            << "\n  input CSV  = " << fPrimaryAction->GetLoadedInputFile()
            << "\n  streamed input records so far = " << fPrimaryAction->GetTotalLoadedEvents();
    }

    G4cout << G4endl;
}

// --------------------------------------------------------------------

void RunAction::EndOfRunAction(const G4Run *run)
{
    if (fStepCsv.is_open())
    {
        fStepCsv.flush();
        fStepCsv.close();
    }

    G4cout
        << "\n[RunAction] End run " << run->GetRunID()
        << "\n  closed CSV = " << fStepCsvPath
        << G4endl;
}