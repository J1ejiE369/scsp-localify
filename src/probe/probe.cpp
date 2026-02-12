#include "probe.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <direct.h> // for _mkdir
#include <io.h>     // for _access
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm> // For std::sort
#include <cctype>    // For std::tolower

#include "../il2cpp/il2cpp_symbols.hpp"

using json = nlohmann::json;

namespace probe {

    std::mutex g_assetMapMutex;
    std::unordered_map<std::string, std::string> g_assetToBundleMap;
    std::unordered_map<std::string, std::string> g_bundleToPathMap;
    std::unordered_map<std::string, json> g_characterIndexByFsPath;

    struct Vector3 { float x, y, z; };
    struct Quaternion { float x, y, z, w; };

    // --- Asset Map Recording (Restored for Linker) ---
    void RecordBundleLoad(Il2CppObject* bundle, std::string path) {
        if (!bundle) return;
        static auto method_get_name = il2cpp_symbols::get_method("UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "get_name", 0);
        if (method_get_name) {
            auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(method_get_name, bundle, nullptr, nullptr);
            if (nameStr) {
                std::string bundleName = nameStr->ToUtf8String();
                std::lock_guard<std::mutex> lock(g_assetMapMutex);
                g_bundleToPathMap[bundleName] = path;
            }
        }
    }

    void RecordAssetLoad(Il2CppObject* bundle, std::string assetName) {
        if (!bundle) return;
        static auto method_get_name = il2cpp_symbols::get_method("UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "get_name", 0);
        if (method_get_name) {
            auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(method_get_name, bundle, nullptr, nullptr);
            if (nameStr) {
                std::string bundleName = nameStr->ToUtf8String();
                std::lock_guard<std::mutex> lock(g_assetMapMutex);
                g_assetToBundleMap[assetName] = bundleName;
            }
        }
    }

    // --- Helper for creating directories ---
    void CreateDir(const std::string& path) {
        _mkdir(path.c_str());
    }

    bool DirExists(const std::string& path) {
        return _access(path.c_str(), 0) == 0;
    }

    std::string SanitizeFileName(const std::string& name) {
        std::string safe = name;
        const std::string invalid = "<>:\"/\\|?*";
        for (char& c : safe) {
            if (invalid.find(c) != std::string::npos || c < 32) {
                c = '_';
            }
        }
        return safe;
    }

    std::string GetTimestampString() {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    // Find character root both in filesystem path and logical path:
    // .../ScenarioManager(Clone)/Characters/<CharacterName>/...
    bool TryGetCharacterContextFromPaths(
        const std::string& fsPath,
        const std::string& fullPath,
        std::string& outCharacterFsPath,
        std::string& outCharacterName,
        std::string& outCharacterFullPath
    ) {
        const std::string fsMarker = "/ScenarioManager(Clone)/Characters/";
        const std::string fullMarker = "ScenarioManager(Clone)/Characters/";
        auto fsPos = fsPath.find(fsMarker);
        auto fullPos = fullPath.find(fullMarker);
        if (fsPos == std::string::npos || fullPos == std::string::npos) return false;

        auto fsStart = fsPos + fsMarker.size();
        auto fullStart = fullPos + fullMarker.size();
        auto fsEnd = fsPath.find('/', fsStart);
        auto fullEnd = fullPath.find('/', fullStart);

        if (fsEnd == std::string::npos) fsEnd = fsPath.size();
        if (fullEnd == std::string::npos) fullEnd = fullPath.size();
        if (fsEnd <= fsStart || fullEnd <= fullStart) return false;

        outCharacterName = fsPath.substr(fsStart, fsEnd - fsStart);
        outCharacterFsPath = fsPath.substr(0, fsEnd);
        outCharacterFullPath = fullPath.substr(0, fullEnd);
        return true;
    }

    void EnsureCharacterIndexInitialized(
        const std::string& characterFsPath,
        const std::string& characterName,
        const std::string& characterFullPath
    ) {
        auto& idx = g_characterIndexByFsPath[characterFsPath];
        if (idx.is_null() || idx.empty()) {
            idx["characterName"] = characterName;
            idx["characterFolderPath"] = characterFsPath;
            idx["characterFullPath"] = characterFullPath;
            idx["characterRootInstanceID"] = 0;
            idx["includedMeshes"] = json::array();
            idx["excludedMeshes"] = json::array();
        }
    }

    void AddCharacterMeshRecord(
        const std::string& nodeFsPath,
        const std::string& nodeFullPath,
        int nodeInstanceID,
        int smrInstanceID,
        const std::string& exportTag,
        bool included,
        const std::string& reason
    ) {
        std::string characterFsPath, characterName, characterFullPath;
        if (!TryGetCharacterContextFromPaths(nodeFsPath, nodeFullPath, characterFsPath, characterName, characterFullPath)) {
            return;
        }

        EnsureCharacterIndexInitialized(characterFsPath, characterName, characterFullPath);
        auto& idx = g_characterIndexByFsPath[characterFsPath];
        if (nodeFsPath == characterFsPath && idx["characterRootInstanceID"].get<int>() == 0) {
            idx["characterRootInstanceID"] = nodeInstanceID;
        }

        json rec;
        rec["ownerGameObjectInstanceID"] = nodeInstanceID;
        rec["ownerGameObjectPath"] = nodeFullPath;
        rec["smrInstanceID"] = smrInstanceID;
        rec["exportTag"] = exportTag;
        rec["reason"] = reason;
        rec["meshFile"] = nodeFsPath + "/mesh.json";
        rec["bonesFile"] = nodeFsPath + "/bones.json";
        rec["materialsFile"] = nodeFsPath + "/materials.json";

        if (included) idx["includedMeshes"].push_back(rec);
        else idx["excludedMeshes"].push_back(rec);
    }

    std::string MakeUniqueDumpRoot(const std::string& baseRoot, const std::string& timestamp) {
        std::string root = baseRoot + "/" + timestamp;
        if (!DirExists(root)) return root;

        int suffix = 1;
        while (true) {
            std::ostringstream oss;
            oss << baseRoot << "/" << timestamp << "_" << suffix;
            const std::string candidate = oss.str();
            if (!DirExists(candidate)) return candidate;
            ++suffix;
        }
    }

    int GetObjectInstanceID(Il2CppObject* obj) {
        if (!obj) return 0;
        static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
        static auto method_GetInstanceID = il2cpp_class_get_method_from_name(klass_Object, "GetInstanceID", 0);
        if (!method_GetInstanceID) return 0;
        auto idObj = il2cpp_runtime_invoke(method_GetInstanceID, obj, nullptr, nullptr);
        if (!idObj) return 0;
        return *(int*)il2cpp_object_unbox((Il2CppObject*)idObj);
    }

    std::string GetObjectName(Il2CppObject* obj) {
        if (!obj) return "";
        static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
        static auto prop_name = il2cpp_class_get_method_from_name(klass_Object, "get_name", 0);
        if (!prop_name) return "";
        auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, obj, nullptr, nullptr);
        if (!nameStr) return "";
        return nameStr->ToUtf8String();
    }

    std::string GetGameObjectPath(Il2CppObject* go) {
        if (!go) return "";

        static auto klass_GameObject = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
        static auto klass_Transform = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
        static auto method_get_transform = il2cpp_class_get_method_from_name(klass_GameObject, "get_transform", 0);
        static auto method_get_parent = il2cpp_class_get_method_from_name(klass_Transform, "get_parent", 0);

        auto transform = (Il2CppObject*)il2cpp_runtime_invoke(method_get_transform, go, nullptr, nullptr);
        if (!transform) return GetObjectName(go);

        std::vector<std::string> names;
        names.push_back(GetObjectName(go));
        while (transform) {
            auto parent = (Il2CppObject*)il2cpp_runtime_invoke(method_get_parent, transform, nullptr, nullptr);
            if (!parent) break;
            static auto prop_gameObject = il2cpp_class_get_method_from_name(
                il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Component"),
                "get_gameObject", 0);
            auto parentGO = (Il2CppObject*)il2cpp_runtime_invoke(prop_gameObject, parent, nullptr, nullptr);
            if (!parentGO) break;
            names.push_back(GetObjectName(parentGO));
            transform = parent;
        }

        std::reverse(names.begin(), names.end());
        std::ostringstream oss;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) oss << "/";
            oss << names[i];
        }
        return oss.str();
    }

