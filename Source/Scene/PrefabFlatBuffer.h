#pragma once

#include <filesystem>

class GameObject;

namespace PrefabFlatBuffer
{
    //! Save a GameObject hierarchy as a prefab asset.
    bool save(const std::filesystem::path& filePath, GameObject* root);

    //! Instantiate a prefab asset and optionally attach to parent.
    GameObject* instantiate(const std::filesystem::path& filePath, GameObject* parent = nullptr);

    //! Apply instance changes back to its prefab asset.
    bool apply(GameObject* instanceObject);

    //! Revert an instance to prefab asset state.
    GameObject* revert(GameObject* instanceObject);

    //! Find prefab instance root for the given object.
    GameObject* findPrefabRoot(GameObject* object);
}
