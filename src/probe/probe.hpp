#pragma once
#include "../stdinclude.hpp"
#include <string>

namespace probe {
    void Update();
    void DumpScene();

    // Phase 2: Asset Tracking
    void RecordBundleLoad(Il2CppObject* bundle, std::string path);
    void RecordAssetLoad(Il2CppObject* bundle, std::string assetName);
    void DumpAssetMap();
}