    // --- Helper to list methods ---
    void ListMethods(Il2CppClass* klass) {
        if (!klass) return;
        void* iter = nullptr;
        while (const MethodInfo* method = il2cpp_class_get_methods(klass, &iter)) {
            std::cout << "Method: " << method->name << std::endl;
        }
    }

    // --- Helper to extract Vector3 Array ---
    json ExtractVector3Array(Il2CppArray* arr) {
        json j = json::array();
        if (!arr) return j;
        
        // Il2CppArray header is usually 32 bytes on x64
        // Data starts immediately after. Vector3 is 3 floats (12 bytes).
        // We need to be careful about alignment/padding, but usually packed for arrays.
        int count = il2cpp_array_length(arr);
        uint8_t* dataPtr = (uint8_t*)arr + 32; 

        for (int i = 0; i < count; i++) {
            Vector3* v = (Vector3*)(dataPtr + i * 12); // sizeof(Vector3) = 12
            j.push_back({ v->x, v->y, v->z });
        }
        return j;
    }

    // --- Helper to extract Vector2 Array ---
    struct Vector2 { float x, y; };
    json ExtractVector2Array(Il2CppArray* arr) {
        json j = json::array();
        if (!arr) return j;
        
        int count = il2cpp_array_length(arr);
        uint8_t* dataPtr = (uint8_t*)arr + 32; 

        for (int i = 0; i < count; i++) {
            Vector2* v = (Vector2*)(dataPtr + i * 8); // sizeof(Vector2) = 8
            j.push_back({ v->x, v->y });
        }
        return j;
    }

    // --- Helper to extract Matrix4x4 Array ---
    struct Matrix4x4 { float m00, m10, m20, m30, m01, m11, m21, m31, m02, m12, m22, m32, m03, m13, m23, m33; };
    json ExtractMatrixArray(Il2CppArray* arr) {
        json j = json::array();
        if (!arr) return j;
        
        int count = il2cpp_array_length(arr);
        uint8_t* dataPtr = (uint8_t*)arr + 32; 

        for (int i = 0; i < count; i++) {
            Matrix4x4* m = (Matrix4x4*)(dataPtr + i * 64); // sizeof(Matrix4x4) = 16 * 4 = 64
            // GLTF uses column-major order, Unity is column-major too?
            // Unity Matrix4x4 memory layout:
            // m00, m10, m20, m30 (column 0)
            // m01, m11, m21, m31 (column 1) ...
            // GLTF expects a flat array of 16 floats in column-major order.
            // So we can just dump the floats.
            j.push_back({
                m->m00, m->m10, m->m20, m->m30,
                m->m01, m->m11, m->m21, m->m31,
                m->m02, m->m12, m->m22, m->m32,
                m->m03, m->m13, m->m23, m->m33
            });
        }
        return j;
    }

    // --- Helper to extract int Array (Indices) ---
    json ExtractIntArray(Il2CppArray* arr) {
        json j = json::array();
        if (!arr) return j;
        
        int count = il2cpp_array_length(arr);
        uint8_t* dataPtr = (uint8_t*)arr + 32; 

        for (int i = 0; i < count; i++) {
            int* v = (int*)(dataPtr + i * 4);
            j.push_back(*v);
        }
        return j;
    }

    // --- Helper to extract BoneWeight Array ---
    struct BoneWeight {
        float weight0, weight1, weight2, weight3;
        int boneIndex0, boneIndex1, boneIndex2, boneIndex3;
    };
    
    json ExtractBoneWeightArray(Il2CppArray* arr) {
        json j = json::array();
        if (!arr) return j;
        
        int count = il2cpp_array_length(arr);
        uint8_t* dataPtr = (uint8_t*)arr + 32; 

        for (int i = 0; i < count; i++) {
            BoneWeight* bw = (BoneWeight*)(dataPtr + i * 32); // sizeof(BoneWeight) = 32
            j.push_back({
                {"weights", {bw->weight0, bw->weight1, bw->weight2, bw->weight3}},
                {"indices", {bw->boneIndex0, bw->boneIndex1, bw->boneIndex2, bw->boneIndex3}}
            });
        }
        return j;
    }

    // --- Helper for New BoneWeight API ---
    struct BoneWeight1 {
        float weight;
        int boneIndex;
    };

