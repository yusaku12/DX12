#pragma once

#include <d3dx12.h>

////! RootSignature用 RootParameter（SRV）を生成
//D3D12_ROOT_PARAMETER createRootParameterSRV(
//    UINT shaderRegister,
//    UINT registerSpace = 0,
//    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL
//) const
//{
//    D3D12_ROOT_PARAMETER param{};
//    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
//    param.ShaderVisibility = visibility;
//    param.Descriptor.ShaderRegister = shaderRegister;
//    param.Descriptor.RegisterSpace = registerSpace;
//    return param;
//}