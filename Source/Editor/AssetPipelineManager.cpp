#include "pch.h"
#include "AssetPipelineManager.h"

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

namespace
{
    struct PakEntry
    {
        std::string path;
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t hash = 0;
        uint32_t flags = 0;
    };

    struct PakArchive
    {
        std::vector<PakEntry> entries;
        uint64_t pakHash = 0;
    };

    constexpr uint32_t kPakVersion = 1;
    constexpr uint32_t kPatchVersion = 1;
    constexpr char kPakMagic[8] = { 'D', 'X', '1', '2', 'P', 'A', 'K', '1' };
    constexpr char kPatchMagic[8] = { 'D', 'X', '1', '2', 'P', 'T', 'C', '1' };
    constexpr uint64_t kPakHeaderSize = 8 + 4 + 4 + 8 + 8;
    constexpr uint64_t kPatchHeaderSize = 8 + 4 + 4 + 4 + 8 + 8 + 8 + 8;

    std::string toLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return text;
    }

    std::filesystem::path normalizePathKey(const std::filesystem::path& path)
    {
        std::error_code ec;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        if (!ec)
        {
            return canonical;
        }

        if (path.is_absolute())
        {
            return path.lexically_normal();
        }

        return (std::filesystem::current_path() / path).lexically_normal();
    }

    std::filesystem::path toRelativeOrNormalized(const std::filesystem::path& path)
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(path, std::filesystem::current_path(), ec);
        if (!ec)
        {
            return relative.lexically_normal();
        }

        return path.lexically_normal();
    }

    std::string toUtf8Path(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }

    std::filesystem::path fromUtf8Path(std::string_view utf8)
    {
        std::u8string u8;
        u8.reserve(utf8.size());
        for (char ch : utf8)
        {
            u8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
        }
        return std::filesystem::path(u8);
    }

    bool isManagedAsset(const std::filesystem::path& assetPath)
    {
        const std::string ext = toLower(assetPath.extension().string());
        return ext == ".prefab"
            || ext == ".scn"
            || ext == ".fbx"
            || ext == ".dds"
            || ext == ".png"
            || ext == ".jpg"
            || ext == ".jpeg"
            || ext == ".bmp"
            || ext == ".tga"
            || ext == ".hdr";
    }

    bool isTextureAsset(const std::filesystem::path& assetPath)
    {
        const std::string ext = toLower(assetPath.extension().string());
        return ext == ".dds"
            || ext == ".png"
            || ext == ".jpg"
            || ext == ".jpeg"
            || ext == ".bmp"
            || ext == ".tga"
            || ext == ".hdr";
    }

    uint64_t fnv1a64(const uint8_t* data, size_t size)
    {
        uint64_t hash = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    std::vector<uint8_t> readBinary(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return {};
        }

        const std::streamsize size = file.tellg();
        if (size < 0)
        {
            return {};
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            return {};
        }

        return bytes;
    }

    uint64_t fileHash(const std::filesystem::path& filePath)
    {
        const std::vector<uint8_t> bytes = readBinary(filePath);
        if (bytes.empty())
        {
            return 0;
        }

        return fnv1a64(bytes.data(), bytes.size());
    }

    template <class T>
    bool readExact(std::ifstream& stream, T& outValue)
    {
        return static_cast<bool>(stream.read(reinterpret_cast<char*>(&outValue), sizeof(T)));
    }

    template <class T>
    void writeExact(std::ofstream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    bool readString(std::ifstream& stream, std::string& outValue)
    {
        uint32_t length = 0;
        if (!readExact(stream, length))
        {
            return false;
        }

        outValue.resize(length);
        if (length == 0)
        {
            return true;
        }

        return static_cast<bool>(stream.read(outValue.data(), static_cast<std::streamsize>(length)));
    }

    void writeString(std::ofstream& stream, std::string_view value)
    {
        const uint32_t length = static_cast<uint32_t>(value.size());
        writeExact(stream, length);
        if (length != 0)
        {
            stream.write(value.data(), static_cast<std::streamsize>(length));
        }
    }

    std::vector<std::filesystem::path> collectManagedAssets(const std::filesystem::path& root)
    {
        std::vector<std::filesystem::path> assets;

        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
        {
            return assets;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                continue;
            }

            if (!entry.is_regular_file(ec) || ec)
            {
                continue;
            }

            if (!isManagedAsset(entry.path()))
            {
                continue;
            }

            assets.push_back(normalizePathKey(entry.path()));
        }

        std::sort(assets.begin(), assets.end());
        assets.erase(std::unique(assets.begin(), assets.end()), assets.end());
        return assets;
    }

    bool copyFileWithDirectories(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
    {
        std::error_code ec;
        std::filesystem::create_directories(destinationPath.parent_path(), ec);
        if (ec)
        {
            return false;
        }

        ec.clear();
        std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    }

    std::filesystem::path cookedTexturePath(const std::filesystem::path& sourcePath, const EditorAssetPipeline::TextureCookSettings& settings)
    {
        if (settings.target == EditorAssetPipeline::TextureTarget::CopySource)
        {
            return sourcePath;
        }

        std::filesystem::path cooked = sourcePath;
        cooked.replace_extension(settings.outputExtension);
        return cooked;
    }

    bool loadTextureSource(const std::filesystem::path& sourcePath, DirectX::ScratchImage& outImage, DirectX::TexMetadata& outMeta, std::string& outError)
    {
        const std::string ext = toLower(sourcePath.extension().string());
        HRESULT hr = E_FAIL;

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".hdr")
        {
            hr = DirectX::LoadFromWICFile(sourcePath.c_str(), DirectX::WIC_FLAGS_NONE, &outMeta, outImage);
        }
        else if (ext == ".tga")
        {
            hr = DirectX::LoadFromTGAFile(sourcePath.c_str(), &outMeta, outImage);
        }
        else if (ext == ".dds")
        {
            hr = DirectX::LoadFromDDSFile(sourcePath.c_str(), DirectX::DDS_FLAGS_NONE, &outMeta, outImage);
        }
        else
        {
            outError = "Unsupported texture extension";
            return false;
        }

        if (FAILED(hr))
        {
            outError = "Texture decode failed";
            return false;
        }

        return true;
    }

    bool saveDDS(const DirectX::ScratchImage& image, const std::filesystem::path& outputPath)
    {
        const HRESULT hr = DirectX::SaveToDDSFile(
            image.GetImages(),
            image.GetImageCount(),
            image.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputPath.wstring().c_str());

        return SUCCEEDED(hr);
    }

    bool convertTextureToBC7(const std::filesystem::path& sourcePath, const std::filesystem::path& cookedPath, std::string& outError)
    {
        DirectX::ScratchImage sourceImage{};
        DirectX::TexMetadata sourceMeta{};
        if (!loadTextureSource(sourcePath, sourceImage, sourceMeta, outError))
        {
            return false;
        }

        if (sourceMeta.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || sourceMeta.arraySize != 1 || sourceMeta.depth != 1)
        {
            outError = "Only single 2D textures are supported for BC7 cooking";
            return false;
        }

        const DirectX::Image* firstImage = sourceImage.GetImage(0, 0, 0);
        if (!firstImage)
        {
            outError = "Texture image data is empty";
            return false;
        }

        DirectX::ScratchImage rgbaImage{};
        HRESULT hr = S_OK;
        if (DirectX::IsCompressed(sourceMeta.format))
        {
            hr = DirectX::Decompress(*firstImage, DXGI_FORMAT_R8G8B8A8_UNORM, rgbaImage);
        }
        else
        {
            hr = DirectX::Convert(*firstImage, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, rgbaImage);
        }

        if (FAILED(hr))
        {
            outError = "Texture normalization failed";
            return false;
        }

        DirectX::ScratchImage mipImage{};
        hr = DirectX::GenerateMipMaps(
            rgbaImage.GetImages(),
            rgbaImage.GetImageCount(),
            rgbaImage.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipImage);

        if (FAILED(hr))
        {
            outError = "Texture mip generation failed";
            return false;
        }

        DirectX::ScratchImage bc7Image{};
        hr = DirectX::Compress(
            mipImage.GetImages(),
            mipImage.GetImageCount(),
            mipImage.GetMetadata(),
            DXGI_FORMAT_BC7_UNORM,
            DirectX::TEX_COMPRESS_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            bc7Image);

        if (FAILED(hr))
        {
            outError = "BC7 compression failed";
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(cookedPath.parent_path(), ec);
        if (ec)
        {
            outError = "Failed to create cooked texture directory";
            return false;
        }

        if (!saveDDS(bc7Image, cookedPath))
        {
            outError = "Failed to save BC7 DDS";
            return false;
        }

        return true;
    }

    std::wstring expandTemplate(std::wstring templateText, const std::filesystem::path& inputPath, const std::filesystem::path& outputPath)
    {
        auto replaceAll = [](std::wstring& text, std::wstring_view token, std::wstring_view replacement)
        {
            size_t pos = 0;
            while ((pos = text.find(token, pos)) != std::wstring::npos)
            {
                text.replace(pos, token.size(), replacement);
                pos += replacement.size();
            }
        };

        replaceAll(templateText, L"{input}", inputPath.wstring());
        replaceAll(templateText, L"{output}", outputPath.wstring());
        replaceAll(templateText, L"{width}", L"0");
        replaceAll(templateText, L"{height}", L"0");
        return templateText;
    }

    bool runExternalEncoder(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPath, const EditorAssetPipeline::TextureCookSettings& settings, std::string& outError)
    {
        if (settings.externalEncoderPath.empty())
        {
            outError = "External encoder path is required for this texture target";
            return false;
        }

        std::wstring commandLine = L"\"" + settings.externalEncoderPath + L"\"";
        if (!settings.externalEncoderArguments.empty())
        {
            commandLine += L" ";
            commandLine += expandTemplate(settings.externalEncoderArguments, sourcePath, outputPath);
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION processInfo{};

        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        if (!CreateProcessW(nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                nullptr,
                &startup,
                &processInfo))
        {
            outError = "Failed to start external texture encoder";
            return false;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);

        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        if (exitCode != 0)
        {
            outError = "External texture encoder failed";
            return false;
        }

        return true;
    }

    bool cookTextureAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& cookedPath, const EditorAssetPipeline::TextureCookSettings& settings, std::string& outError)
    {
        switch (settings.target)
        {
        case EditorAssetPipeline::TextureTarget::CopySource:
            return copyFileWithDirectories(sourcePath, cookedPath);
        case EditorAssetPipeline::TextureTarget::BC7:
            return convertTextureToBC7(sourcePath, cookedPath, outError);
        case EditorAssetPipeline::TextureTarget::ExternalCommand:
            return runExternalEncoder(sourcePath, cookedPath, settings, outError);
        default:
            outError = "Unknown texture target";
            return false;
        }
    }

    #pragma pack(push, 1)
    struct PakHeader
    {
        char magic[8]{};
        uint32_t version = 0;
        uint32_t entryCount = 0;
        uint64_t directoryOffset = 0;
        uint64_t directorySize = 0;
    };

    struct PatchHeader
    {
        char magic[8]{};
        uint32_t version = 0;
        uint32_t changedCount = 0;
        uint32_t removedCount = 0;
        uint64_t basePakHash = 0;
        uint64_t newPakHash = 0;
        uint64_t directoryOffset = 0;
        uint64_t directorySize = 0;
    };
    #pragma pack(pop)

    bool readPakArchive(const std::filesystem::path& pakPath, PakArchive& outArchive)
    {
        std::ifstream file(pakPath, std::ios::binary);
        if (!file)
        {
            return false;
        }

        PakHeader header{};
        if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
        {
            return false;
        }

        if (std::memcmp(header.magic, kPakMagic, sizeof(kPakMagic)) != 0 || header.version != kPakVersion)
        {
            return false;
        }

        file.seekg(static_cast<std::streamoff>(header.directoryOffset), std::ios::beg);
        if (!file)
        {
            return false;
        }

        outArchive.entries.clear();
        outArchive.entries.reserve(header.entryCount);

        for (uint32_t i = 0; i < header.entryCount; ++i)
        {
            PakEntry entry{};
            if (!readString(file, entry.path)
                || !readExact(file, entry.offset)
                || !readExact(file, entry.size)
                || !readExact(file, entry.hash)
                || !readExact(file, entry.flags))
            {
                return false;
            }

            outArchive.entries.push_back(std::move(entry));
        }

        outArchive.pakHash = fileHash(pakPath);
        return true;
    }

    std::vector<uint8_t> readPakEntryData(const std::filesystem::path& pakPath, const PakEntry& entry)
    {
        std::ifstream file(pakPath, std::ios::binary);
        if (!file)
        {
            return {};
        }

        file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        if (!file)
        {
            return {};
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(entry.size));
        if (entry.size != 0 && !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(entry.size)))
        {
            return {};
        }

        return bytes;
    }

    std::filesystem::path pakManifestPath(const std::filesystem::path& pakPath)
    {
        std::filesystem::path manifest = pakPath;
        manifest += ".manifest.txt";
        return manifest;
    }

    std::filesystem::path patchManifestPath(const std::filesystem::path& patchPath)
    {
        std::filesystem::path manifest = patchPath;
        manifest += ".manifest.txt";
        return manifest;
    }

    std::string textureTargetToString(EditorAssetPipeline::TextureTarget target)
    {
        switch (target)
        {
        case EditorAssetPipeline::TextureTarget::CopySource: return "CopySource";
        case EditorAssetPipeline::TextureTarget::BC7: return "BC7";
        case EditorAssetPipeline::TextureTarget::ExternalCommand: return "ExternalCommand";
        default: return "Unknown";
        }
    }

    void writePakHeader(std::ofstream& out, uint32_t entryCount, uint64_t directoryOffset, uint64_t directorySize)
    {
        PakHeader header{};
        std::memcpy(header.magic, kPakMagic, sizeof(kPakMagic));
        header.version = kPakVersion;
        header.entryCount = entryCount;
        header.directoryOffset = directoryOffset;
        header.directorySize = directorySize;
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    }

    void writePatchHeader(std::ofstream& out, uint32_t changedCount, uint32_t removedCount, uint64_t basePakHash, uint64_t newPakHash, uint64_t directoryOffset, uint64_t directorySize)
    {
        PatchHeader header{};
        std::memcpy(header.magic, kPatchMagic, sizeof(kPatchMagic));
        header.version = kPatchVersion;
        header.changedCount = changedCount;
        header.removedCount = removedCount;
        header.basePakHash = basePakHash;
        header.newPakHash = newPakHash;
        header.directoryOffset = directoryOffset;
        header.directorySize = directorySize;
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    }

    uint64_t directoryBytesForPak(const std::vector<PakEntry>& entries)
    {
        uint64_t total = 0;
        for (const PakEntry& entry : entries)
        {
            total += sizeof(uint32_t) + entry.path.size() + sizeof(uint64_t) * 3 + sizeof(uint32_t);
        }
        return total;
    }

    uint64_t directoryBytesForPatch(const std::vector<PakEntry>& entries)
    {
        return directoryBytesForPak(entries);
    }

    bool writePakDirectory(std::ofstream& out, const std::vector<PakEntry>& entries)
    {
        for (const PakEntry& entry : entries)
        {
            writeString(out, entry.path);
            writeExact(out, entry.offset);
            writeExact(out, entry.size);
            writeExact(out, entry.hash);
            writeExact(out, entry.flags);
        }
        return out.good();
    }

    bool writeTextManifest(const std::filesystem::path& filePath, const std::string& text)
    {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return out.good();
    }
}

