// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <string>

class Settings
{
public:
  static Settings* getInstance();

  bool random_rotation;      // True = Random rotation on model insert.
  bool random_tilt;      // True = Random tileing on model insert.
  bool random_size;      // True = Random sizing on model insert.
  bool copy_autoselect;  //
  bool copyModelStats;

  int FarZ;        // the far clipping value

  bool AutoSelectingMode;  // true activates auto selection when you deselect a model. False not.
  bool holelinesOn;    // If true show the blue subchunk lines always. Not only in holemode is on.

  bool tabletMode;
  bool renderModelsWithBox;

  const bool& noAntiAliasing() const;

  std::string wodSavePath;
  std::string importFile;
  std::string wmvLogFile;

private:
  bool _noAntiAliasing;

  Settings();
  static Settings* instance;
};
