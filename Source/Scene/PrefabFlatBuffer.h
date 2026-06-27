#pragma once

#include <filesystem>
#include <string>
#include <vector>

class GameObject;

namespace PrefabFlatBuffer
{
    struct OverrideEntry
    {
        std::string objectName;
        std::string detail;
    };

    struct OverrideInfo
    {
        bool valid = false;
        bool isVariant = false;
        std::filesystem::path compareTargetPath;
        std::filesystem::path basePrefabPath;
        std::vector<OverrideEntry> entries;
    };

    //! Save a GameObject hierarchy as a prefab asset.
    bool save(const std::filesystem::path& filePath, GameObject* root);

    //! Instantiate a prefab asset and optionally attach to parent.
    GameObject* instantiate(const std::filesystem::path& filePath, GameObject* parent = nullptr);

    //! Apply instance changes back to its prefab asset.
    bool apply(GameObject* instanceObject);

    //! Revert an instance to prefab asset state.
    GameObject* revert(GameObject* instanceObject);

    //! Create prefab variant file from source instance and base prefab path.
    bool createVariant(const std::filesystem::path& variantPath, const std::filesystem::path& basePrefabPath, GameObject* sourceRoot);

    //! Build override info by comparing instance against source prefab (or variant base prefab).
    bool buildOverrideInfo(GameObject* instanceObject, OverrideInfo& outInfo);

    //! Find prefab instance root for the given object.
    GameObject* findPrefabRoot(GameObject* object);
}