    json ExtractBoneWeightsFromNewAPI(Il2CppObject* mesh, int vertexCount) {
        json j = json::array();
        
        std::cout << "  [Debug] Extracting BoneWeights via New API (IntPtr mode)..." << std::endl;

        static auto klass_Mesh = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Mesh");
        static auto method_GetBonesPerVertexArray = il2cpp_class_get_method_from_name(klass_Mesh, "GetBonesPerVertexArray", 0);
        static auto method_GetAllBoneWeightsArray = il2cpp_class_get_method_from_name(klass_Mesh, "GetAllBoneWeightsArray", 0);

        if (!method_GetBonesPerVertexArray || !method_GetAllBoneWeightsArray) {
            std::cout << "  [Error] New BoneWeight APIs not found." << std::endl;
            return j;
        }

        auto countsObj = il2cpp_runtime_invoke(method_GetBonesPerVertexArray, mesh, nullptr, nullptr);
        auto weightsObj = il2cpp_runtime_invoke(method_GetAllBoneWeightsArray, mesh, nullptr, nullptr);

        if (!countsObj || !weightsObj) {
             std::cout << "  [Error] New BoneWeight API returned null objects." << std::endl;
             return j;
        }

        // Helper to get data pointer from IntPtr
        auto GetDataPointer = [](Il2CppObject* obj, const char* name) -> void* {
            Il2CppClass* klass = il2cpp_object_get_class(obj);
            std::string className = il2cpp_class_get_name(klass);
            std::cout << "  [Debug] " << name << " Class: " << className << std::endl;
            
            if (className == "IntPtr") {
                // IntPtr is a struct { void* m_value; }
                // Boxed struct layout: Header + Fields
                // On x64, Header is 16 bytes (vtable + monitor), so m_value is at offset 16.
                // We verified this with debug output previously.
                return *(void**)((uint8_t*)obj + 16);
            }
            return nullptr;
        };

        void* countsPtr = GetDataPointer((Il2CppObject*)countsObj, "countsObj");
        void* weightsPtr = GetDataPointer((Il2CppObject*)weightsObj, "weightsObj");

        if (!countsPtr || !weightsPtr) {
            std::cout << "  [Error] Failed to get data pointers (not IntPtr?)." << std::endl;
            return j;
        }

        uint8_t* countsData = (uint8_t*)countsPtr;
        BoneWeight1* weightsData = (BoneWeight1*)weightsPtr;
        
        std::cout << "  [Debug] Pointers obtained. Counts: " << countsPtr << ", Weights: " << weightsPtr << std::endl;
        std::cout << "  [Debug] VertexCount: " << vertexCount << std::endl;

        int weightIdx = 0;

        for (int i = 0; i < vertexCount; i++) {
            uint8_t count = countsData[i];
            
            // Collect weights for this vertex
            std::vector<BoneWeight1> vertexWeights;
            for (int k = 0; k < count; k++) {
                // We don't know total weight count easily, so rely on valid data
                BoneWeight1 bw = weightsData[weightIdx];
                vertexWeights.push_back(bw);
                weightIdx++;
            }

            // Sort by weight descending
            std::sort(vertexWeights.begin(), vertexWeights.end(), [](const BoneWeight1& a, const BoneWeight1& b) {
                return a.weight > b.weight;
            });

            // Take top 4
            float w[4] = {0,0,0,0};
            int ind[4] = {0,0,0,0};
            
            float totalWeight = 0;
            for(int k=0; k < std::min((int)vertexWeights.size(), 4); ++k) {
                w[k] = vertexWeights[k].weight;
                ind[k] = vertexWeights[k].boneIndex;
                totalWeight += w[k];
            }

            // Normalize
            if (totalWeight > 0) {
                for(int k=0; k<4; ++k) w[k] /= totalWeight;
            }

            j.push_back({
                {"weights", {w[0], w[1], w[2], w[3]}},
                {"indices", {ind[0], ind[1], ind[2], ind[3]}}
            });
        }
        
        std::cout << "  [Debug] Extraction finished successfully." << std::endl;
        return j;
    }

    std::string GetIl2CppClassFullName(Il2CppObject* obj) {
        if (!obj) return "";
        auto klass = il2cpp_object_get_class(obj);
        if (!klass) return "";
        const char* ns = il2cpp_class_get_namespace(klass);
        const char* name = il2cpp_class_get_name(klass);
        if (!name) return "";
        if (ns && ns[0] != '\0') return std::string(ns) + "." + std::string(name);
        return std::string(name);
    }

