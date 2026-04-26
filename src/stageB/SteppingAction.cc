#include "SteppingAction.hh"

#include "EventAction.hh"
#include "RunAction.hh"
#include "PrimaryGeneratorAction.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4ParticleDefinition.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4StepPoint.hh"
#include "G4TrackStatus.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <fstream>
#include <string>

// --------------------------------------------------------------------
// helpers
namespace
{
    G4bool IsTrackedHeavyParticle(const G4Track *track)
    {
        const auto *def = track->GetDefinition();
        if (!def)
            return false;

        if (def->GetParticleName() == "alpha")
        {
            return true;
        }

        // Li7 ion
        if (def->GetParticleType() == "nucleus" &&
            def->GetAtomicNumber() == 3 &&
            def->GetAtomicMass() == 7)
        {
            return true;
        }

        return false;
    }

    std::string ParticleLabel(const G4Track *track)
    {
        const auto *def = track->GetDefinition();
        if (!def)
            return "unknown";

        if (def->GetParticleName() == "alpha")
        {
            return "alpha";
        }

        if (def->GetParticleType() == "nucleus" &&
            def->GetAtomicNumber() == 3 &&
            def->GetAtomicMass() == 7)
        {
            return "Li7";
        }

        return def->GetParticleName();
    }

    std::string PhaseLabel(const G4VPhysicalVolume *pv)
    {
        if (!pv)
            return "outside";

        const auto *lv = pv->GetLogicalVolume();
        if (!lv)
            return "outside";

        const auto &lvName = lv->GetName();

        if (lvName == "BN_LV")
            return "BN";
        if (lvName == "ZnS_LV")
            return "ZnS";
        if (lvName == "MatrixLV")
            return "binder_void";
        if (lvName == "WorldLV")
            return "outside";

        return "other";
    }

    G4bool IsOutsidePhase(const std::string &phase)
    {
        return (phase == "outside");
    }
}

// --------------------------------------------------------------------

SteppingAction::SteppingAction(EventAction *eventAction,
                               const PrimaryGeneratorAction *primaryAction)
    : G4UserSteppingAction(),
      fEventAction(eventAction),
      fPrimaryAction(primaryAction)
{
}

// --------------------------------------------------------------------

SteppingAction::~SteppingAction() = default;

// --------------------------------------------------------------------

void SteppingAction::UserSteppingAction(const G4Step *step)
{
    if (!step)
        return;

    auto *track = step->GetTrack();
    if (!track)
        return;

    if (!IsTrackedHeavyParticle(track))
        return;

    const auto *prePoint = step->GetPreStepPoint();
    const auto *postPoint = step->GetPostStepPoint();
    if (!prePoint || !postPoint)
        return;

    const auto *prePV = prePoint->GetPhysicalVolume();
    const auto *postPV = postPoint->GetPhysicalVolume();

    const std::string phasePre = PhaseLabel(prePV);
    const std::string phasePost = PhaseLabel(postPV);

    // Do not keep tracking into world vacuum after leaving the patch.
    // But record the boundary-crossing step itself.
    const G4ThreeVector &xPre = prePoint->GetPosition();
    const G4ThreeVector &xPost = postPoint->GetPosition();

    const G4double stepLen = step->GetStepLength();
    const G4double edep = step->GetTotalEnergyDeposit();
    const G4double ekinPre = prePoint->GetKineticEnergy();
    const G4double ekinPost = postPoint->GetKineticEnergy();

    if (fEventAction)
    {
        fEventAction->AddEdep(edep);
    }

    auto *runAction = fEventAction ? fEventAction->GetRunAction() : nullptr;
    if (runAction && fPrimaryAction)
    {
        // In streaming mode, PrimaryGeneratorAction may move from one
        // input CSV to the next during the same Geant4 run.
        // Make sure the output CSV matches the current input CSV.
        runAction->SwitchOutputCsvForInputPath(fPrimaryAction->GetLoadedInputFile());
    }

    if (runAction && runAction->IsStepCsvOpen())
    {
        std::ofstream &csv = runAction->GetStepCsv();

        const auto &rec = fEventAction->GetCurrentRecord();
        //const auto &capturePos = fEventAction->GetCurrentLocalCapturePosition();
        const auto &bnCenter = fEventAction->GetCurrentSelectedBNCenter();

        csv
            << rec.eventID << ","
            << rec.thickness_um << ","
            << rec.bn_wt << ","
            << rec.zns_wt << ","
            << rec.capture_x_um << ","
            << rec.capture_y_um << ","
            << rec.corr_x_um << ","
            << rec.corr_y_um << ","
            << rec.depth_um << ","
            << fEventAction->GetCurrentSurfaceMode() << ","
            << fEventAction->GetCurrentTargetLocalZ() / um << ","
            << fEventAction->GetCurrentUsedLocalZ() / um << ","
            << bnCenter.x() / um << ","
            << bnCenter.y() / um << ","
            << bnCenter.z() / um << ","
            << track->GetTrackID() << ","
            << track->GetCurrentStepNumber() << ","
            << ParticleLabel(track) << ","
            << phasePre << ","
            << phasePost << ","
            << xPre.x() / um << ","
            << xPre.y() / um << ","
            << xPre.z() / um << ","
            << xPost.x() / um << ","
            << xPost.y() / um << ","
            << xPost.z() / um << ","
            << stepLen / um << ","
            << edep / keV << ","
            << ekinPre / keV << ","
            << ekinPost / keV
            << "\n";
    }

    // Kill track once it exits the microstructure into world/outside
    if (!IsOutsidePhase(phasePre) && IsOutsidePhase(phasePost))
    {
        track->SetTrackStatus(fStopAndKill);
    }
}