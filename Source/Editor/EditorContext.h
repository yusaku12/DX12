#pragma once

class GameObject;

//=====================================================
// Editor が共有する最低限の状態
//=====================================================
struct EditorContext
{
    GameObject* selectedObject = nullptr;
};

extern EditorContext g_editor;