#pragma once

#define NOMINMAX

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

#undef ERROR
#undef OPAQUE

//! 自作の機能
#include "System\stringformat.h"
#include "dx12.h"
#include "System\logger.h"
#include "imgui_render.h"
#include "Math\simplemath.h"
#include "Graphics\piplinestate.h"
#include "Graphics\texturemanager.h"
#include "Graphics\shadermanager.h"

//! 省略系
using namespace DirectX::SimpleMath;