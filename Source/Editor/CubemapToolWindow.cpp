#include "pch.h"
#include "CubemapToolWindow.h"
#include "System/Dialog.h"
#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

namespace
{
    //! 面の並び順
    static constexpr std::array<const char*, 6> s_faceLabels =
    {
        "+X", "-X", "+Y", "-Y", "+Z", "-Z"
    };

    //! 入出力パスの状態
    struct CubemapToolState
    {
        std::array<std::wstring, 6> facePaths{};
        std::wstring outputPath{};
        bool m_useEquirect = false;
        int m_faceSize = 1024;
        bool m_exportIbl = true;
        int m_irradianceSize = 32;
    };

    static CubemapToolState s_state{};
    static std::array<std::array<char, 512>, 6> s_facePathUtf8{};
    static std::array<char, 512> s_outputPathUtf8{};

    //! UTF16 -> UTF8
    std::string utf16ToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return {};

        int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return {};

        std::vector<char> buffer(static_cast<size_t>(size));
        int written = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), size, nullptr, nullptr);
        if (written <= 0)
        {
            return {};
        }

        return std::string(buffer.data());
    }

    //! UTF8 -> UTF16
    std::wstring utf8ToUtf16(const std::string& text)
    {
        if (text.empty())
            return {};

        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (size <= 0)
            return {};

        std::vector<wchar_t> buffer(static_cast<size_t>(size));
        int written = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer.data(), size);
        if (written <= 0)
        {
            return {};
        }

        return std::wstring(buffer.data());
    }

    //! UTF8 バッファ更新
    void updateUtf8Buffers()
    {
        for (size_t i = 0; i < s_faceLabels.size(); ++i)
        {
            const auto text = utf16ToUtf8(s_state.facePaths[i]);
            std::snprintf(s_facePathUtf8[i].data(), s_facePathUtf8[i].size(), "%s", text.c_str());
        }

        const auto outText = utf16ToUtf8(s_state.outputPath);
        std::snprintf(s_outputPathUtf8.data(), s_outputPathUtf8.size(), "%s", outText.c_str());
    }

    //! ローダー関数型
    using LoaderFunc = std::function<HRESULT(const std::wstring&, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

    //! ローダーテーブル生成
    std::unordered_map<std::wstring, LoaderFunc> createLoaderTable()
    {
        std::unordered_map<std::wstring, LoaderFunc> table;

        table[L"sph"]
            = table[L"spa"]
            = table[L"bmp"]
            = table[L"png"]
            = table[L"jpg"]
            = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
            {
                return DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
            };

        table[L"tga"]
            = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
            {
                return DirectX::LoadFromTGAFile(path.c_str(), meta, img);
            };

        table[L"dds"]
            = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
            {
                return DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, meta, img);
            };

        return table;
    }

    //! 拡張子から画像読み込み
    bool loadImageAny(const std::wstring& filePath, DirectX::ScratchImage& image, DirectX::TexMetadata& meta)
    {
        static const auto loaderTable = createLoaderTable();

        std::filesystem::path path(filePath);
        if (!path.has_extension())
        {
            LOG_ERROR("拡張子がありません: %s", toRelativePath(filePath).c_str());
            return false;
        }

        std::wstring ext = path.extension().wstring();
        if (!ext.empty() && ext[0] == L'.')
            ext.erase(ext.begin());

        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        auto it = loaderTable.find(ext);
        if (it == loaderTable.end())
        {
            LOG_ERROR("未対応の拡張子です: %s", toRelativePath(filePath).c_str());
            return false;
        }

        HRESULT hr = it->second(filePath, &meta, image);
        if (FAILED(hr))
        {
            LOG_ERROR("画像読み込み失敗: %s", toRelativePath(filePath).c_str());
            return false;
        }

        return true;
    }

    //! 画像を指定フォーマットに正規化
    bool normalizeImage(DirectX::ScratchImage& image, DirectX::TexMetadata& meta, DXGI_FORMAT targetFormat)
    {
        const DirectX::Image* src = image.GetImage(0, 0, 0);
        if (!src)
        {
            LOG_ERROR("画像データ取得に失敗しました");
            return false;
        }

        if (meta.format == targetFormat && !DirectX::IsCompressed(meta.format))
            return true;

        DirectX::ScratchImage converted{};
        HRESULT hr = S_OK;

        if (DirectX::IsCompressed(meta.format))
        {
            hr = DirectX::Decompress(*src, targetFormat, converted);
        }
        else
        {
            hr = DirectX::Convert(*src, targetFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
        }

        if (FAILED(hr))
        {
            LOG_ERROR("画像変換に失敗しました");
            return false;
        }

        image = std::move(converted);
        meta = image.GetMetadata();
        return true;
    }

    //! 線形補間
    float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    //! Equirectangular サンプリング（RGBA8）
    void sampleEquirectRGBA8(const DirectX::Image* src, float u, float v, uint8_t* out)
    {
        const size_t width = src->width;
        const size_t height = src->height;

        u = u - std::floor(u);
        v = std::clamp(v, 0.0f, 1.0f);

        const float fx = u * static_cast<float>(width - 1);
        const float fy = v * static_cast<float>(height - 1);

        const size_t x0 = static_cast<size_t>(fx);
        const size_t y0 = static_cast<size_t>(fy);
        const size_t x1 = (x0 + 1) % width;
        const size_t y1 = std::min(y0 + 1, height - 1);

        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);

        const uint8_t* p00 = src->pixels + y0 * src->rowPitch + x0 * 4;
        const uint8_t* p10 = src->pixels + y0 * src->rowPitch + x1 * 4;
        const uint8_t* p01 = src->pixels + y1 * src->rowPitch + x0 * 4;
        const uint8_t* p11 = src->pixels + y1 * src->rowPitch + x1 * 4;

        for (int c = 0; c < 4; ++c)
        {
            const float c0 = lerp(static_cast<float>(p00[c]), static_cast<float>(p10[c]), tx);
            const float c1 = lerp(static_cast<float>(p01[c]), static_cast<float>(p11[c]), tx);
            const float c2 = lerp(c0, c1, ty);
            out[c] = static_cast<uint8_t>(std::clamp(c2, 0.0f, 255.0f));
        }
    }

    //! DDS 保存
    bool saveDDS(const DirectX::ScratchImage& image, const std::wstring& outputPath)
    {
        HRESULT hr = DirectX::SaveToDDSFile(
            image.GetImages(),
            image.GetImageCount(),
            image.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputPath.c_str());

        if (FAILED(hr))
        {
            LOG_ERROR("DDS書き出しに失敗しました: %s", toRelativePath(outputPath).c_str());
            return false;
        }

        LOG_INFO("DDSを書き出しました: %s", toRelativePath(outputPath).c_str());
        return true;
    }

    //! IBL 出力パス生成
    std::wstring buildIblPath(const std::wstring& basePath, const std::wstring& suffix)
    {
        std::filesystem::path path(basePath);
        std::wstring name = path.stem().wstring() + suffix + path.extension().wstring();
        return (path.parent_path() / name).wstring();
    }

    //! IBL テクスチャ出力
    bool exportIblTextures(const DirectX::ScratchImage& cubeImage, const std::wstring& baseOutputPath, int irradianceSize)
    {
        const int size = std::clamp(irradianceSize, 4, 512);

        DirectX::ScratchImage irradianceImage{};
        HRESULT hr = DirectX::Resize(
            cubeImage.GetImages(),
            cubeImage.GetImageCount(),
            cubeImage.GetMetadata(),
            static_cast<size_t>(size),
            static_cast<size_t>(size),
            DirectX::TEX_FILTER_DEFAULT,
            irradianceImage);

        if (FAILED(hr))
        {
            LOG_ERROR("Irradiance の生成に失敗しました");
            return false;
        }

        const std::wstring irradiancePath = buildIblPath(baseOutputPath, L"_irradiance");
        if (!saveDDS(irradianceImage, irradiancePath))
            return false;

        DirectX::ScratchImage prefilterImage{};
        hr = DirectX::GenerateMipMaps(
            cubeImage.GetImages(),
            cubeImage.GetImageCount(),
            cubeImage.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            0,
            prefilterImage);

        if (FAILED(hr))
        {
            LOG_ERROR("Prefilter の生成に失敗しました");
            return false;
        }

        const std::wstring prefilterPath = buildIblPath(baseOutputPath, L"_prefilter");
        if (!saveDDS(prefilterImage, prefilterPath))
            return false;

        LOG_INFO("IBL テクスチャを書き出しました (irradiance / prefilter)");
        return true;
    }

    //! Equirectangular -> Cubemap 生成
    bool createCubemapImageFromEquirect(const std::wstring& inputPath, int faceSize, DirectX::ScratchImage& cubeImage)
    {
        DirectX::ScratchImage srcImage{};
        DirectX::TexMetadata srcMeta{};

        if (!loadImageAny(inputPath, srcImage, srcMeta))
            return false;

        if (!normalizeImage(srcImage, srcMeta, DXGI_FORMAT_R8G8B8A8_UNORM))
            return false;

        const DirectX::Image* src = srcImage.GetImage(0, 0, 0);
        if (!src)
        {
            LOG_ERROR("画像データ取得に失敗しました");
            return false;
        }

        if (src->width != src->height * 2)
        {
            LOG_WARN("入力が2:1ではありません（続行します）");
        }

        const int size = std::clamp(faceSize, 16, 4096);

        std::array<std::vector<uint8_t>, 6> facePixels{};
        for (auto& pixels : facePixels)
        {
            pixels.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
        }

        const float invSize = 1.0f / static_cast<float>(size);

        for (size_t face = 0; face < 6; ++face)
        {
            uint8_t* dst = facePixels[face].data();

            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) * invSize * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + 0.5f) * invSize * 2.0f - 1.0f;

                    Vector3 dir{};
                    switch (face)
                    {
                    case 0: dir = Vector3(1.0f, -v, -u); break;  // +X
                    case 1: dir = Vector3(-1.0f, -v, u); break;  // -X
                    case 2: dir = Vector3(u, 1.0f, v); break;    // +Y
                    case 3: dir = Vector3(u, -1.0f, -v); break;  // -Y
                    case 4: dir = Vector3(u, -v, 1.0f); break;   // +Z
                    case 5: dir = Vector3(-u, -v, -1.0f); break; // -Z
                    }

                    dir.Normalize();

                    const float theta = std::atan2(dir.z, dir.x);
                    const float phi = std::asin(dir.y);

                    const float srcU = 0.5f + theta / (DirectX::XM_2PI);
                    const float srcV = 0.5f - phi / DirectX::XM_PI;

                    uint8_t* out = dst + (static_cast<size_t>(y) * size + x) * 4;
                    sampleEquirectRGBA8(src, srcU, srcV, out);
                }
            }
        }

        std::array<DirectX::Image, 6> images{};
        for (size_t i = 0; i < images.size(); ++i)
        {
            images[i].width = size;
            images[i].height = size;
            images[i].format = DXGI_FORMAT_R8G8B8A8_UNORM;
            images[i].rowPitch = static_cast<size_t>(size) * 4;
            images[i].slicePitch = images[i].rowPitch * static_cast<size_t>(size);
            images[i].pixels = facePixels[i].data();
        }

        HRESULT hr = cubeImage.InitializeCubeFromImages(images.data(), images.size());
        if (FAILED(hr))
        {
            LOG_ERROR("キューブマップ初期化に失敗しました");
            return false;
        }

        return true;
    }

    //! キューブマップ生成
    bool createCubemapDDS(const std::wstring& outputPath)
    {
        if (outputPath.empty())
        {
            LOG_ERROR("出力先が未指定です");
            return false;
        }

        DirectX::ScratchImage cubeImage{};

        if (s_state.m_useEquirect)
        {
            if (s_state.facePaths[0].empty())
            {
                LOG_ERROR("+X に入力画像を指定してください");
                return false;
            }

            if (!createCubemapImageFromEquirect(s_state.facePaths[0], s_state.m_faceSize, cubeImage))
                return false;
        }
        else
        {
            std::array<std::wstring, 6> facePaths = s_state.facePaths;

            size_t nonEmptyCount = 0;
            for (const auto& path : facePaths)
            {
                if (!path.empty())
                    ++nonEmptyCount;
            }

            if (nonEmptyCount == 1 && !facePaths[0].empty())
            {
                for (size_t i = 1; i < facePaths.size(); ++i)
                {
                    facePaths[i] = facePaths[0];
                }
                LOG_INFO("1枚指定のため、全6面に同一画像を適用しました");
            }
            else if (nonEmptyCount != 6)
            {
                LOG_ERROR("6面すべてを指定するか、+X のみ1枚指定してください");
                return false;
            }

            std::array<DirectX::ScratchImage, 6> faces{};
            std::array<DirectX::TexMetadata, 6> metas{};

            for (size_t i = 0; i < s_faceLabels.size(); ++i)
            {
                if (facePaths[i].empty())
                {
                    LOG_ERROR("未指定の面があります: %s", s_faceLabels[i]);
                    return false;
                }

                if (!loadImageAny(facePaths[i], faces[i], metas[i]))
                    return false;

                if (metas[i].arraySize != 1 || metas[i].depth != 1)
                {
                    LOG_ERROR("配列/3Dテクスチャは非対応です: %s", toRelativePath(facePaths[i]).c_str());
                    return false;
                }
            }

            const size_t width = metas[0].width;
            const size_t height = metas[0].height;

            for (size_t i = 1; i < metas.size(); ++i)
            {
                if (metas[i].width != width || metas[i].height != height)
                {
                    LOG_ERROR("全画像のサイズを一致させてください");
                    return false;
                }
            }

            const DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

            for (size_t i = 0; i < faces.size(); ++i)
            {
                if (!normalizeImage(faces[i], metas[i], targetFormat))
                    return false;
            }

            std::array<DirectX::Image, 6> faceImages{};
            for (size_t i = 0; i < faces.size(); ++i)
            {
                const DirectX::Image* src = faces[i].GetImage(0, 0, 0);
                if (!src)
                {
                    LOG_ERROR("画像データ取得に失敗しました: %s", toRelativePath(facePaths[i]).c_str());
                    return false;
                }
                faceImages[i] = *src;
            }

            HRESULT hr = cubeImage.InitializeCubeFromImages(faceImages.data(), faceImages.size());
            if (FAILED(hr))
            {
                LOG_ERROR("キューブマップ初期化に失敗しました");
                return false;
            }
        }

        if (!saveDDS(cubeImage, outputPath))
            return false;

        if (s_state.m_exportIbl)
        {
            if (!exportIblTextures(cubeImage, outputPath, s_state.m_irradianceSize))
                return false;
        }

        return true;
    }
}

