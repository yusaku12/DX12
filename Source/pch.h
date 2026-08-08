#pragma once

// DXの標準機能
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
#include <magic_enum.hpp>
#include <map>
#include <thread>
#include <future>
#include <deque>
#include <unordered_set>

#pragma warning(push)
#pragma warning(disable:4366)
#include <tracy/Tracy.hpp>
#pragma warning(pop)

#undef ERROR
#undef OPAQUE

// 自作の機能
#include "Math\SimpleMath.h"

// ozz の SIMD 演算子は DirectX 名前空間の公開前に解析する。
#include <ozz/base/maths/soa_transform.h>

//! 省略系
using namespace DirectX;
using namespace SimpleMath;

#include "System\Dialog.h"
#include "System\stringformat.h"
#include "System\CrashReporter.h"
#include "Graphics\DX12.h"
#include "System\Logger.h"
#include "System\LoggerMacros.h"
#include "System\MemorySystem.h"
#include "imgui_render.h"
#include "Graphics\PiplineState.h"
#include "Graphics\TextureManager.h"
#include "Graphics\ShaderManager.h"
#include "Graphics\RootsignatureManager.h"
#include "Graphics\DescriptorHeapManager.h"
#include "Graphics\PSOCreator.h"
#include "System\DebugPrimitive.h"
#include "Graphics\CommandListPool.h"
#include "Physics\PhysicsWorld.h"
#include "Editor\ScreenCapture.h"
#include "Render\GBufferRenderTargets.h"
