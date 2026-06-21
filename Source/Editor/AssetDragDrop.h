#pragma once

#include <filesystem>

class GameObject;

namespace EditorAssetDragDrop
{
    //! アセット D&D 用のペイロード種別
    const char* getAssetPayloadType();

    //! 直前の ImGui Item をドラッグソース化する
    bool beginAssetDragSource(const std::filesystem::path& assetPath, const char* label = nullptr);

    //! 現在の DragDropTarget でアセットペイロードを受理して反映する
    bool acceptAssetDropInCurrentTarget(GameObject* parent = nullptr);

    //! アセットパスからシーンへ反映（Prefab/Scene/FBX を処理）
    bool applyAssetToScene(const std::filesystem::path& assetPath, GameObject* parent = nullptr);
}