namespace EditorAssetPipeline
{
    bool AssetPipelineManager::cookAssets(const CookSettings& settings, CookReport* outReport)
    {
        const std::filesystem::path sourceRoot = normalizePathKey(settings.sourceRoot);
        const std::filesystem::path cookRoot = normalizePathKey(settings.cookRoot);

        std::error_code ec;
        if (!std::filesystem::exists(sourceRoot, ec))
        {
            return false;
        }

        std::filesystem::create_directories(cookRoot, ec);
        if (ec)
        {
            return false;
        }

        const std::vector<std::filesystem::path> assets = collectManagedAssets(sourceRoot);
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> cookedAssets;
        cookedAssets.reserve(assets.size());

        size_t copiedFileCount = 0;
        size_t textureCount = 0;
        size_t convertedTextureCount = 0;

        for (const std::filesystem::path& assetPath : assets)
        {
            EditorAssetMeta::AssetMetaManager::Instance().refreshAsset(assetPath, nullptr);

            std::error_code relEc;
            std::filesystem::path relativePath = std::filesystem::relative(assetPath, sourceRoot, relEc);
            if (relEc)
            {
                relativePath = assetPath.filename();
            }

            std::filesystem::path cookedPath = cookRoot / relativePath;
            const std::string ext = toLower(assetPath.extension().string());
            bool success = false;
            std::string errorMessage;

            if (ext == ".fbx")
            {
                success = copyFileWithDirectories(assetPath, cookedPath);
                if (success)
                {
                    ++copiedFileCount;
                }

                std::filesystem::path mdlPath = assetPath;
                mdlPath.replace_extension(".mdl");
                std::error_code mdlEc;
                if (std::filesystem::exists(mdlPath, mdlEc))
                {
                    std::filesystem::path cookedMdlPath = cookRoot / std::filesystem::relative(mdlPath, sourceRoot, mdlEc);
                    if (mdlEc)
                    {
                        cookedMdlPath = cookRoot / mdlPath.filename();
                    }

                    if (copyFileWithDirectories(mdlPath, cookedMdlPath))
                    {
                        ++copiedFileCount;
                    }
                }
            }
            else if (isTextureAsset(assetPath))
            {
                ++textureCount;
                cookedPath = cookedTexturePath(assetPath, settings.texture);
                cookedPath = cookRoot / std::filesystem::relative(cookedPath, sourceRoot, relEc);
                if (relEc)
                {
                    cookedPath = cookRoot / assetPath.filename();
                    if (settings.texture.target != TextureTarget::CopySource)
                    {
                        cookedPath.replace_extension(settings.texture.outputExtension);
                    }
                }

                success = cookTextureAsset(assetPath, cookedPath, settings.texture, errorMessage);
                if (success && settings.texture.target == TextureTarget::BC7)
                {
                    ++convertedTextureCount;
                }
                if (success)
                {
                    ++copiedFileCount;
                }
            }
            else
            {
                success = copyFileWithDirectories(assetPath, cookedPath);
                if (success)
                {
                    ++copiedFileCount;
                }
            }

            if (!success)
            {
                if (!errorMessage.empty())
                {
                    LOG_ERROR("[AssetPipelineManager] %s: %s", toUtf8Path(assetPath).c_str(), errorMessage.c_str());
                }
                return false;
            }

            cookedAssets.emplace_back(assetPath, cookedPath);
        }

        const std::filesystem::path manifestPath = cookRoot / "AssetCookManifest.txt";
        std::ostringstream manifest;
        manifest << "manifestVersion=1\n";
        manifest << "textureTarget=" << textureTargetToString(settings.texture.target) << "\n";
        manifest << "assetCount=" << cookedAssets.size() << "\n";
        for (const auto& [sourcePath, cookedPath] : cookedAssets)
        {
            manifest << "[asset]\n";
            manifest << "sourcePath=" << toRelativeOrNormalized(sourcePath).generic_string() << "\n";
            manifest << "cookedPath=" << toRelativeOrNormalized(cookedPath).generic_string() << "\n";
            manifest << "sourceHash=" << fileHash(sourcePath) << "\n";
            manifest << "cookedHash=" << fileHash(cookedPath) << "\n";
            manifest << "[/asset]\n";
        }

        if (!writeTextManifest(manifestPath, manifest.str()))
        {
            return false;
        }

        if (outReport)
        {
            outReport->assetCount = assets.size();
            outReport->textureCount = textureCount;
            outReport->convertedTextureCount = convertedTextureCount;
            outReport->copiedFileCount = copiedFileCount;
            outReport->manifestPath = manifestPath;
        }

        LOG_INFO("[AssetPipelineManager] Cooked assets=%zu textures=%zu converted=%zu copied=%zu manifest=%s",
            assets.size(),
            textureCount,
            convertedTextureCount,
            copiedFileCount,
            toUtf8Path(manifestPath).c_str());

        return true;
    }

