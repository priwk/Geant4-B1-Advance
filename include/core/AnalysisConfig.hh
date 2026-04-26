#ifndef AnalysisConfig_h
#define AnalysisConfig_h 1

#include <string>

enum class RunMode
{
  StageA_NeutronPatch,  // 固定 50x50x30 um^3 微结构 patch 做热中子等效化
  StageB_ReplayAlphaLi, // 读取 capture CSV，重放 alpha / Li7
  StageC_OpticalStub    // 预留光学接口骨架
};

class AnalysisConfig
{
public:
  AnalysisConfig();
  ~AnalysisConfig();

  static const char *RunModeName(RunMode mode);

public:
  // ---- 全局运行模式 ----
  RunMode runMode;

  // ---- 固定微结构基准参数（当前项目中默认不应被随意改动）----
  double patchXY_um;        // 固定 patch 横向尺寸，默认 50 um
  double microThickness_um; // 固定局部厚度，默认 30 um

  // ---- 配比信息 ----
  double bnWt;
  double znsWt;

  // ---- placement 输入控制 ----
  bool useRandomPlacement;
  std::string placementFilePath;

  // ---- Stage B 输入 ----
  std::string captureCsvPath;

  // ---- Stage C 预留输入 ----
  std::string opticalSourcePath;

  // ---- Stage B 深度映射兼容开关 ----
  // true: 允许 thickness_um == local patch thickness
  // 后面会用于替换当前 PrimaryGeneratorAction 里过严的限制
  bool allowThicknessEqualLocalPatch;
};

#endif