#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"

#include <vector>

class G4VPhysicalVolume;
class G4LogicalVolume;
class G4Material;

class DetectorConstruction : public G4VUserDetectorConstruction
{
public:
  DetectorConstruction();
  ~DetectorConstruction() override;

  G4VPhysicalVolume *Construct() override;

  // ----- 几何结构参数 -----
  void SetScreenThicknessUm(G4double thicknessUm); // CSV 中的实际屏幕厚度
  void SetMicroThicknessUm(G4double thicknessUm);  // CSV 中的局部微结构 z 窗口
  void SetPatchXYUm(G4double patchXYUm);           // CSV 中的局部 x/y 窗口
  void SetWeightRatio(G4double bnWt, G4double znsWt);
  void SetPresetRatio(G4int presetIndex);

  G4double GetScreenThicknessUm() const { return fScreenThickness / um; }
  G4double GetMicroThicknessUm() const { return fMicroThickness / um; }
  G4double GetPatchXYUm() const { return fPatchXY / um; }
  G4double GetBnWt() const { return fBnWt; }
  G4double GetZnsWt() const { return fZnsWt; }

  G4double GetEffectiveLocalThickness() const;
  G4double GetFrontSurfaceZ() const { return +0.5 * GetEffectiveLocalThickness(); }
  G4double GetBackSurfaceZ() const { return -0.5 * GetEffectiveLocalThickness(); }

  G4double GetBNRadius() const { return fBNRadius; }
  G4double GetZnSRadius() const { return fZnSRadius; }

  G4double GetSafeMarginXY() const { return fSafeMarginXY; }

  G4LogicalVolume *GetScoringVolume() const { return fScoringVolume; }
  G4LogicalVolume *GetMatrixLogical() const { return fMatrixLogical; }
  G4LogicalVolume *GetBNLogical() const { return fBNLogical; }
  G4LogicalVolume *GetZnSLogical() const { return fZnSLogical; }

  const std::vector<G4ThreeVector> &GetPlacedBNCenters() const { return fPlacedBNCenters; }
  const std::vector<G4ThreeVector> &GetSafeBNCenters() const { return fSafeBNCenters; }
  const std::vector<G4ThreeVector> &GetUsableZnSCandidateCenters() const { return fUsableZnSCandidateCenters; }
  const std::vector<G4ThreeVector> &GetPlacedZnSCenters() const { return fPlacedZnSCenters; }

private:
  void DefineMaterials();
  void NotifyGeometryChanged();

  static G4double HashToUnit(G4int ix, G4int iy, G4int iz, G4int salt);

  G4int ComputeTargetZnSCount(G4int placedBNCount, G4int usableZnSCount) const;
  G4bool IsInsideSafeXY(const G4ThreeVector &pos) const;

private:
  // ---- screen-level thickness ----
  G4double fScreenThickness; // CSV 中的实际全屏厚度

  // ---- local microstructure window ----
  G4double fMicroThickness; // CSV 中的局部 z 窗口
  G4double fPatchXY;        // CSV 中的局部 x/y 窗口

  // ---- particle geometry ----
  G4double fBNRadius;     // BN radius (2.5 um)
  G4double fZnSRadius;    // ZnS radius (1.0 um)
  G4double fBNPitch;      // BN lattice pitch
  G4double fZnSPitch;     // ZnS lattice pitch
  G4double fOverlapGap;   // avoid touching overlaps
  G4double fSafeMarginXY; // avoid choosing BN too close to x/y patch edge

  // ---- background ----
  G4double fVoidVolumeFraction;

  // ---- composition ----
  G4double fBnWt;
  G4double fZnsWt;

  // ---- materials ----
  G4Material *fVacuumMaterial;
  G4Material *fMatrixMaterial;
  G4Material *fBNMaterial;
  G4Material *fZnSMaterial;

  // ---- logical volumes ----
  G4LogicalVolume *fScoringVolume;
  G4LogicalVolume *fWorldLogical;
  G4LogicalVolume *fMatrixLogical;
  G4LogicalVolume *fBNLogical;
  G4LogicalVolume *fZnSLogical;

  // ---- cached geometry centers ----
  std::vector<G4ThreeVector> fPlacedBNCenters;
  std::vector<G4ThreeVector> fSafeBNCenters;
  std::vector<G4ThreeVector> fUsableZnSCandidateCenters;
  std::vector<G4ThreeVector> fPlacedZnSCenters;
};

#endif