    bool AssetPipelineManager::buildPak(const std::filesystem::path& cookedRoot, const std::filesystem::path& pakPath, PackageReport* outReport)
    {
        const std::filesystem::path normalizedRoot = normalizePathKey(cookedRoot);

        std::vector<std::filesystem::path> files;
        std::error_code iterEc;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(normalizedRoot, std::filesystem::directory_options::skip_permission_denied, iterEc))
        {
            if (iterEc)
            {
                continue;
            }

            if (!entry.is_regular_file(iterEc) || iterEc)
            {
                continue;
            }

            const std::string ext = toLower(entry.path().extension().string());
            if (ext == ".pak" || ext == ".patchpak")
            {
                continue;
            }

            files.push_back(normalizePathKey(entry.path()));
        }

        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());

        std::vector<PakEntry> entries;
        entries.reserve(files.size());
        std::vector<uint8_t> archiveData;

        for (const std::filesystem::path& filePath : files)
        {
            std::error_code relEc;
            std::filesystem::path relativePath = std::filesystem::relative(filePath, normalizedRoot, relEc);
            if (relEc)
            {
                relativePath = filePath.filename();
            }

            const std::vector<uint8_t> bytes = readBinary(filePath);
            if (bytes.empty())
            {
                std::error_code fileEc;
                if (std::filesystem::file_size(filePath, fileEc) > 0)
                {
                    return false;
                }
            }

            PakEntry entry{};
            entry.path = relativePath.generic_string();
            entry.offset = kPakHeaderSize + static_cast<uint64_t>(archiveData.size());
            entry.size = bytes.size();
            entry.hash = bytes.empty() ? 0 : fnv1a64(bytes.data(), bytes.size());
            entry.flags = 0;

            archiveData.insert(archiveData.end(), bytes.begin(), bytes.end());
            entries.push_back(std::move(entry));
        }

        const uint64_t directorySize = directoryBytesForPak(entries);
        const uint64_t directoryOffset = kPakHeaderSize + static_cast<uint64_t>(archiveData.size());

        std::error_code ec;
        std::filesystem::create_directories(pakPath.parent_path(), ec);
        if (ec)
        {
            return false;
        }

        std::ofstream out(pakPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }

        writePakHeader(out, static_cast<uint32_t>(entries.size()), directoryOffset, directorySize);
        if (!archiveData.empty())
        {
            out.write(reinterpret_cast<const char*>(archiveData.data()), static_cast<std::streamsize>(archiveData.size()));
        }

        if (!writePakDirectory(out, entries))
        {
            return false;
        }

        if (!out.good())
        {
            return false;
        }

        const std::filesystem::path manifestPath = pakManifestPath(pakPath);
        std::ostringstream manifest;
        manifest << "manifestVersion=1\n";
        manifest << "pakPath=" << toRelativeOrNormalized(pakPath).generic_string() << "\n";
        manifest << "entryCount=" << entries.size() << "\n";
        for (const PakEntry& entry : entries)
        {
            manifest << "entry=" << entry.path << "|" << entry.size << "|" << entry.hash << "\n";
        }

        if (!writeTextManifest(manifestPath, manifest.str()))
        {
            return false;
        }

        if (outReport)
        {
            outReport->entryCount = entries.size();
            outReport->payloadBytes = archiveData.size();
            outReport->pakHash = fileHash(pakPath);
            outReport->pakPath = pakPath;
            outReport->manifestPath = manifestPath;
        }

        LOG_INFO("[AssetPipelineManager] Pak built: %s entries=%zu bytes=%zu",
            toUtf8Path(pakPath).c_str(),
            entries.size(),
            archiveData.size());

        return true;
    }

    bool AssetPipelineManager::buildPatch(const std::filesystem::path& basePakPath, const std::filesystem::path& newPakPath, const std::filesystem::path& patchPath, PatchReport* outReport)
    {
        PakArchive baseArchive{};
        PakArchive newArchive{};
        if (!readPakArchive(basePakPath, baseArchive) || !readPakArchive(newPakPath, newArchive))
        {
            return false;
        }

        std::unordered_map<std::string, PakEntry> baseLookup;
        std::unordered_map<std::string, PakEntry> newLookup;
        for (const PakEntry& entry : baseArchive.entries)
        {
            baseLookup.emplace(entry.path, entry);
        }
        for (const PakEntry& entry : newArchive.entries)
        {
            newLookup.emplace(entry.path, entry);
        }

        std::vector<PakEntry> changedEntries;
        std::vector<uint8_t> changedData;
        std::vector<std::string> removedEntries;

        for (const auto& [path, entry] : newLookup)
        {
            const auto baseIt = baseLookup.find(path);
            const bool changed = baseIt == baseLookup.end() || baseIt->second.hash != entry.hash || baseIt->second.size != entry.size;
            if (!changed)
            {
                continue;
            }

            const std::vector<uint8_t> bytes = readPakEntryData(newPakPath, entry);
            if (bytes.size() != entry.size)
            {
                return false;
            }

            PakEntry patchEntry = entry;
            patchEntry.offset = kPatchHeaderSize + static_cast<uint64_t>(changedData.size());
            changedData.insert(changedData.end(), bytes.begin(), bytes.end());
            changedEntries.push_back(std::move(patchEntry));
        }

        for (const auto& [path, entry] : baseLookup)
        {
            if (newLookup.find(path) == newLookup.end())
            {
                removedEntries.push_back(entry.path);
            }
        }

        std::sort(changedEntries.begin(), changedEntries.end(), [](const PakEntry& a, const PakEntry& b)
            {
                return a.path < b.path;
            });
        std::sort(removedEntries.begin(), removedEntries.end());

        const uint64_t directorySize = directoryBytesForPatch(changedEntries);
        const uint64_t directoryOffset = kPatchHeaderSize + static_cast<uint64_t>(changedData.size());

        std::error_code ec;
        std::filesystem::create_directories(patchPath.parent_path(), ec);
        if (ec)
        {
            return false;
        }

        std::ofstream out(patchPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }

        writePatchHeader(out,
            static_cast<uint32_t>(changedEntries.size()),
            static_cast<uint32_t>(removedEntries.size()),
            baseArchive.pakHash,
            newArchive.pakHash,
            directoryOffset,
            directorySize);

        if (!changedData.empty())
        {
            out.write(reinterpret_cast<const char*>(changedData.data()), static_cast<std::streamsize>(changedData.size()));
        }

        if (!writePakDirectory(out, changedEntries))
        {
            return false;
        }

        if (!out.good())
        {
            return false;
        }

        const std::filesystem::path manifestPath = patchManifestPath(patchPath);
        std::ostringstream manifest;
        manifest << "manifestVersion=1\n";
        manifest << "basePak=" << toRelativeOrNormalized(basePakPath).generic_string() << "\n";
        manifest << "newPak=" << toRelativeOrNormalized(newPakPath).generic_string() << "\n";
        manifest << "changedCount=" << changedEntries.size() << "\n";
        manifest << "removedCount=" << removedEntries.size() << "\n";
        for (const PakEntry& entry : changedEntries)
        {
            manifest << "changed=" << entry.path << "|" << entry.size << "|" << entry.hash << "\n";
        }
        for (const std::string& removed : removedEntries)
        {
            manifest << "removed=" << removed << "\n";
        }

        if (!writeTextManifest(manifestPath, manifest.str()))
        {
            return false;
        }

        if (outReport)
        {
            outReport->changedCount = changedEntries.size();
            outReport->removedCount = removedEntries.size();
            outReport->patchHash = fileHash(patchPath);
            outReport->patchPath = patchPath;
            outReport->manifestPath = manifestPath;
        }

        LOG_INFO("[AssetPipelineManager] Patch built: %s changed=%zu removed=%zu",
            toUtf8Path(patchPath).c_str(),
            changedEntries.size(),
            removedEntries.size());

        return true;
    }
}