void drawCubemapToolWindow()
{
    ImGui::Begin("Cubemap Tool");

    ImGui::Checkbox("Equirectangular (2:1) -> Cubemap", &s_state.m_useEquirect);
    ImGui::SameLine();
    ImGui::DragInt("Face Size", &s_state.m_faceSize, 1.0f, 64, 4096);

    ImGui::Separator();

    if (s_state.m_useEquirect)
    {
        ImGui::Text("Input (Panorama)");
        ImGui::SameLine();
        ImGui::TextUnformatted(s_facePathUtf8[0].data());
        ImGui::SameLine();
        if (ImGui::Button("Select"))
        {
            std::vector<std::wstring> paths;
            if (Dialog::openFile(paths, L"Select Panorama (2:1)", L"", false) == DialogResult::OK && !paths.empty())
            {
                s_state.facePaths[0] = paths[0];
                updateUtf8Buffers();
            }
        }
    }
    else
    {
        ImGui::Text("Inputs (Cubemap Faces)");
        for (size_t i = 0; i < s_faceLabels.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", s_faceLabels[i]);
            ImGui::SameLine();
            ImGui::TextUnformatted(s_facePathUtf8[i].data());
            ImGui::SameLine();
            if (ImGui::Button("Select"))
            {
                std::vector<std::wstring> paths;
                if (Dialog::openFile(paths, L"Select Face Texture", L"", false) == DialogResult::OK && !paths.empty())
                {
                    s_state.facePaths[i] = paths[0];
                    updateUtf8Buffers();
                }
            }
            ImGui::PopID();
        }

        if (ImGui::Button("+X を全面に適用"))
        {
            if (s_state.facePaths[0].empty())
            {
                LOG_WARN("+X が未指定です");
            }
            else
            {
                for (size_t i = 1; i < s_faceLabels.size(); ++i)
                {
                    s_state.facePaths[i] = s_state.facePaths[0];
                }
                updateUtf8Buffers();
                LOG_INFO("+X を全6面に適用しました");
            }
        }
    }

    ImGui::Separator();

    ImGui::Checkbox("Export IBL (Irradiance / Prefilter)", &s_state.m_exportIbl);
    if (s_state.m_exportIbl)
    {
        ImGui::SameLine();
        ImGui::DragInt("Irradiance Size", &s_state.m_irradianceSize, 1.0f, 4, 256);
    }

    ImGui::Separator();

    ImGui::Text("Output");
    ImGui::SameLine();
    ImGui::TextUnformatted(s_outputPathUtf8.data());
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        std::wstring outPath;
        if (Dialog::saveFile(outPath, L"Save DDS", L"", L"dds") == DialogResult::OK)
        {
            s_state.outputPath = outPath;
            updateUtf8Buffers();
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Generate DDS"))
    {
        createCubemapDDS(s_state.outputPath);
    }

    ImGui::End();
}