    bool TryExtractMeshDataViaMeshDataAPI(Il2CppObject* mesh, json& data) {
        if (!mesh) return false;

        static auto klass_Mesh = (Il2CppClass*)il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Mesh");
        if (!klass_Mesh) return false;

        static auto method_AcquireReadOnlyMeshData =
            il2cpp_class_get_method_from_name(klass_Mesh, "AcquireReadOnlyMeshData", 1);
        static Il2CppClass* klass_MeshDataArray = nullptr;
        static Il2CppClass* klass_MeshData = nullptr;
        static std::string meshDataArrayClassSource = "unknown";
        static std::string meshDataClassSource = "unknown";
        if (!klass_MeshDataArray) {
            klass_MeshDataArray = il2cpp_symbols::get_nested_class(klass_Mesh, "MeshDataArray");
            if (klass_MeshDataArray) meshDataArrayClassSource = "nested_under_mesh";
            if (!klass_MeshDataArray) {
                klass_MeshDataArray = (Il2CppClass*)il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "MeshDataArray");
                if (klass_MeshDataArray) meshDataArrayClassSource = "unityengine_direct";
            }
            if (!klass_MeshDataArray) {
                klass_MeshDataArray = (Il2CppClass*)il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine.Rendering", "MeshDataArray");
                if (klass_MeshDataArray) meshDataArrayClassSource = "unityengine_rendering_direct";
            }
        }
        if (!klass_MeshData) {
            klass_MeshData = il2cpp_symbols::get_nested_class(klass_Mesh, "MeshData");
            if (klass_MeshData) meshDataClassSource = "nested_under_mesh";
            if (!klass_MeshData) {
                klass_MeshData = (Il2CppClass*)il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "MeshData");
                if (klass_MeshData) meshDataClassSource = "unityengine_direct";
            }
            if (!klass_MeshData) {
                klass_MeshData = (Il2CppClass*)il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine.Rendering", "MeshData");
                if (klass_MeshData) meshDataClassSource = "unityengine_rendering_direct";
            }
        }

        data["meshDataArrayClassResolved"] = (klass_MeshDataArray != nullptr);
        data["meshDataClassResolved"] = (klass_MeshData != nullptr);
        data["meshDataArrayClassSource"] = meshDataArrayClassSource;
        data["meshDataClassSource"] = meshDataClassSource;
        if (!method_AcquireReadOnlyMeshData) {
            data["meshDataFallbackError"] = "AcquireReadOnlyMeshData_method_missing";
            return false;
        }
        if (!klass_MeshDataArray || !klass_MeshData) {
            data["meshDataFallbackError"] = "MeshData_or_MeshDataArray_class_missing";
            return false;
        }

        Il2CppObject* exception = nullptr;
        void* argsAcquire[] = { mesh };
        auto meshDataArrayObj = (Il2CppObject*)il2cpp_runtime_invoke(
            (MethodInfo*)method_AcquireReadOnlyMeshData, nullptr, argsAcquire, &exception);
        if (!meshDataArrayObj || exception) {
            data["meshDataFallbackStep"] = "AcquireReadOnlyMeshData";
            data["meshDataFallbackError"] = "AcquireReadOnlyMeshData_failed";
            if (exception) data["meshDataExceptionType"] = GetIl2CppClassFullName(exception);
            std::cout << "  [MeshData] AcquireReadOnlyMeshData failed";
            if (exception) std::cout << " with exception " << GetIl2CppClassFullName(exception);
            std::cout << std::endl;
            return false;
        }

        auto method_mda_length = il2cpp_class_get_method_from_name(klass_MeshDataArray, "get_Length", 0);
        auto method_mda_item = il2cpp_class_get_method_from_name(klass_MeshDataArray, "get_Item", 1);
        auto method_mda_dispose = il2cpp_class_get_method_from_name(klass_MeshDataArray, "Dispose", 0);
        if (!method_mda_length || !method_mda_item) {
            if (method_mda_dispose) il2cpp_runtime_invoke(method_mda_dispose, meshDataArrayObj, nullptr, nullptr);
            data["meshDataFallbackStep"] = "MeshDataArray_methods";
            data["meshDataFallbackError"] = "MeshDataArray_get_Length_or_get_Item_missing";
            return false;
        }

        int length = 0;
        auto lenObj = il2cpp_runtime_invoke((MethodInfo*)method_mda_length, meshDataArrayObj, nullptr, &exception);
        if (lenObj && !exception) length = *(int*)il2cpp_object_unbox((Il2CppObject*)lenObj);
        if (length <= 0) {
            if (method_mda_dispose) il2cpp_runtime_invoke(method_mda_dispose, meshDataArrayObj, nullptr, nullptr);
            data["meshDataFallbackStep"] = "MeshDataArray_get_Length";
            data["meshDataFallbackError"] = "MeshDataArray_length_zero_or_failed";
            if (exception) data["meshDataExceptionType"] = GetIl2CppClassFullName(exception);
            return false;
        }

        int zero = 0;
        void* argsItem[] = { &zero };
        auto meshDataObj = (Il2CppObject*)il2cpp_runtime_invoke(
            (MethodInfo*)method_mda_item, meshDataArrayObj, argsItem, &exception);
        if (!meshDataObj || exception) {
            if (method_mda_dispose) il2cpp_runtime_invoke(method_mda_dispose, meshDataArrayObj, nullptr, nullptr);
            data["meshDataFallbackStep"] = "MeshDataArray_get_Item";
            data["meshDataFallbackError"] = "MeshDataArray_get_Item_failed";
            if (exception) data["meshDataExceptionType"] = GetIl2CppClassFullName(exception);
            return false;
        }

        auto tryInvokeArray = [&](const char* methodName, int argc) -> Il2CppArray* {
            auto m = il2cpp_class_get_method_from_name(klass_MeshData, methodName, argc);
            if (!m) return nullptr;
            Il2CppObject* exc = nullptr;
            void* ret = nullptr;
            if (argc == 0) {
                ret = il2cpp_runtime_invoke((MethodInfo*)m, meshDataObj, nullptr, &exc);
            } else {
                int arg0 = 0;
                void* args[] = { &arg0 };
                ret = il2cpp_runtime_invoke((MethodInfo*)m, meshDataObj, args, &exc);
            }
            if (exc || !ret) return nullptr;
            return (Il2CppArray*)ret;
        };

        auto tryInvokeInt = [&](const char* methodName, int argc, int fallback = 0) -> int {
            auto m = il2cpp_class_get_method_from_name(klass_MeshData, methodName, argc);
            if (!m) return fallback;
            Il2CppObject* exc = nullptr;
            void* ret = nullptr;
            if (argc == 0) {
                ret = il2cpp_runtime_invoke((MethodInfo*)m, meshDataObj, nullptr, &exc);
            } else {
                int arg0 = 0;
                void* args[] = { &arg0 };
                ret = il2cpp_runtime_invoke((MethodInfo*)m, meshDataObj, args, &exc);
            }
            if (exc || !ret) return fallback;
            return *(int*)il2cpp_object_unbox((Il2CppObject*)ret);
        };

        int meshDataVertexCount = tryInvokeInt("GetVertexCount", 0, 0);
        int meshDataIndexCount = tryInvokeInt("GetIndexCount", 0, 0);
        if (meshDataIndexCount <= 0) meshDataIndexCount = tryInvokeInt("GetIndexCount", 1, 0);

        auto vertsArray = tryInvokeArray("GetVertices", 0);
        if (!vertsArray) vertsArray = tryInvokeArray("GetVertices", 1);
        auto normalsArray = tryInvokeArray("GetNormals", 0);
        if (!normalsArray) normalsArray = tryInvokeArray("GetNormals", 1);
        auto uvArray = tryInvokeArray("GetUVs", 0);
        if (!uvArray) uvArray = tryInvokeArray("GetUVs", 1);
        auto indicesArray = tryInvokeArray("GetIndices", 0);
        if (!indicesArray) indicesArray = tryInvokeArray("GetIndices", 1);

        bool ok = false;
        if (vertsArray) {
            data["vertices"] = ExtractVector3Array(vertsArray);
            data["vertexArrayCount"] = il2cpp_array_length(vertsArray);
            data["vertexCount"] = data["vertexArrayCount"];
            ok = (data["vertexArrayCount"].get<int>() > 0);
        } else if (meshDataVertexCount > 0) {
            // API exists but direct vertices call returned null; still record diagnostic count.
            data["vertexArrayCount"] = 0;
            data["vertexCount"] = meshDataVertexCount;
        }

        if (normalsArray) data["normals"] = ExtractVector3Array(normalsArray);
        if (uvArray) data["uv"] = ExtractVector2Array(uvArray);
        if (indicesArray) data["indices"] = ExtractIntArray(indicesArray);

        if ((!indicesArray || data["indices"].empty()) && meshDataIndexCount > 0) {
            data["indexCountReportedByMeshData"] = meshDataIndexCount;
        }
        data["meshDataArrayLength"] = length;
        data["meshDataVertexCount"] = meshDataVertexCount;
        data["meshDataIndexCount"] = meshDataIndexCount;

        if (method_mda_dispose) {
            il2cpp_runtime_invoke((MethodInfo*)method_mda_dispose, meshDataArrayObj, nullptr, nullptr);
        }

        if (!ok) {
            data["meshDataFallbackStep"] = "MeshData_GetVertices_or_GetIndices";
            data["meshDataFallbackError"] = "MeshData_calls_returned_empty";
        } else {
            data["meshDataFallbackError"] = "";
        }

        return ok;
    }

    // --- Mesh Data Extraction ---
    json ExtractMeshData(Il2CppObject* mesh) {
        json data;
        if (!mesh) return data;

        try {
            static auto klass_Mesh = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Mesh");
            if (!klass_Mesh) { std::cout << "Error: Mesh class not found" << std::endl; return data; }

            data["meshInstanceID"] = GetObjectInstanceID(mesh);
            data["meshName"] = GetObjectName(mesh);
            data["vertexCount"] = 0;
            data["vertexArrayCount"] = 0;
            data["vertices"] = json::array();
            data["normals"] = json::array();
            data["uv"] = json::array();
            data["indices"] = json::array();

            // Mesh accessibility diagnostics (critical for "vertexCount>0 but vertices empty" cases).
            static auto prop_vertexCount = il2cpp_class_get_method_from_name(klass_Mesh, "get_vertexCount", 0);
            static auto prop_isReadable = il2cpp_class_get_method_from_name(klass_Mesh, "get_isReadable", 0);
            static auto prop_canAccess = il2cpp_class_get_method_from_name(klass_Mesh, "get_canAccess", 0);
            if (prop_vertexCount) {
                auto vtxObj = il2cpp_runtime_invoke(prop_vertexCount, mesh, nullptr, nullptr);
                if (vtxObj) {
                    data["vertexCountReported"] = *(int*)il2cpp_object_unbox((Il2CppObject*)vtxObj);
                    data["vertexCount"] = data["vertexCountReported"];
                }
            }
            if (prop_isReadable) {
                auto readableObj = il2cpp_runtime_invoke(prop_isReadable, mesh, nullptr, nullptr);
                if (readableObj) data["isReadable"] = *(bool*)il2cpp_object_unbox((Il2CppObject*)readableObj);
            }
            if (prop_canAccess) {
                auto accessObj = il2cpp_runtime_invoke(prop_canAccess, mesh, nullptr, nullptr);
                if (accessObj) data["canAccess"] = *(bool*)il2cpp_object_unbox((Il2CppObject*)accessObj);
            }
            
            // Vertices
            static auto prop_vertices = il2cpp_class_get_method_from_name(klass_Mesh, "get_vertices", 0);
            if (prop_vertices) {
                auto vertsArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_vertices, mesh, nullptr, nullptr);
                if (vertsArray) {
                    data["vertexArrayCount"] = il2cpp_array_length(vertsArray);
                    data["vertexCount"] = data["vertexArrayCount"];
                    data["vertices"] = ExtractVector3Array(vertsArray);
                }
            }

            // Normals
            static auto prop_normals = il2cpp_class_get_method_from_name(klass_Mesh, "get_normals", 0);
            if (prop_normals) {
                auto normalsArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_normals, mesh, nullptr, nullptr);
                if (normalsArray) {
                    data["normals"] = ExtractVector3Array(normalsArray);
                }
            }

            // UVs
            static auto prop_uv = il2cpp_class_get_method_from_name(klass_Mesh, "get_uv", 0);
            if (prop_uv) {
                auto uvArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_uv, mesh, nullptr, nullptr);
                if (uvArray) {
                    data["uv"] = ExtractVector2Array(uvArray);
                }
            }

            // Triangles (Indices)
            static auto prop_triangles = il2cpp_class_get_method_from_name(klass_Mesh, "get_triangles", 0);
            if (prop_triangles) {
                auto trisArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_triangles, mesh, nullptr, nullptr);
                if (trisArray) {
                    data["indices"] = ExtractIntArray(trisArray);
                }
            }

            // Fallback for non-readable meshes (e.g. hair): try Mesh.AcquireReadOnlyMeshData path.
            if (data.value("vertexArrayCount", 0) == 0) {
                const bool gotMeshData = TryExtractMeshDataViaMeshDataAPI(mesh, data);
                data["meshDataFallbackUsed"] = true;
                data["meshDataFallbackSuccess"] = gotMeshData;
            } else {
                data["meshDataFallbackUsed"] = false;
                data["meshDataFallbackSuccess"] = false;
            }

            // SubMesh information (better material alignment during reconstruction)
            static auto prop_subMeshCount = il2cpp_class_get_method_from_name(klass_Mesh, "get_subMeshCount", 0);
            static auto method_GetIndices = il2cpp_class_get_method_from_name(klass_Mesh, "GetIndices", 1);
            if (prop_subMeshCount) {
                auto subMeshCountObj = il2cpp_runtime_invoke(prop_subMeshCount, mesh, nullptr, nullptr);
                if (subMeshCountObj) {
                    int subMeshCount = *(int*)il2cpp_object_unbox((Il2CppObject*)subMeshCountObj);
                    data["subMeshCount"] = subMeshCount;
                    data["subMeshes"] = json::array();

                    if (method_GetIndices) {
                        for (int i = 0; i < subMeshCount; ++i) {
                            void* argsSub[] = { &i };
                            auto subIdxArray = (Il2CppArray*)il2cpp_runtime_invoke(method_GetIndices, mesh, argsSub, nullptr);
                            json subMesh;
                            subMesh["submeshIndex"] = i;
                            if (subIdxArray) {
                                subMesh["indices"] = ExtractIntArray(subIdxArray);
                            }
                            data["subMeshes"].push_back(subMesh);
                        }
                    }
                }
            }

            // Bone Weights
            static auto prop_boneWeights = il2cpp_class_get_method_from_name(klass_Mesh, "get_boneWeights", 0);
            if (prop_boneWeights) {
                auto weightsArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_boneWeights, mesh, nullptr, nullptr);
                if (weightsArray) {
                    data["boneWeightCount"] = il2cpp_array_length(weightsArray);
                    data["boneWeights"] = ExtractBoneWeightArray(weightsArray);
                } else {
                    // Try new API if old one returns null (though usually it just doesn't exist)
                     std::cout << "get_boneWeights returned null. Trying new API..." << std::endl;
                     int vCount = data.value("vertexCount", 0);
                     data["boneWeights"] = ExtractBoneWeightsFromNewAPI(mesh, vCount);
                     if (data["boneWeights"].size() > 0) {
                         data["boneWeightCount"] = data["boneWeights"].size();
                         std::cout << "Successfully extracted " << data["boneWeightCount"] << " weights via new API." << std::endl;
                     }
                }
            } else {
                std::cout << "get_boneWeights not found. Trying new API..." << std::endl;
                int vCount = data.value("vertexCount", 0);
                data["boneWeights"] = ExtractBoneWeightsFromNewAPI(mesh, vCount);
                if (data["boneWeights"].size() > 0) {
                    data["boneWeightCount"] = data["boneWeights"].size();
                    std::cout << "Successfully extracted " << data["boneWeightCount"] << " weights via new API." << std::endl;
                } else {
                    std::cout << "Failed to extract bone weights via new API too." << std::endl;
                }
            }

            // Bindposes
            static auto prop_bindposes = il2cpp_class_get_method_from_name(klass_Mesh, "get_bindposes", 0);
            if (prop_bindposes) {
                auto bindposesArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_bindposes, mesh, nullptr, nullptr);
                if (bindposesArray) {
                    data["bindposeCount"] = il2cpp_array_length(bindposesArray);
                    data["bindposes"] = ExtractMatrixArray(bindposesArray);
                }
            }
        } catch (...) {
            std::cout << "Exception in ExtractMeshData" << std::endl;
        }

        return data;
    }

    // --- Bone Data Extraction ---
    json ExtractBoneData(Il2CppObject* smr, Il2CppObject* ownerGo) {
        json data;
        if (!smr) return data;

        try {
            static auto klass_SMR = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer");
            if (!klass_SMR) { std::cout << "Error: SMR class not found" << std::endl; return data; }

            data["smrInstanceID"] = GetObjectInstanceID(smr);
            data["ownerGameObjectInstanceID"] = GetObjectInstanceID(ownerGo);
            data["ownerGameObjectPath"] = GetGameObjectPath(ownerGo);
            
            // Bones
            static auto prop_bones = il2cpp_class_get_method_from_name(klass_SMR, "get_bones", 0);
            if (prop_bones) {
                auto bonesArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_bones, smr, nullptr, nullptr);
                
                if (bonesArray) {
                    int count = il2cpp_array_length(bonesArray);
                    data["boneCount"] = count;
                    
                    json boneNames = json::array();
                    json detailedBones = json::array();
                    static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
                    static auto prop_name = il2cpp_class_get_method_from_name(klass_Object, "get_name", 0);
                    static auto prop_gameObject = il2cpp_class_get_method_from_name(
                        il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Component"),
                        "get_gameObject", 0);

                    for (int i = 0; i < count; i++) {
                        auto boneTransform = (Il2CppObject*)il2cpp_symbols::array_get_value(bonesArray, i);
                        json detailed;
                        detailed["indexInSmrBones"] = i;
                        if (boneTransform) {
                            auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, boneTransform, nullptr, nullptr);
                            std::string boneName = nameStr ? nameStr->ToUtf8String() : "null";
                            boneNames.push_back(boneName);
                            detailed["name"] = boneName;
                            detailed["transformInstanceID"] = GetObjectInstanceID(boneTransform);
                            auto boneGO = (Il2CppObject*)il2cpp_runtime_invoke(prop_gameObject, boneTransform, nullptr, nullptr);
                            detailed["gameObjectInstanceID"] = GetObjectInstanceID(boneGO);
                            detailed["fullPath"] = GetGameObjectPath(boneGO);
                            detailed["isNull"] = false;
                        } else {
                            boneNames.push_back("null");
                            detailed["name"] = "null";
                            detailed["transformInstanceID"] = 0;
                            detailed["gameObjectInstanceID"] = 0;
                            detailed["fullPath"] = "";
                            detailed["isNull"] = true;
                        }
                        detailedBones.push_back(detailed);
                    }
                    data["bones"] = boneNames;
                    data["bonesDetailed"] = detailedBones;
                }
            }
            
            // Root Bone
            static auto prop_rootBone = il2cpp_class_get_method_from_name(klass_SMR, "get_rootBone", 0);
            if (prop_rootBone) {
                auto rootBone = (Il2CppObject*)il2cpp_runtime_invoke(prop_rootBone, smr, nullptr, nullptr);
                if (rootBone) {
                    static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
                    static auto prop_name = il2cpp_class_get_method_from_name(klass_Object, "get_name", 0);
                    auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, rootBone, nullptr, nullptr);
                    if (nameStr) data["rootBone"] = nameStr->ToUtf8String();
                    data["rootBoneTransformInstanceID"] = GetObjectInstanceID(rootBone);
                    static auto prop_gameObject = il2cpp_class_get_method_from_name(
                        il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Component"),
                        "get_gameObject", 0);
                    auto rootBoneGO = (Il2CppObject*)il2cpp_runtime_invoke(prop_gameObject, rootBone, nullptr, nullptr);
                    data["rootBoneGameObjectInstanceID"] = GetObjectInstanceID(rootBoneGO);
                    data["rootBonePath"] = GetGameObjectPath(rootBoneGO);
                }
            }
        } catch (...) {
            std::cout << "Exception in ExtractBoneData" << std::endl;
        }

        return data;
    }

    // --- Material Data Extraction ---
    json ExtractMaterialData(Il2CppObject* smr, Il2CppObject* ownerGo) {
        json data = json::array();
        if (!smr) return data;

        try {
            static auto klass_Renderer = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
            static auto prop_materials = il2cpp_class_get_method_from_name(klass_Renderer, "get_materials", 0);
            
            if (prop_materials) {
                auto matsArray = (Il2CppArray*)il2cpp_runtime_invoke(prop_materials, smr, nullptr, nullptr);
                if (matsArray) {
                    int count = il2cpp_array_length(matsArray);
                    
                    static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
                    static auto prop_name = il2cpp_class_get_method_from_name(klass_Object, "get_name", 0);
                    
                    static auto klass_Material = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Material");
                    static auto prop_shader = il2cpp_class_get_method_from_name(klass_Material, "get_shader", 0);
                    static auto prop_mainTexture = il2cpp_class_get_method_from_name(klass_Material, "get_mainTexture", 0);

                    for (int i = 0; i < count; i++) {
                        json matJson;
                        auto material = (Il2CppObject*)il2cpp_symbols::array_get_value(matsArray, i);
                        matJson["materialSlot"] = i;
                        matJson["smrInstanceID"] = GetObjectInstanceID(smr);
                        matJson["ownerGameObjectInstanceID"] = GetObjectInstanceID(ownerGo);
                        
                        if (material) {
                            matJson["materialInstanceID"] = GetObjectInstanceID(material);
                            // Name
                            auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, material, nullptr, nullptr);
                            if (nameStr) matJson["name"] = nameStr->ToUtf8String();

                            // Shader
                            if (prop_shader) {
                                auto shader = (Il2CppObject*)il2cpp_runtime_invoke(prop_shader, material, nullptr, nullptr);
                                if (shader) {
                                    auto shaderNameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, shader, nullptr, nullptr);
                                    if (shaderNameStr) matJson["shader"] = shaderNameStr->ToUtf8String();
                                }
                            }

                            // Main Texture
                            if (prop_mainTexture) {
                                auto texture = (Il2CppObject*)il2cpp_runtime_invoke(prop_mainTexture, material, nullptr, nullptr);
                                if (texture) {
                                    matJson["mainTextureInstanceID"] = GetObjectInstanceID(texture);
                                    auto texNameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, texture, nullptr, nullptr);
                                    if (texNameStr) matJson["mainTexture"] = texNameStr->ToUtf8String();
                                }
                            }
                        }
                        data.push_back(matJson);
                    }
                }
            }
        } catch (...) {
            std::cout << "Exception in ExtractMaterialData" << std::endl;
        }
        return data;
    }

    // Modified Serialize to include extraction trigger
    json SerializeGameObject(Il2CppObject* go, const std::string& parentPath, int parentInstanceID = 0) {
        if (!go) return nullptr;
        json node;

        // Get Name
        static auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
        static auto prop_name = il2cpp_class_get_method_from_name(klass_Object, "get_name", 0);
        
        std::string name = "GameObject";
        if (prop_name) {
             auto nameStr = (Il2CppString*)il2cpp_runtime_invoke(prop_name, go, nullptr, nullptr);
             if (nameStr) name = nameStr->ToUtf8String();
        }
        const int goInstanceID = GetObjectInstanceID(go);
        node["name"] = name;
        node["instanceID"] = goInstanceID;
        node["parentInstanceID"] = parentInstanceID;
        node["fullPath"] = GetGameObjectPath(go);
        
        // Construct Path for this GameObject
        std::string safeName = SanitizeFileName(name);
        // Avoid accidental overwrite when siblings share same name.
        std::string folderName = safeName + "__" + std::to_string(goInstanceID);
        std::string currentPath = parentPath + "/" + folderName;
        node["dumpFolderName"] = folderName;
        CreateDir(currentPath);

        // Get Active
        static auto klass_GameObject = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
        static auto method_get_activeSelf = il2cpp_class_get_method_from_name(klass_GameObject, "get_activeSelf", 0);
        static auto method_get_activeInHierarchy = il2cpp_class_get_method_from_name(klass_GameObject, "get_activeInHierarchy", 0);
        if (method_get_activeSelf) {
            auto activeObj = il2cpp_runtime_invoke(method_get_activeSelf, go, nullptr, nullptr);
            if (activeObj) {
                bool active = *(bool*)il2cpp_object_unbox((Il2CppObject*)activeObj);
                node["active"] = active;
            }
        }
        if (method_get_activeInHierarchy) {
            auto activeObj = il2cpp_runtime_invoke(method_get_activeInHierarchy, go, nullptr, nullptr);
            if (activeObj) {
                bool active = *(bool*)il2cpp_object_unbox((Il2CppObject*)activeObj);
                node["activeInHierarchy"] = active;
            }
        }

        // Check for SkinnedMeshRenderer and Extract
        static auto method_GetComponent = il2cpp_class_get_method_from_name(klass_GameObject, "GetComponent", 1);
        static auto klass_SMR = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer");
        
        if (klass_SMR && method_GetComponent) {
            static auto type_SMR = il2cpp_class_get_type(klass_SMR);
            static auto reftype_SMR = il2cpp_type_get_object(type_SMR);
            void* argsSMR[] = { reftype_SMR };
            auto smr = (Il2CppObject*)il2cpp_runtime_invoke(method_GetComponent, go, argsSMR, nullptr);
            
            if (smr) {
                node["component"] = "SkinnedMeshRenderer";
                const int smrInstanceID = GetObjectInstanceID(smr);
                node["smrInstanceID"] = smrInstanceID;
                std::cout << "  Found SMR on: " << name << ". Extracting to " << currentPath << std::endl;

                static auto klass_Renderer = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
                static auto method_get_enabled = il2cpp_class_get_method_from_name(klass_Renderer, "get_enabled", 0);
                bool rendererEnabled = true;
                if (method_get_enabled) {
                    auto enabledObj = il2cpp_runtime_invoke(method_get_enabled, smr, nullptr, nullptr);
                    if (enabledObj) rendererEnabled = *(bool*)il2cpp_object_unbox((Il2CppObject*)enabledObj);
                }
                node["rendererEnabled"] = rendererEnabled;
                std::string lowerPath = node["fullPath"].get<std::string>();
                std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                    [](unsigned char ch) { return (char)std::tolower(ch); });
                const bool isProxy =
                    lowerPath.find("swaybone") != std::string::npos ||
                    lowerPath.find("magicacloth") != std::string::npos ||
                    lowerPath.find("collider") != std::string::npos ||
                    lowerPath.find("spring") != std::string::npos ||
                    lowerPath.find("hitradius") != std::string::npos;
                node["exportTag"] = isProxy ? "proxy" : "render";
                std::string excludeReason;
                bool includeForAssemble = !isProxy;
                if (isProxy) excludeReason = "path_keyword_proxy";
                
                // Extract Mesh
                static auto prop_sharedMesh = il2cpp_class_get_method_from_name(klass_SMR, "get_sharedMesh", 0);
                static auto klass_MeshFilter = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "MeshFilter");
                static auto prop_meshFilterSharedMesh = klass_MeshFilter
                    ? il2cpp_class_get_method_from_name(klass_MeshFilter, "get_sharedMesh", 0)
                    : nullptr;
                bool hasMeshFile = false;
                bool usedMeshFilterFallback = false;
                if (prop_sharedMesh || (klass_MeshFilter && prop_meshFilterSharedMesh)) {
                    Il2CppObject* mesh = nullptr;
                    std::string meshSource = "none";

                    if (prop_sharedMesh) {
                        mesh = (Il2CppObject*)il2cpp_runtime_invoke(prop_sharedMesh, smr, nullptr, nullptr);
                        if (mesh) meshSource = "smr_sharedMesh";
                    }

                    if (!mesh && klass_MeshFilter && method_GetComponent && prop_meshFilterSharedMesh) {
                        static auto type_MeshFilter = il2cpp_class_get_type(klass_MeshFilter);
                        static auto reftype_MeshFilter = il2cpp_type_get_object(type_MeshFilter);
                        void* argsMF[] = { reftype_MeshFilter };
                        auto meshFilter = (Il2CppObject*)il2cpp_runtime_invoke(method_GetComponent, go, argsMF, nullptr);
                        if (meshFilter) {
                            mesh = (Il2CppObject*)il2cpp_runtime_invoke(prop_meshFilterSharedMesh, meshFilter, nullptr, nullptr);
                            if (mesh) {
                                meshSource = "meshfilter_sharedMesh";
                                usedMeshFilterFallback = true;
                            }
                        }
                    }

                    if (mesh) {
                        json meshJson = ExtractMeshData(mesh);

                        // If SMR sharedMesh exists but has no readable vertex array, try MeshFilter fallback.
                        int vtxArray = meshJson.value("vertexArrayCount", meshJson.value("vertexCount", 0));
                        if (vtxArray == 0 && !usedMeshFilterFallback &&
                            klass_MeshFilter && method_GetComponent && prop_meshFilterSharedMesh) {
                            static auto type_MeshFilter = il2cpp_class_get_type(klass_MeshFilter);
                            static auto reftype_MeshFilter = il2cpp_type_get_object(type_MeshFilter);
                            void* argsMF[] = { reftype_MeshFilter };
                            auto meshFilter = (Il2CppObject*)il2cpp_runtime_invoke(method_GetComponent, go, argsMF, nullptr);
                            if (meshFilter) {
                                auto mfMesh = (Il2CppObject*)il2cpp_runtime_invoke(prop_meshFilterSharedMesh, meshFilter, nullptr, nullptr);
                                if (mfMesh) {
                                    json mfMeshJson = ExtractMeshData(mfMesh);
                                    int mfVtxArray = mfMeshJson.value("vertexArrayCount", mfMeshJson.value("vertexCount", 0));
                                    if (mfVtxArray > 0) {
                                        meshJson = mfMeshJson;
                                        meshSource = "meshfilter_sharedMesh_after_smr_empty";
                                        usedMeshFilterFallback = true;
                                        vtxArray = mfVtxArray;
                                    }
                                }
                            }
                        }

                        meshJson["meshSource"] = meshSource;
                        meshJson["meshSourceFallbackUsed"] = usedMeshFilterFallback;
                        meshJson["smrInstanceID"] = GetObjectInstanceID(smr);
                        meshJson["ownerGameObjectInstanceID"] = GetObjectInstanceID(go);
                        meshJson["ownerGameObjectPath"] = node["fullPath"];
                        meshJson["exportTag"] = node["exportTag"];
                        meshJson["rendererEnabled"] = rendererEnabled;
                        meshJson["activeInHierarchy"] = node.value("activeInHierarchy", true);
                        int vtx = meshJson.value("vertexArrayCount", meshJson.value("vertexCount", 0));
                        if (vtx == 0) {
                            includeForAssemble = false;
                            const bool canAccess = meshJson.value("canAccess", true);
                            const bool isReadable = meshJson.value("isReadable", true);
                            const int reported = meshJson.value("vertexCountReported", meshJson.value("vertexCount", 0));
                            if ((!canAccess || !isReadable) && reported > 0) {
                                if (excludeReason.empty()) excludeReason = "non_readable_mesh";
                            } else {
                                if (excludeReason.empty()) excludeReason = "zero_vertex";
                            }
                        }
                        if (!rendererEnabled || !node.value("activeInHierarchy", true)) {
                            includeForAssemble = false;
                            if (excludeReason.empty()) excludeReason = "inactive_or_disabled";
                        }
                        std::ofstream o(currentPath + "/mesh.json");
                        o << meshJson.dump(4) << std::endl;
                        o.close();
                        hasMeshFile = true;
                    } else {
                        includeForAssemble = false;
                        if (excludeReason.empty()) {
                            if (!prop_sharedMesh && !(klass_MeshFilter && prop_meshFilterSharedMesh)) {
                                excludeReason = "sharedMesh_and_meshfilter_method_missing";
                            } else {
                                excludeReason = "sharedMesh_and_meshfilter_null";
                            }
                        }
                    }
                } else {
                    includeForAssemble = false;
                    if (excludeReason.empty()) excludeReason = "sharedMesh_and_meshfilter_method_missing";
                }

                // Extract Bones
                json boneJson = ExtractBoneData(smr, go);
                std::ofstream o2(currentPath + "/bones.json");
                o2 << boneJson.dump(4) << std::endl;
                o2.close();

                // Extract Materials
                json matJson = ExtractMaterialData(smr, go);
                std::ofstream o3(currentPath + "/materials.json");
                o3 << matJson.dump(4) << std::endl;
                o3.close();

                if (!hasMeshFile) {
                    includeForAssemble = false;
                    if (excludeReason.empty()) excludeReason = "mesh_dump_failed";
                }
                if (excludeReason.empty()) excludeReason = "ok";
                AddCharacterMeshRecord(
                    currentPath,
                    node["fullPath"].get<std::string>(),
                    goInstanceID,
                    smrInstanceID,
                    node["exportTag"].get<std::string>(),
                    includeForAssemble,
                    excludeReason
                );
            }
        }

        // Transform Data
        static auto prop_transform = il2cpp_class_get_method_from_name(klass_GameObject, "get_transform", 0);
        auto transform = (Il2CppObject*)il2cpp_runtime_invoke(prop_transform, go, nullptr, nullptr);

        if (transform) {
            static auto klass_Transform = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
            
            // Pos
            static auto method_get_localPosition = il2cpp_class_get_method_from_name(klass_Transform, "get_localPosition", 0);
            if (method_get_localPosition) {
                auto posObj = il2cpp_runtime_invoke(method_get_localPosition, transform, nullptr, nullptr);
                if (posObj) {
                    Vector3 pos = *(Vector3*)il2cpp_object_unbox((Il2CppObject*)posObj);
                    node["localPosition"] = { pos.x, pos.y, pos.z };
                }
            }
            // Rot
            static auto method_get_localRotation = il2cpp_class_get_method_from_name(klass_Transform, "get_localRotation", 0);
            if (method_get_localRotation) {
                auto rotObj = il2cpp_runtime_invoke(method_get_localRotation, transform, nullptr, nullptr);
                if (rotObj) {
                    Quaternion rot = *(Quaternion*)il2cpp_object_unbox((Il2CppObject*)rotObj);
                    node["localRotation"] = { rot.x, rot.y, rot.z, rot.w };
                }
            }
            // Scale
            static auto method_get_localScale = il2cpp_class_get_method_from_name(klass_Transform, "get_localScale", 0);
            if (method_get_localScale) {
                auto scaleObj = il2cpp_runtime_invoke(method_get_localScale, transform, nullptr, nullptr);
                if (scaleObj) {
                    Vector3 scale = *(Vector3*)il2cpp_object_unbox((Il2CppObject*)scaleObj);
                    node["localScale"] = { scale.x, scale.y, scale.z };
                }
            }

            // Children
            node["children"] = json::array();
            static auto prop_childCount = il2cpp_class_get_method_from_name(klass_Transform, "get_childCount", 0);
            static auto method_GetChild = il2cpp_class_get_method_from_name(klass_Transform, "GetChild", 1);

            if (prop_childCount && method_GetChild) {
                auto childCountObj = il2cpp_runtime_invoke(prop_childCount, transform, nullptr, nullptr);
                if (childCountObj) {
                    int childCount = *(int*)il2cpp_object_unbox((Il2CppObject*)childCountObj);

                    for (int i = 0; i < childCount; i++) {
                        void* argsChild[] = { &i };
                        auto childTransform = (Il2CppObject*)il2cpp_runtime_invoke(method_GetChild, transform, argsChild, nullptr);
                        
                        if (childTransform) {
                            static auto prop_gameObject = il2cpp_class_get_method_from_name(il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Component"), "get_gameObject", 0);
                            auto childGO = (Il2CppObject*)il2cpp_runtime_invoke(prop_gameObject, childTransform, nullptr, nullptr);
                            
                            // Pass currentPath to child to preserve hierarchy
                            node["children"].push_back(SerializeGameObject(childGO, currentPath, node["instanceID"].get<int>()));
                        }
                    }
                }
            }
        }
        return node;
    }

    void DumpScene() {
        try {
            g_characterIndexByFsPath.clear();
            std::string timestamp = GetTimestampString();
            std::string dumpRoot = MakeUniqueDumpRoot("ModelDumps", timestamp);
            CreateDir("ModelDumps");
            CreateDir(dumpRoot);
            
            std::cout << "=== Dumping Scene to " << dumpRoot << " ===" << std::endl;
            
            json rootArray = json::array();

            auto klass_Object = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
            auto klass_Transform = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
            
            if (!klass_Object || !klass_Transform) {
                std::cout << "Error: Essential classes not found!" << std::endl;
                return;
            }

            auto method_FindObjectsOfType = il2cpp_class_get_method_from_name(klass_Object, "FindObjectsOfType", 1);
            auto type_Transform = il2cpp_class_get_type(klass_Transform);
            auto reflection_type = il2cpp_type_get_object(type_Transform);
            void* args[] = { reflection_type };
            
            Il2CppObject* exception = nullptr;
            std::cout << "Finding all Transforms..." << std::endl;
            auto transformArray = (Il2CppArray*)il2cpp_runtime_invoke(method_FindObjectsOfType, nullptr, args, &exception);
            
            if (exception) {
                std::cout << "Exception during FindObjectsOfType" << std::endl;
                return;
            }

            if (transformArray) {
                int count = il2cpp_array_length(transformArray);
                std::cout << "Found " << count << " transforms. Filtering roots..." << std::endl;

                static auto prop_parent = il2cpp_class_get_method_from_name(klass_Transform, "get_parent", 0);
                static auto prop_gameObject = il2cpp_class_get_method_from_name(il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Component"), "get_gameObject", 0);

                for (int i = 0; i < count; i++) {
                    auto transform = (Il2CppObject*)il2cpp_symbols::array_get_value(transformArray, i);
                    if (!transform) continue;
                    
                    auto parent = (Il2CppObject*)il2cpp_runtime_invoke(prop_parent, transform, nullptr, nullptr);
                    
                    if (parent == nullptr) {
                        auto go = (Il2CppObject*)il2cpp_runtime_invoke(prop_gameObject, transform, nullptr, nullptr);
                        // Pass the dump root path
                        std::cout << "Processing root: " << i << std::endl;
                        rootArray.push_back(SerializeGameObject(go, dumpRoot, 0));
                    }
                }
            }
            
            std::cout << "Writing hierarchy.json..." << std::endl;
            json finalJson;
            finalJson["hierarchy"] = rootArray;
            std::ofstream o(dumpRoot + "/hierarchy.json");
            o << finalJson.dump(4) << std::endl;
            o.close();

            // Write one index per character for easier per-character reconstruction.
            for (auto& kv : g_characterIndexByFsPath) {
                const std::string& characterFsPath = kv.first;
                json& idx = kv.second;
                idx["dumpRoot"] = dumpRoot;
                idx["timestamp"] = timestamp;
                idx["includedCount"] = idx["includedMeshes"].size();
                idx["excludedCount"] = idx["excludedMeshes"].size();
                std::ofstream oi(characterFsPath + "/character_index.json");
                oi << idx.dump(4) << std::endl;
                oi.close();
            }

            std::cout << "=== Dump Complete ===" << std::endl;

        } catch (const std::exception& e) {
            std::cout << "FATAL ERROR in DumpScene: " << e.what() << std::endl;
            std::cout << "Press any key to continue..." << std::endl;
            system("pause");
        } catch (...) {
            std::cout << "UNKNOWN FATAL ERROR in DumpScene" << std::endl;
            std::cout << "Press any key to continue..." << std::endl;
            system("pause");
        }
    }


    void Update() {
        static bool f9_key_state = false;
        if (GetAsyncKeyState(VK_F9) & 0x8000) {
            if (!f9_key_state) {
                f9_key_state = true;
                std::cout << "\n[Probe] F9 Pressed. Dumping Scene to JSON..." << std::endl;
                DumpScene();
            }
        } else {
            f9_key_state = false;
        }
    }
}
