#pragma once

//! DXの標準機能
#include <windows.h>
#include <d3d12.h>
#include <string>
#include <string_view>
#include <format>
#include <utility>
#include <mutex>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <tchar.h>
#include <stdexcept>
#include <dxgi1_6.h>
#include <wrl.h>
#include <memory>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <dxgi1_2.h>
#include <unordered_map>
#include <filesystem>
#include <cstdio>
#include <wrl\client.h>
#include <fstream>
#include <array>
#include <d3dx12.h>
#include <typeindex>
#include <algorithm>

#undef ERROR
#undef OPAQUE

//! 自作の機能
#include "System\stringformat.h"
#include "Graphics\DX12.h"
#include "System\Logger.h"
#include "System\LoggerMacros.h"
#include "imgui_render.h"
#include "Math\SimpleMath.h"
#include "Graphics\PiplineState.h"
#include "Graphics\TextureManager.h"
#include "Graphics\ShaderManager.h"
#include "Graphics\RootsignatureManager.h"
#include "Graphics\DescriptorHeapManager.h"
#include "System\TimeManager.h"
#include "Scene\SceneManager.h"
#include "Camera\CameraManager.h"
#include "GameObject\GameObjectRegistry.h"
#include "Editor\EditorManager.h"
#include "Input\InputManager.h"
#include "Audio\AudioManager.h"

//! 省略系
using namespace DirectX;
using namespace SimpleMath;