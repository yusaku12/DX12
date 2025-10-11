#pragma once

//! DX�̕W���@�\
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
#include <d3d12.h>
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
#include <Windows.h>
#undef ERROR

//! ����̋@�\
#include "System\stringformat.h"
#include "dx12.h"
#include "System\logger.h"
#include "imgui_render.h"
#include "Math\simplemath.h"
#include "Graphics\loadshader.h"
#include "Graphics\loadtexture.h"

//! �ȗ��n
using namespace DirectX::SimpleMath;