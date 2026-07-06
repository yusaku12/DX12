#include "pch.h"
#include "MaterialGraphEditorWindow.h"

#include <sstream>

namespace
{
    enum class ValueType : int
    {
        Invalid = -1,
        Color = 0,
        Scalar,
        Normal,
    };

    enum class GraphDomain : int
    {
        Surface = 0,
        Particle,
        PostEffect,
    };

    enum class GraphNodeType : int
    {
        BaseColorTexture = 0,
        NormalTexture,
        MaterialColor,
        MaterialMetallic,
        MaterialRoughness,
        MaterialAO,
        ConstantColor,
        ConstantScalar,
        AddColor,
        MultiplyColor,
        MultiplyScalar,
        LerpColor,
        SaturateScalar,
        OneMinusScalar,
        UnpackNormal,
        BlendNormal,
        NormalStrength,
        ToScalarR,
    };

    struct GraphNode
    {
        int id = -1;
        GraphNodeType type = GraphNodeType::ConstantColor;
        ImVec2 position = ImVec2(40.0f, 40.0f);
        std::array<float, 4> value = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<int, 3> inputs = { -1, -1, -1 };
    };

    struct MaterialGraph
    {
        int graphId = 0;
        std::string name = "Default";
        GraphDomain domain = GraphDomain::Surface;
        std::vector<GraphNode> nodes;
        int nextNodeId = 1;
        int selectedNodeId = -1;

        int outBaseColor = -1;
        int outMetallic = -1;
        int outRoughness = -1;
        int outAo = -1;
        int outAlpha = -1;
        int outNormal = -1;
    };

    struct MaterialBinding
    {
        std::string materialName;
        int graphId = 0;
    };

    struct GraphState
    {
        char graphFilePath[260] = "Data/MaterialGraph/Library.mgraph";
        char bindingFilePath[260] = "Data/MaterialGraph/MaterialGraphBindings.txt";
        char newBindingMaterial[128] = "";
        int newBindingGraphId = 0;

        std::vector<MaterialGraph> graphs;
        int activeGraphIndex = 0;
        std::vector<MaterialBinding> bindings;
        std::string status;
    };

    GraphState s_state{};

    constexpr const char* kGeneratedHlslPath = "HLSL/MaterialGraphGenerated.hlsli";

    void sanitizeOutputsForDomain(MaterialGraph& graph);

    ImVec2 addV2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x + b.x, a.y + b.y);
    }

    ImVec2 subV2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x - b.x, a.y - b.y);
    }

    std::vector<std::string> splitByTab(const std::string& line)
    {
        std::vector<std::string> parts;
        std::string token;
        std::istringstream iss(line);
        while (std::getline(iss, token, '\t'))
        {
            parts.push_back(token);
        }
        return parts;
    }

    const char* valueTypeName(ValueType type)
    {
        switch (type)
        {
        case ValueType::Color: return "Color";
        case ValueType::Scalar: return "Scalar";
        case ValueType::Normal: return "Normal";
        default: return "Invalid";
        }
    }

    const char* graphDomainName(GraphDomain domain)
    {
        switch (domain)
        {
        case GraphDomain::Surface: return "Surface";
        case GraphDomain::Particle: return "Particle";
        case GraphDomain::PostEffect: return "PostEffect";
        default: return "Unknown";
        }
    }

    const char* graphDomainDispatchName(GraphDomain domain)
    {
        switch (domain)
        {
        case GraphDomain::Surface: return "Surface";
        case GraphDomain::Particle: return "Particle";
        case GraphDomain::PostEffect: return "PostEffect";
        default: return "Surface";
        }
    }

    bool isNodeAllowedInDomain(GraphNodeType type, GraphDomain domain)
    {
        switch (domain)
        {
        case GraphDomain::Surface:
            return true;
        case GraphDomain::Particle:
            if (type == GraphNodeType::MaterialMetallic ||
                type == GraphNodeType::MaterialRoughness ||
                type == GraphNodeType::MaterialAO)
            {
                return false;
            }
            return true;
        case GraphDomain::PostEffect:
            if (type == GraphNodeType::MaterialMetallic ||
                type == GraphNodeType::MaterialRoughness ||
                type == GraphNodeType::MaterialAO ||
                type == GraphNodeType::MaterialColor ||
                type == GraphNodeType::NormalTexture ||
                type == GraphNodeType::UnpackNormal ||
                type == GraphNodeType::BlendNormal ||
                type == GraphNodeType::NormalStrength)
            {
                return false;
            }
            return true;
        default:
            return true;
        }
    }

    const char* nodeTypeName(GraphNodeType type)
    {
        switch (type)
        {
        case GraphNodeType::BaseColorTexture: return "BaseColorTexture";
        case GraphNodeType::NormalTexture: return "NormalTexture";
        case GraphNodeType::MaterialColor: return "MaterialColor";
        case GraphNodeType::MaterialMetallic: return "MaterialMetallic";
        case GraphNodeType::MaterialRoughness: return "MaterialRoughness";
        case GraphNodeType::MaterialAO: return "MaterialAO";
        case GraphNodeType::ConstantColor: return "ConstantColor";
        case GraphNodeType::ConstantScalar: return "ConstantScalar";
        case GraphNodeType::AddColor: return "AddColor";
        case GraphNodeType::MultiplyColor: return "MultiplyColor";
        case GraphNodeType::MultiplyScalar: return "MultiplyScalar";
        case GraphNodeType::LerpColor: return "LerpColor";
        case GraphNodeType::SaturateScalar: return "SaturateScalar";
        case GraphNodeType::OneMinusScalar: return "OneMinusScalar";
        case GraphNodeType::UnpackNormal: return "UnpackNormal";
        case GraphNodeType::BlendNormal: return "BlendNormal";
        case GraphNodeType::NormalStrength: return "NormalStrength";
        case GraphNodeType::ToScalarR: return "ToScalarR";
        default: return "Unknown";
        }
    }

    bool tryParseNodeType(const std::string& text, GraphNodeType& outType)
    {
        for (int i = static_cast<int>(GraphNodeType::BaseColorTexture);
             i <= static_cast<int>(GraphNodeType::ToScalarR);
             ++i)
        {
            const GraphNodeType t = static_cast<GraphNodeType>(i);
            if (text == nodeTypeName(t))
            {
                outType = t;
                return true;
            }
        }
        return false;
    }

    ValueType outputType(GraphNodeType type)
    {
        switch (type)
        {
        case GraphNodeType::BaseColorTexture:
        case GraphNodeType::NormalTexture:
        case GraphNodeType::MaterialColor:
        case GraphNodeType::ConstantColor:
        case GraphNodeType::AddColor:
        case GraphNodeType::MultiplyColor:
        case GraphNodeType::LerpColor:
            return ValueType::Color;
        case GraphNodeType::MaterialMetallic:
        case GraphNodeType::MaterialRoughness:
        case GraphNodeType::MaterialAO:
        case GraphNodeType::ConstantScalar:
        case GraphNodeType::MultiplyScalar:
        case GraphNodeType::SaturateScalar:
        case GraphNodeType::OneMinusScalar:
        case GraphNodeType::ToScalarR:
            return ValueType::Scalar;
        case GraphNodeType::UnpackNormal:
        case GraphNodeType::BlendNormal:
        case GraphNodeType::NormalStrength:
            return ValueType::Normal;
        default:
            return ValueType::Invalid;
        }
    }

    int inputSlotCount(GraphNodeType type)
    {
        switch (type)
        {
        case GraphNodeType::AddColor:
        case GraphNodeType::MultiplyColor:
        case GraphNodeType::MultiplyScalar:
        case GraphNodeType::BlendNormal:
        case GraphNodeType::NormalStrength:
            return 2;
        case GraphNodeType::LerpColor:
            return 3;
        case GraphNodeType::SaturateScalar:
        case GraphNodeType::OneMinusScalar:
        case GraphNodeType::UnpackNormal:
        case GraphNodeType::ToScalarR:
            return 1;
        default:
            return 0;
        }
    }

    ValueType inputType(GraphNodeType type, int slot)
    {
        switch (type)
        {
        case GraphNodeType::AddColor:
        case GraphNodeType::MultiplyColor:
            return ValueType::Color;
        case GraphNodeType::MultiplyScalar:
            return ValueType::Scalar;
        case GraphNodeType::LerpColor:
            return (slot == 2) ? ValueType::Scalar : ValueType::Color;
        case GraphNodeType::SaturateScalar:
        case GraphNodeType::OneMinusScalar:
            return ValueType::Scalar;
        case GraphNodeType::UnpackNormal:
            return ValueType::Color;
        case GraphNodeType::BlendNormal:
            return (slot == 2) ? ValueType::Scalar : ValueType::Normal;
        case GraphNodeType::NormalStrength:
            return (slot == 0) ? ValueType::Normal : ValueType::Scalar;
        case GraphNodeType::ToScalarR:
            return ValueType::Color;
        default:
            return ValueType::Invalid;
        }
    }

    MaterialGraph* currentGraph()
    {
        if (s_state.activeGraphIndex < 0 || s_state.activeGraphIndex >= static_cast<int>(s_state.graphs.size()))
        {
            return nullptr;
        }
        return &s_state.graphs[s_state.activeGraphIndex];
    }

    const MaterialGraph* currentGraphConst()
    {
        if (s_state.activeGraphIndex < 0 || s_state.activeGraphIndex >= static_cast<int>(s_state.graphs.size()))
        {
            return nullptr;
        }
        return &s_state.graphs[s_state.activeGraphIndex];
    }

    GraphNode* findNodeById(MaterialGraph& graph, int id)
    {
        for (GraphNode& node : graph.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    const GraphNode* findNodeByIdConst(const MaterialGraph& graph, int id)
    {
        for (const GraphNode& node : graph.nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    std::string fmtFloat(float value)
    {
        const float safe = std::isfinite(value) ? value : 0.0f;
        return std::format("{:.6f}f", safe);
    }

    std::string fallbackExpression(ValueType expectedType)
    {
        switch (expectedType)
        {
        case ValueType::Color: return "float4(1.0f, 1.0f, 1.0f, 1.0f)";
        case ValueType::Scalar: return "1.0f";
        case ValueType::Normal: return "float3(0.0f, 0.0f, 1.0f)";
        default: return "0.0f";
        }
    }

    std::string makeDefaultGraphName(int graphId)
    {
        return std::format("Graph{}", graphId);
    }

    int nextGraphId()
    {
        int maxId = -1;
        for (const MaterialGraph& graph : s_state.graphs)
        {
            maxId = std::max(maxId, graph.graphId);
        }
        return maxId + 1;
    }

    void initializeDefaultGraph(MaterialGraph& graph)
    {
        graph.nodes.clear();
        graph.nextNodeId = 1;
        graph.selectedNodeId = -1;

        GraphNode base;
        base.id = graph.nextNodeId++;
        base.type = GraphNodeType::BaseColorTexture;
        base.position = ImVec2(40.0f, 70.0f);

        GraphNode color;
        color.id = graph.nextNodeId++;
        color.type = GraphNodeType::MaterialColor;
        color.position = ImVec2(40.0f, 190.0f);

        GraphNode mul;
        mul.id = graph.nextNodeId++;
        mul.type = GraphNodeType::MultiplyColor;
        mul.position = ImVec2(300.0f, 120.0f);
        mul.inputs[0] = base.id;
        mul.inputs[1] = color.id;

        GraphNode metallic;
        metallic.id = graph.nextNodeId++;
        metallic.type = GraphNodeType::MaterialMetallic;
        metallic.position = ImVec2(40.0f, 330.0f);

        GraphNode roughness;
        roughness.id = graph.nextNodeId++;
        roughness.type = GraphNodeType::MaterialRoughness;
        roughness.position = ImVec2(40.0f, 450.0f);

        GraphNode ao;
        ao.id = graph.nextNodeId++;
        ao.type = GraphNodeType::MaterialAO;
        ao.position = ImVec2(40.0f, 570.0f);

        GraphNode normalTex;
        normalTex.id = graph.nextNodeId++;
        normalTex.type = GraphNodeType::NormalTexture;
        normalTex.position = ImVec2(40.0f, 690.0f);

        GraphNode unpackNormal;
        unpackNormal.id = graph.nextNodeId++;
        unpackNormal.type = GraphNodeType::UnpackNormal;
        unpackNormal.position = ImVec2(300.0f, 690.0f);
        unpackNormal.inputs[0] = normalTex.id;

        graph.nodes.push_back(base);
        graph.nodes.push_back(color);
        graph.nodes.push_back(mul);
        graph.nodes.push_back(metallic);
        graph.nodes.push_back(roughness);
        graph.nodes.push_back(ao);
        graph.nodes.push_back(normalTex);
        graph.nodes.push_back(unpackNormal);

        graph.outBaseColor = mul.id;
        graph.outMetallic = metallic.id;
        graph.outRoughness = roughness.id;
        graph.outAo = ao.id;
        graph.outAlpha = mul.id;
        graph.outNormal = unpackNormal.id;
        graph.selectedNodeId = mul.id;

        if (graph.domain == GraphDomain::PostEffect)
        {
            graph.nodes.erase(
                std::remove_if(graph.nodes.begin(), graph.nodes.end(), [](const GraphNode& node)
                    {
                        return !isNodeAllowedInDomain(node.type, GraphDomain::PostEffect);
                    }),
                graph.nodes.end());
        }

            sanitizeOutputsForDomain(graph);
    }

    void ensureDefaultState()
    {
        if (!s_state.graphs.empty())
        {
            return;
        }

        MaterialGraph surface;
        surface.graphId = 0;
        surface.name = "DefaultSurface";
        surface.domain = GraphDomain::Surface;
        initializeDefaultGraph(surface);
        s_state.graphs.push_back(std::move(surface));

        MaterialGraph particle;
        particle.graphId = 1;
        particle.name = "DefaultParticle";
        particle.domain = GraphDomain::Particle;
        initializeDefaultGraph(particle);
        s_state.graphs.push_back(std::move(particle));

        MaterialGraph post;
        post.graphId = 2;
        post.name = "DefaultPostEffect";
        post.domain = GraphDomain::PostEffect;
        initializeDefaultGraph(post);
        s_state.graphs.push_back(std::move(post));

        s_state.activeGraphIndex = 0;
    }

    bool saveBindingsToFile(const std::filesystem::path& filePath)
    {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        std::ofstream out(filePath, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "version\t1\n";
        for (const MaterialBinding& binding : s_state.bindings)
        {
            out << "bind\t" << binding.graphId << "\t" << binding.materialName << "\n";
        }

        return out.good();
    }

    bool loadBindingsFromFile(const std::filesystem::path& filePath)
    {
        std::ifstream in(filePath);
        if (!in)
        {
            return false;
        }

        std::vector<MaterialBinding> loaded;
        std::string line;
        int version = 0;

        while (std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::vector<std::string> parts = splitByTab(line);
            if (parts.empty())
            {
                continue;
            }

            if (parts[0] == "version")
            {
                if (parts.size() < 2)
                {
                    return false;
                }
                version = std::stoi(parts[1]);
            }
            else if (parts[0] == "bind")
            {
                if (parts.size() < 3)
                {
                    return false;
                }

                MaterialBinding b;
                b.graphId = std::stoi(parts[1]);
                b.materialName = parts[2];
                loaded.push_back(std::move(b));
            }
        }

        if (version != 1)
        {
            return false;
        }

        s_state.bindings = std::move(loaded);
        return true;
    }

    void sanitizeOutputsForDomain(MaterialGraph& graph)
    {
        if (graph.domain == GraphDomain::PostEffect)
        {
            graph.outMetallic = -1;
            graph.outRoughness = -1;
            graph.outAo = -1;
            graph.outNormal = -1;
        }
        else if (graph.domain == GraphDomain::Particle)
        {
            graph.outNormal = -1;
        }
    }

    bool hasNodeType(const MaterialGraph& graph, GraphNodeType type)
    {
        return std::any_of(graph.nodes.begin(), graph.nodes.end(), [type](const GraphNode& node)
            {
                return node.type == type;
            });
    }

    bool hasConnectedOutput(const MaterialGraph& graph, int outputNodeId)
    {
        if (outputNodeId < 0)
        {
            return false;
        }
        return std::any_of(graph.nodes.begin(), graph.nodes.end(), [outputNodeId](const GraphNode& node)
            {
                return node.id == outputNodeId;
            });
    }

    bool nameSuggestsPostEffect(const std::string& graphName)
    {
        std::string lower = graphName;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        static const char* kPostKeywords[] =
        {
            "post", "bloom", "dof", "depthoffield", "motionblur", "colorgrade", "tonemap", "vignette", "ui"
        };

        for (const char* keyword : kPostKeywords)
        {
            if (lower.find(keyword) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    GraphDomain inferDomainFromV2Graph(const MaterialGraph& graph)
    {
        const bool usesMaterialNodes =
            hasNodeType(graph, GraphNodeType::MaterialColor) ||
            hasNodeType(graph, GraphNodeType::MaterialMetallic) ||
            hasNodeType(graph, GraphNodeType::MaterialRoughness) ||
            hasNodeType(graph, GraphNodeType::MaterialAO);

        const bool usesNormalNodes =
            hasNodeType(graph, GraphNodeType::NormalTexture) ||
            hasNodeType(graph, GraphNodeType::UnpackNormal) ||
            hasNodeType(graph, GraphNodeType::BlendNormal) ||
            hasNodeType(graph, GraphNodeType::NormalStrength) ||
            hasConnectedOutput(graph, graph.outNormal);

        if (usesMaterialNodes || usesNormalNodes)
        {
            return GraphDomain::Surface;
        }

        const bool writesPbr =
            hasConnectedOutput(graph, graph.outMetallic) ||
            hasConnectedOutput(graph, graph.outRoughness) ||
            hasConnectedOutput(graph, graph.outAo);

        if (!writesPbr || nameSuggestsPostEffect(graph.name))
        {
            return GraphDomain::PostEffect;
        }

        return GraphDomain::Particle;
    }

    void migrateV2GraphDomains(std::vector<MaterialGraph>& graphs, int& outParticleCount, int& outPostEffectCount)
    {
        outParticleCount = 0;
        outPostEffectCount = 0;

        for (MaterialGraph& graph : graphs)
        {
            const GraphDomain inferred = inferDomainFromV2Graph(graph);
            graph.domain = inferred;
            sanitizeOutputsForDomain(graph);

            if (inferred == GraphDomain::Particle)
            {
                ++outParticleCount;
            }
            else if (inferred == GraphDomain::PostEffect)
            {
                ++outPostEffectCount;
            }
        }
    }

    bool saveGraphLibraryToFile(const std::filesystem::path& filePath)
    {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        std::ofstream out(filePath, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "version\t3\n";

        for (const MaterialGraph& graph : s_state.graphs)
        {
            out << "graph\t" << graph.graphId << "\t" << static_cast<int>(graph.domain) << "\t" << graph.name << "\n";
            out << "next\t" << graph.graphId << "\t" << graph.nextNodeId << "\n";
            out << "output\t" << graph.graphId << "\tbaseColor\t" << graph.outBaseColor << "\n";
            out << "output\t" << graph.graphId << "\tmetallic\t" << graph.outMetallic << "\n";
            out << "output\t" << graph.graphId << "\troughness\t" << graph.outRoughness << "\n";
            out << "output\t" << graph.graphId << "\tao\t" << graph.outAo << "\n";
            out << "output\t" << graph.graphId << "\talpha\t" << graph.outAlpha << "\n";
            out << "output\t" << graph.graphId << "\tnormal\t" << graph.outNormal << "\n";

            for (const GraphNode& node : graph.nodes)
            {
                out << "node\t" << graph.graphId << "\t" << node.id << "\t" << nodeTypeName(node.type)
                    << "\t" << node.position.x << "\t" << node.position.y
                    << "\t" << node.value[0] << "\t" << node.value[1] << "\t" << node.value[2] << "\t" << node.value[3]
                    << "\t" << node.inputs[0] << "\t" << node.inputs[1] << "\t" << node.inputs[2]
                    << "\n";
            }
        }

        return out.good();
    }

    bool loadGraphLibraryFromFile(const std::filesystem::path& filePath)
    {
        std::ifstream in(filePath);
        if (!in)
        {
            return false;
        }

        std::unordered_map<int, MaterialGraph> graphMap;
        std::vector<int> order;
        std::string line;
        int version = 0;

        while (std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::vector<std::string> parts = splitByTab(line);
            if (parts.empty())
            {
                continue;
            }

            if (parts[0] == "version")
            {
                if (parts.size() < 2)
                {
                    return false;
                }
                version = std::stoi(parts[1]);
                continue;
            }

            if (parts[0] == "graph")
            {
                if (parts.size() < 3)
                {
                    return false;
                }
                const int graphId = std::stoi(parts[1]);
                MaterialGraph graph;
                graph.graphId = graphId;
                if (version >= 3)
                {
                    if (parts.size() < 4)
                    {
                        return false;
                    }
                    const int domainValue = std::clamp(std::stoi(parts[2]), 0, 2);
                    graph.domain = static_cast<GraphDomain>(domainValue);
                    graph.name = parts[3];
                }
                else
                {
                    graph.domain = GraphDomain::Surface;
                    graph.name = parts[2];
                }
                graphMap[graphId] = std::move(graph);
                order.push_back(graphId);
                continue;
            }

            if (parts.size() < 3)
            {
                return false;
            }

            const int graphId = std::stoi(parts[1]);
            auto graphIt = graphMap.find(graphId);
            if (graphIt == graphMap.end())
            {
                return false;
            }
            MaterialGraph& graph = graphIt->second;

            if (parts[0] == "next")
            {
                if (parts.size() < 3) return false;
                graph.nextNodeId = std::stoi(parts[2]);
            }
            else if (parts[0] == "output")
            {
                if (parts.size() < 4) return false;
                const std::string& key = parts[2];
                const int value = std::stoi(parts[3]);
                if (key == "baseColor") graph.outBaseColor = value;
                else if (key == "metallic") graph.outMetallic = value;
                else if (key == "roughness") graph.outRoughness = value;
                else if (key == "ao") graph.outAo = value;
                else if (key == "alpha") graph.outAlpha = value;
                else if (key == "normal") graph.outNormal = value;
            }
            else if (parts[0] == "node")
            {
                if (parts.size() < 13) return false;

                GraphNode node;
                node.id = std::stoi(parts[2]);
                if (!tryParseNodeType(parts[3], node.type))
                {
                    return false;
                }
                node.position.x = std::stof(parts[4]);
                node.position.y = std::stof(parts[5]);
                node.value[0] = std::stof(parts[6]);
                node.value[1] = std::stof(parts[7]);
                node.value[2] = std::stof(parts[8]);
                node.value[3] = std::stof(parts[9]);
                node.inputs[0] = std::stoi(parts[10]);
                node.inputs[1] = std::stoi(parts[11]);
                node.inputs[2] = std::stoi(parts[12]);
                graph.nodes.push_back(node);
                graph.nextNodeId = std::max(graph.nextNodeId, node.id + 1);
            }
        }

        if ((version != 2 && version != 3) || graphMap.empty())
        {
            return false;
        }

        std::vector<MaterialGraph> loaded;
        loaded.reserve(order.size());
        for (int id : order)
        {
            auto it = graphMap.find(id);
            if (it != graphMap.end())
            {
                if (it->second.name.empty())
                {
                    it->second.name = makeDefaultGraphName(it->second.graphId);
                }
                loaded.push_back(std::move(it->second));
            }
        }

        if (loaded.empty())
        {
            return false;
        }

        int migratedParticle = 0;
        int migratedPostEffect = 0;
        if (version == 2)
        {
            migrateV2GraphDomains(loaded, migratedParticle, migratedPostEffect);
        }
        else
        {
            for (MaterialGraph& graph : loaded)
            {
                sanitizeOutputsForDomain(graph);
            }
        }

        s_state.graphs = std::move(loaded);
        s_state.activeGraphIndex = 0;
        s_state.graphs[0].selectedNodeId = -1;
        if (version == 2)
        {
            s_state.status = std::format("Migrated v2 graph domains: Particle={}, PostEffect={}", migratedParticle, migratedPostEffect);
        }
        return true;
    }

    int addNode(MaterialGraph& graph, GraphNodeType type)
    {
        GraphNode node;
        node.id = graph.nextNodeId++;
        node.type = type;
        node.position = ImVec2(120.0f + static_cast<float>(graph.nodes.size() % 4) * 220.0f,
            80.0f + static_cast<float>(graph.nodes.size() / 4) * 140.0f);
        if (type == GraphNodeType::ConstantScalar)
        {
            node.value = { 1.0f, 1.0f, 1.0f, 1.0f };
        }
        graph.nodes.push_back(node);
        graph.selectedNodeId = node.id;
        return node.id;
    }

    void removeNode(MaterialGraph& graph, int id)
    {
        graph.nodes.erase(
            std::remove_if(graph.nodes.begin(), graph.nodes.end(), [id](const GraphNode& node) { return node.id == id; }),
            graph.nodes.end());

        for (GraphNode& node : graph.nodes)
        {
            for (int& input : node.inputs)
            {
                if (input == id)
                {
                    input = -1;
                }
            }
        }

        if (graph.outBaseColor == id) graph.outBaseColor = -1;
        if (graph.outMetallic == id) graph.outMetallic = -1;
        if (graph.outRoughness == id) graph.outRoughness = -1;
        if (graph.outAo == id) graph.outAo = -1;
        if (graph.outAlpha == id) graph.outAlpha = -1;
        if (graph.outNormal == id) graph.outNormal = -1;

        if (graph.selectedNodeId == id)
        {
            graph.selectedNodeId = -1;
        }
    }

    bool drawTypedOutputSelector(MaterialGraph& graph, const char* label, int& outputNodeId, ValueType expected)
    {
        bool changed = false;

        std::string preview = "<Default>";
        if (const GraphNode* node = findNodeByIdConst(graph, outputNodeId))
        {
            preview = std::format("#{} {}", node->id, nodeTypeName(node->type));
        }

        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            const bool defaultSelected = (outputNodeId < 0);
            if (ImGui::Selectable("<Default>", defaultSelected))
            {
                outputNodeId = -1;
                changed = true;
            }

            for (const GraphNode& node : graph.nodes)
            {
                const bool compatible = (outputType(node.type) == expected);
                if (!compatible)
                {
                    ImGui::BeginDisabled();
                }

                const bool selected = (outputNodeId == node.id);
                const std::string line = std::format("#{} {} ({})", node.id, nodeTypeName(node.type), valueTypeName(outputType(node.type)));
                if (ImGui::Selectable(line.c_str(), selected) && compatible)
                {
                    outputNodeId = node.id;
                    changed = true;
                }

                if (!compatible)
                {
                    ImGui::EndDisabled();
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    std::string emitGraphFunction(const MaterialGraph& graph, std::string& outError)
    {
        std::unordered_map<int, const GraphNode*> nodeMap;
        nodeMap.reserve(graph.nodes.size());
        for (const GraphNode& node : graph.nodes)
        {
            nodeMap[node.id] = &node;
        }

        enum class VisitState : uint8_t
        {
            NotVisited,
            Visiting,
            Done
        };

        std::unordered_map<int, VisitState> visit;
        visit.reserve(graph.nodes.size());

        struct TypedExpr
        {
            ValueType type = ValueType::Invalid;
            std::string expr;
        };

        std::unordered_map<int, TypedExpr> cached;

        std::ostringstream body;

        std::function<TypedExpr(int, ValueType)> emitNode = [&](int id, ValueType expectedType) -> TypedExpr
        {
            if (id < 0)
            {
                return { expectedType, fallbackExpression(expectedType) };
            }

            auto nodeIt = nodeMap.find(id);
            if (nodeIt == nodeMap.end())
            {
                return { expectedType, fallbackExpression(expectedType) };
            }

            const GraphNode& node = *nodeIt->second;
            const ValueType nodeOutType = outputType(node.type);
            if (nodeOutType != expectedType)
            {
                outError = std::format("Type mismatch: graph {} node {} expected {} but got {}",
                    graph.graphId,
                    node.id,
                    valueTypeName(expectedType),
                    valueTypeName(nodeOutType));
                return { expectedType, fallbackExpression(expectedType) };
            }

            auto cacheIt = cached.find(id);
            if (cacheIt != cached.end())
            {
                return cacheIt->second;
            }

            VisitState state = VisitState::NotVisited;
            auto stateIt = visit.find(id);
            if (stateIt != visit.end())
            {
                state = stateIt->second;
            }

            if (state == VisitState::Visiting)
            {
                outError = std::format("Cycle detected in graph {} at node {}", graph.graphId, id);
                return { expectedType, fallbackExpression(expectedType) };
            }

            visit[id] = VisitState::Visiting;

            auto readInput = [&](int slot) -> TypedExpr
            {
                const ValueType inType = inputType(node.type, slot);
                return emitNode(node.inputs[slot], inType);
            };

            std::string expression;
            switch (node.type)
            {
            case GraphNodeType::BaseColorTexture:
                expression = "baseColorTex.Sample(linearSampler, uv)";
                break;
            case GraphNodeType::NormalTexture:
                expression = "normalTex.Sample(linearSampler, uv)";
                break;
            case GraphNodeType::MaterialColor:
                expression = "materialDiffuse";
                break;
            case GraphNodeType::MaterialMetallic:
                expression = "materialPbr.x";
                break;
            case GraphNodeType::MaterialRoughness:
                expression = "materialPbr.y";
                break;
            case GraphNodeType::MaterialAO:
                expression = "materialPbr.z";
                break;
            case GraphNodeType::ConstantColor:
                expression = std::format("float4({}, {}, {}, {})",
                    fmtFloat(node.value[0]),
                    fmtFloat(node.value[1]),
                    fmtFloat(node.value[2]),
                    fmtFloat(node.value[3]));
                break;
            case GraphNodeType::ConstantScalar:
                expression = fmtFloat(node.value[0]);
                break;
            case GraphNodeType::AddColor:
            {
                TypedExpr a = readInput(0);
                TypedExpr b = readInput(1);
                expression = std::format("({} + {})", a.expr, b.expr);
                break;
            }
            case GraphNodeType::MultiplyColor:
            {
                TypedExpr a = readInput(0);
                TypedExpr b = readInput(1);
                expression = std::format("({} * {})", a.expr, b.expr);
                break;
            }
            case GraphNodeType::MultiplyScalar:
            {
                TypedExpr a = readInput(0);
                TypedExpr b = readInput(1);
                expression = std::format("({} * {})", a.expr, b.expr);
                break;
            }
            case GraphNodeType::LerpColor:
            {
                TypedExpr a = readInput(0);
                TypedExpr b = readInput(1);
                TypedExpr s = readInput(2);
                expression = std::format("lerp({}, {}, saturate({}))", a.expr, b.expr, s.expr);
                break;
            }
            case GraphNodeType::SaturateScalar:
            {
                TypedExpr a = readInput(0);
                expression = std::format("saturate({})", a.expr);
                break;
            }
            case GraphNodeType::OneMinusScalar:
            {
                TypedExpr a = readInput(0);
                expression = std::format("(1.0f - {})", a.expr);
                break;
            }
            case GraphNodeType::UnpackNormal:
            {
                TypedExpr c = readInput(0);
                expression = std::format("normalize({}.xyz * 2.0f - 1.0f)", c.expr);
                break;
            }
            case GraphNodeType::BlendNormal:
            {
                TypedExpr n0 = readInput(0);
                TypedExpr n1 = readInput(1);
                TypedExpr s = readInput(2);
                expression = std::format("normalize(lerp({}, {}, saturate({})))", n0.expr, n1.expr, s.expr);
                break;
            }
            case GraphNodeType::NormalStrength:
            {
                TypedExpr n = readInput(0);
                TypedExpr s = readInput(1);
                expression = std::format("normalize(float3({}.xy * {}, {}.z))", n.expr, s.expr, n.expr);
                break;
            }
            case GraphNodeType::ToScalarR:
            {
                TypedExpr c = readInput(0);
                expression = std::format("({}).r", c.expr);
                break;
            }
            default:
                expression = fallbackExpression(expectedType);
                break;
            }

            const std::string varName = std::format("g{}_n{}", graph.graphId, id);
            const char* hlslType = "float";
            if (expectedType == ValueType::Color) hlslType = "float4";
            else if (expectedType == ValueType::Normal) hlslType = "float3";

            body << "    " << hlslType << " " << varName << " = " << expression << ";\n";

            visit[id] = VisitState::Done;
            TypedExpr result{ expectedType, varName };
            cached[id] = result;
            return result;
        };

        TypedExpr baseColor = emitNode(graph.outBaseColor, ValueType::Color);
        TypedExpr metallic = emitNode(graph.outMetallic, ValueType::Scalar);
        TypedExpr roughness = emitNode(graph.outRoughness, ValueType::Scalar);
        TypedExpr ao = emitNode(graph.outAo, ValueType::Scalar);
        TypedExpr alpha = emitNode(graph.outAlpha, ValueType::Scalar);

        bool hasNormalOutput = (graph.outNormal >= 0);
        TypedExpr normal = hasNormalOutput
            ? emitNode(graph.outNormal, ValueType::Normal)
            : TypedExpr{ ValueType::Normal, "float3(0.0f, 0.0f, 1.0f)" };

        std::ostringstream fn;
        fn << "MaterialGraphResult Evaluate" << graphDomainDispatchName(graph.domain) << "Graph_" << graph.graphId << "(\n"
            << "    float2 uv,\n"
            << "    float4 materialDiffuse,\n"
            << "    float3 materialPbr,\n"
            << "    Texture2D<float4> baseColorTex,\n"
            << "    Texture2D<float4> normalTex,\n"
            << "    SamplerState linearSampler)\n"
            << "{\n"
            << body.str()
            << "    MaterialGraphResult outValue;\n"
            << "    outValue.baseColor = " << baseColor.expr << ";\n"
            << "    outValue.metallic = saturate(" << metallic.expr << ");\n"
            << "    outValue.roughness = saturate(" << roughness.expr << ");\n"
            << "    outValue.ao = saturate(" << ao.expr << ");\n"
            << "    outValue.alpha = saturate(" << alpha.expr << ");\n";

        if (hasNormalOutput)
        {
            fn << "    outValue.normalTS = normalize(" << normal.expr << ");\n"
                << "    outValue.hasNormal = 1.0f;\n";
        }
        else
        {
            fn << "    outValue.normalTS = float3(0.0f, 0.0f, 1.0f);\n"
                << "    outValue.hasNormal = 0.0f;\n";
        }

        fn << "    return outValue;\n"
            << "}\n\n";

        return fn.str();
    }

    void appendDomainDispatch(std::ostringstream& file, GraphDomain domain, const std::vector<MaterialGraph>& graphs)
    {
        std::vector<const MaterialGraph*> domainGraphs;
        domainGraphs.reserve(graphs.size());
        for (const MaterialGraph& graph : graphs)
        {
            if (graph.domain == domain)
            {
                domainGraphs.push_back(&graph);
            }
        }

        if (domainGraphs.empty())
        {
            domainGraphs.push_back(&graphs.front());
        }

        const int fallbackGraphId = domainGraphs.front()->graphId;

        file
            << "MaterialGraphResult Evaluate" << graphDomainDispatchName(domain) << "GraphById(\n"
            << "    int graphId,\n"
            << "    float2 uv,\n"
            << "    float4 materialDiffuse,\n"
            << "    float3 materialPbr,\n"
            << "    Texture2D<float4> baseColorTex,\n"
            << "    Texture2D<float4> normalTex,\n"
            << "    SamplerState linearSampler)\n"
            << "{\n"
            << "    switch (graphId)\n"
            << "    {\n";

        for (const MaterialGraph* graph : domainGraphs)
        {
            file << "    case " << graph->graphId << ": return Evaluate" << graphDomainDispatchName(graph->domain) << "Graph_" << graph->graphId
                << "(uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);\n";
        }

        file
            << "    default: return Evaluate" << graphDomainDispatchName(domain) << "Graph_" << fallbackGraphId
            << "(uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);\n"
            << "    }\n"
            << "}\n\n";
    }

    bool generateHlslFile(std::string& outError)
    {
        if (s_state.graphs.empty())
        {
            outError = "No graph data.";
            return false;
        }

        std::ostringstream file;
        file
            << "#ifndef MATERIAL_GRAPH_GENERATED_HLSLI\n"
            << "#define MATERIAL_GRAPH_GENERATED_HLSLI\n\n"
            << "struct MaterialGraphResult\n"
            << "{\n"
            << "    float4 baseColor;\n"
            << "    float metallic;\n"
            << "    float roughness;\n"
            << "    float ao;\n"
            << "    float alpha;\n"
            << "    float3 normalTS;\n"
            << "    float hasNormal;\n"
            << "};\n\n";

        for (const MaterialGraph& graph : s_state.graphs)
        {
            std::string fn = emitGraphFunction(graph, outError);
            if (!outError.empty())
            {
                return false;
            }
            file << fn;
        }

        appendDomainDispatch(file, GraphDomain::Surface, s_state.graphs);
        appendDomainDispatch(file, GraphDomain::Particle, s_state.graphs);
        appendDomainDispatch(file, GraphDomain::PostEffect, s_state.graphs);

        // 互換性維持: 既存呼び出しは Surface にマップ
        file
            << "MaterialGraphResult EvaluateMaterialGraphById(\n"
            << "    int graphId,\n"
            << "    float2 uv,\n"
            << "    float4 materialDiffuse,\n"
            << "    float3 materialPbr,\n"
            << "    Texture2D<float4> baseColorTex,\n"
            << "    Texture2D<float4> normalTex,\n"
            << "    SamplerState linearSampler)\n"
            << "{\n"
            << "    return EvaluateSurfaceGraphById(graphId, uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);\n"
            << "}\n\n"
            << "#endif\n";

        std::ofstream out(kGeneratedHlslPath, std::ios::trunc);
        if (!out)
        {
            outError = std::format("Failed to open output: {}", kGeneratedHlslPath);
            return false;
        }

        out << file.str();
        if (!out.good())
        {
            outError = std::format("Failed to write output: {}", kGeneratedHlslPath);
            return false;
        }

        return true;
    }

    void drawGraphCanvas(MaterialGraph& graph)
    {
        ImGui::BeginChild("MaterialGraphCanvas", ImVec2(0.0f, 330.0f), true);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        for (const GraphNode& node : graph.nodes)
        {
            const ImVec2 p0 = addV2(canvasPos, node.position);
            const float height = 64.0f + static_cast<float>(inputSlotCount(node.type)) * 18.0f;
            const ImVec2 p1 = addV2(p0, ImVec2(220.0f, height));

            const bool selected = (node.id == graph.selectedNodeId);
            const ImU32 fillColor = selected ? IM_COL32(85, 118, 184, 225) : IM_COL32(55, 62, 80, 225);

            drawList->AddRectFilled(p0, p1, fillColor, 6.0f);
            drawList->AddRect(p0, p1, IM_COL32(220, 220, 235, 180), 6.0f, 0, 1.5f);
            drawList->AddText(addV2(p0, ImVec2(8.0f, 8.0f)), IM_COL32(240, 240, 240, 255), std::format("#{}", node.id).c_str());
            drawList->AddText(addV2(p0, ImVec2(8.0f, 28.0f)), IM_COL32(225, 225, 185, 255), nodeTypeName(node.type));
            drawList->AddText(addV2(p0, ImVec2(8.0f, 46.0f)), IM_COL32(190, 205, 235, 220), valueTypeName(outputType(node.type)));

            for (int i = 0; i < inputSlotCount(node.type); ++i)
            {
                const ImVec2 pinPos = addV2(p0, ImVec2(0.0f, 64.0f + i * 18.0f));
                drawList->AddCircleFilled(pinPos, 4.0f, IM_COL32(245, 190, 120, 220));
                drawList->AddText(addV2(pinPos, ImVec2(8.0f, -7.0f)), IM_COL32(230, 230, 230, 220), std::format("In{} ({})", i, valueTypeName(inputType(node.type, i))).c_str());
            }

            const ImVec2 outPinPos = addV2(p0, ImVec2(220.0f, 36.0f));
            drawList->AddCircleFilled(outPinPos, 4.0f, IM_COL32(120, 215, 160, 220));

            ImGui::SetCursorScreenPos(p0);
            ImGui::InvisibleButton(std::format("##MaterialNode{}", node.id).c_str(), ImVec2(220.0f, height));

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                graph.selectedNodeId = node.id;
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                GraphNode* mutableNode = findNodeById(graph, node.id);
                if (mutableNode)
                {
                    mutableNode->position.x += ImGui::GetIO().MouseDelta.x;
                    mutableNode->position.y += ImGui::GetIO().MouseDelta.y;
                }
            }
        }

        for (const GraphNode& node : graph.nodes)
        {
            const ImVec2 toNodePos = addV2(canvasPos, node.position);
            for (int i = 0; i < inputSlotCount(node.type); ++i)
            {
                const int srcNodeId = node.inputs[i];
                const GraphNode* source = findNodeByIdConst(graph, srcNodeId);
                if (!source)
                {
                    continue;
                }

                const ImVec2 from = addV2(addV2(canvasPos, source->position), ImVec2(220.0f, 36.0f));
                const ImVec2 to = addV2(toNodePos, ImVec2(0.0f, 64.0f + i * 18.0f));

                const bool typeOk = (outputType(source->type) == inputType(node.type, i));
                const ImU32 color = typeOk ? IM_COL32(200, 210, 230, 190) : IM_COL32(245, 90, 90, 230);
                drawList->AddBezierCubic(from, addV2(from, ImVec2(60.0f, 0.0f)), subV2(to, ImVec2(60.0f, 0.0f)), to, color, 2.0f);
            }
        }

        ImGui::EndChild();
    }

    void drawSelectedNodeEditor(MaterialGraph& graph)
    {
        if (graph.selectedNodeId < 0)
        {
            ImGui::TextDisabled("No node selected");
            return;
        }

        GraphNode* node = findNodeById(graph, graph.selectedNodeId);
        if (!node)
        {
            ImGui::TextDisabled("No node selected");
            return;
        }

        int typeIndex = static_cast<int>(node->type);
        const char* typeItems[] =
        {
            "BaseColorTexture",
            "NormalTexture",
            "MaterialColor",
            "MaterialMetallic",
            "MaterialRoughness",
            "MaterialAO",
            "ConstantColor",
            "ConstantScalar",
            "AddColor",
            "MultiplyColor",
            "MultiplyScalar",
            "LerpColor",
            "SaturateScalar",
            "OneMinusScalar",
            "UnpackNormal",
            "BlendNormal",
            "NormalStrength",
            "ToScalarR"
        };

        ImGui::Text("Node #%d", node->id);
        ImGui::Text("Output Type: %s", valueTypeName(outputType(node->type)));

        if (ImGui::Combo("Type", &typeIndex, typeItems, IM_ARRAYSIZE(typeItems)))
        {
            node->type = static_cast<GraphNodeType>(typeIndex);
            for (int i = inputSlotCount(node->type); i < 3; ++i)
            {
                node->inputs[i] = -1;
            }
        }

        if (node->type == GraphNodeType::ConstantColor)
        {
            ImGui::ColorEdit4("Color", node->value.data());
        }
        else if (node->type == GraphNodeType::ConstantScalar)
        {
            ImGui::SliderFloat("Scalar", &node->value[0], 0.0f, 1.0f, "%.3f");
            node->value[1] = node->value[0];
            node->value[2] = node->value[0];
            node->value[3] = node->value[0];
        }

        const int slotCount = inputSlotCount(node->type);
        for (int i = 0; i < slotCount; ++i)
        {
            const ValueType expected = inputType(node->type, i);
            std::string label = std::format("Input {} ({})", i, valueTypeName(expected));
            std::string preview = "<None>";
            if (const GraphNode* src = findNodeByIdConst(graph, node->inputs[i]))
            {
                preview = std::format("#{} {} ({})", src->id, nodeTypeName(src->type), valueTypeName(outputType(src->type)));
            }

            if (ImGui::BeginCombo(label.c_str(), preview.c_str()))
            {
                const bool noneSelected = (node->inputs[i] < 0);
                if (ImGui::Selectable("<None>", noneSelected))
                {
                    node->inputs[i] = -1;
                }

                for (const GraphNode& candidate : graph.nodes)
                {
                    if (candidate.id == node->id)
                    {
                        continue;
                    }

                    if (!isNodeAllowedInDomain(candidate.type, graph.domain))
                    {
                        continue;
                    }

                    const bool compatible = (outputType(candidate.type) == expected);
                    if (!compatible)
                    {
                        ImGui::BeginDisabled();
                    }

                    const bool selected = (candidate.id == node->inputs[i]);
                    const std::string line = std::format("#{} {} ({})", candidate.id, nodeTypeName(candidate.type), valueTypeName(outputType(candidate.type)));
                    if (ImGui::Selectable(line.c_str(), selected) && compatible)
                    {
                        node->inputs[i] = candidate.id;
                    }

                    if (!compatible)
                    {
                        ImGui::EndDisabled();
                    }
                }

                ImGui::EndCombo();
            }
        }
    }

    void drawGraphList()
    {
        ImGui::SeparatorText("Graph Library");

        static int newGraphDomain = static_cast<int>(GraphDomain::Surface);
        const char* domainItems[] = { "Surface", "Particle", "PostEffect" };
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("New Graph Domain", &newGraphDomain, domainItems, IM_ARRAYSIZE(domainItems));

        if (ImGui::Button("Add Graph"))
        {
            MaterialGraph graph;
            graph.graphId = nextGraphId();
            graph.name = makeDefaultGraphName(graph.graphId);
            graph.domain = static_cast<GraphDomain>(std::clamp(newGraphDomain, 0, 2));
            initializeDefaultGraph(graph);
            s_state.graphs.push_back(std::move(graph));
            s_state.activeGraphIndex = static_cast<int>(s_state.graphs.size()) - 1;
        }

        ImGui::SameLine();
        if (s_state.graphs.size() > 1 && ImGui::Button("Remove Graph"))
        {
            const int idx = s_state.activeGraphIndex;
            if (idx >= 0 && idx < static_cast<int>(s_state.graphs.size()))
            {
                const int removedGraphId = s_state.graphs[idx].graphId;
                s_state.graphs.erase(s_state.graphs.begin() + idx);
                s_state.activeGraphIndex = std::clamp(idx, 0, static_cast<int>(s_state.graphs.size()) - 1);

                for (MaterialBinding& binding : s_state.bindings)
                {
                    if (binding.graphId == removedGraphId)
                    {
                        binding.graphId = s_state.graphs.front().graphId;
                    }
                }
            }
        }

        if (ImGui::BeginListBox("Graphs", ImVec2(0.0f, 120.0f)))
        {
            for (int i = 0; i < static_cast<int>(s_state.graphs.size()); ++i)
            {
                const MaterialGraph& graph = s_state.graphs[i];
                const bool selected = (i == s_state.activeGraphIndex);
                const std::string label = std::format("ID {} [{}] : {}", graph.graphId, graphDomainName(graph.domain), graph.name);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    s_state.activeGraphIndex = i;
                }
            }
            ImGui::EndListBox();
        }

        if (MaterialGraph* graph = currentGraph())
        {
            char nameBuf[128] = {};
            strncpy_s(nameBuf, graph->name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Graph Name", nameBuf, IM_ARRAYSIZE(nameBuf)))
            {
                graph->name = nameBuf;
            }
            ImGui::Text("Graph ID: %d", graph->graphId);
            ImGui::Text("Domain: %s", graphDomainName(graph->domain));
        }
    }

    void drawBindingEditor()
    {
        ImGui::SeparatorText("Material Binding");

        ImGui::InputText("Material Name", s_state.newBindingMaterial, IM_ARRAYSIZE(s_state.newBindingMaterial));
        ImGui::InputInt("Bind Graph ID", &s_state.newBindingGraphId);

        if (ImGui::Button("Add / Update Binding"))
        {
            if (s_state.newBindingMaterial[0] != '\0')
            {
                bool updated = false;
                for (MaterialBinding& b : s_state.bindings)
                {
                    if (b.materialName == s_state.newBindingMaterial)
                    {
                        b.graphId = s_state.newBindingGraphId;
                        updated = true;
                        break;
                    }
                }

                if (!updated)
                {
                    MaterialBinding b;
                    b.materialName = s_state.newBindingMaterial;
                    b.graphId = s_state.newBindingGraphId;
                    s_state.bindings.push_back(std::move(b));
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Bindings"))
        {
            if (saveBindingsToFile(std::filesystem::path(s_state.bindingFilePath)))
            {
                s_state.status = "Binding save success.";
            }
            else
            {
                s_state.status = "Binding save failed.";
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Bindings"))
        {
            if (loadBindingsFromFile(std::filesystem::path(s_state.bindingFilePath)))
            {
                s_state.status = "Binding load success.";
            }
            else
            {
                s_state.status = "Binding load failed.";
            }
        }

        if (ImGui::BeginTable("BindingTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0.0f, 120.0f)))
        {
            ImGui::TableSetupColumn("Material");
            ImGui::TableSetupColumn("Graph ID", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < s_state.bindings.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s_state.bindings[i].materialName.c_str());

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputInt("##gid", &s_state.bindings[i].graphId);

                ImGui::TableNextColumn();
                if (ImGui::Button("Remove"))
                {
                    s_state.bindings.erase(s_state.bindings.begin() + static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
}

void drawMaterialGraphEditorWindow()
{
    ensureDefaultState();

    ImGui::InputText("Graph Library File", s_state.graphFilePath, IM_ARRAYSIZE(s_state.graphFilePath));
    ImGui::InputText("Binding File", s_state.bindingFilePath, IM_ARRAYSIZE(s_state.bindingFilePath));

    if (ImGui::Button("Load Graph Library"))
    {
        if (loadGraphLibraryFromFile(std::filesystem::path(s_state.graphFilePath)))
        {
            if (s_state.status.rfind("Migrated v2 graph domains:", 0) == 0)
            {
                s_state.status = std::format("Graph library load success. {}", s_state.status);
            }
            else
            {
                s_state.status = "Graph library load success.";
            }
        }
        else
        {
            s_state.status = "Graph library load failed.";
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Graph Library"))
    {
        if (saveGraphLibraryToFile(std::filesystem::path(s_state.graphFilePath)))
        {
            s_state.status = "Graph library save success.";
        }
        else
        {
            s_state.status = "Graph library save failed.";
        }
    }

    drawGraphList();

    MaterialGraph* graph = currentGraph();
    if (!graph)
    {
        ImGui::TextDisabled("No graph selected");
        return;
    }

    ImGui::SeparatorText("Node Editing");

    static int addTypeIndex = 0;
    const char* addTypeItems[] =
    {
        "BaseColorTexture",
        "NormalTexture",
        "MaterialColor",
        "MaterialMetallic",
        "MaterialRoughness",
        "MaterialAO",
        "ConstantColor",
        "ConstantScalar",
        "AddColor",
        "MultiplyColor",
        "MultiplyScalar",
        "LerpColor",
        "SaturateScalar",
        "OneMinusScalar",
        "UnpackNormal",
        "BlendNormal",
        "NormalStrength",
        "ToScalarR"
    };

    ImGui::SetNextItemWidth(220.0f);
    ImGui::Combo("Add Node Type", &addTypeIndex, addTypeItems, IM_ARRAYSIZE(addTypeItems));
    ImGui::SameLine();
    if (ImGui::Button("Add Node"))
    {
        const GraphNodeType addType = static_cast<GraphNodeType>(addTypeIndex);
        if (isNodeAllowedInDomain(addType, graph->domain))
        {
            addNode(*graph, addType);
        }
        else
        {
            s_state.status = "Node type is not available in this graph domain.";
        }
    }

    ImGui::SameLine();
    if (graph->selectedNodeId >= 0 && ImGui::Button("Remove Selected"))
    {
        removeNode(*graph, graph->selectedNodeId);
    }

    ImGui::SeparatorText("Graph Outputs");
    drawTypedOutputSelector(*graph, "BaseColor", graph->outBaseColor, ValueType::Color);
    drawTypedOutputSelector(*graph, "Alpha", graph->outAlpha, ValueType::Scalar);
    if (graph->domain == GraphDomain::Surface)
    {
        drawTypedOutputSelector(*graph, "Metallic", graph->outMetallic, ValueType::Scalar);
        drawTypedOutputSelector(*graph, "Roughness", graph->outRoughness, ValueType::Scalar);
        drawTypedOutputSelector(*graph, "AO", graph->outAo, ValueType::Scalar);
        drawTypedOutputSelector(*graph, "Normal", graph->outNormal, ValueType::Normal);
    }
    else if (graph->domain == GraphDomain::Particle)
    {
        drawTypedOutputSelector(*graph, "Metallic", graph->outMetallic, ValueType::Scalar);
        drawTypedOutputSelector(*graph, "Roughness", graph->outRoughness, ValueType::Scalar);
        drawTypedOutputSelector(*graph, "AO", graph->outAo, ValueType::Scalar);
        ImGui::TextDisabled("Normal output is not available in Particle domain.");
    }
    else
    {
        graph->outMetallic = -1;
        graph->outRoughness = -1;
        graph->outAo = -1;
        graph->outNormal = -1;
        ImGui::TextDisabled("Only BaseColor and Alpha are available in PostEffect domain.");
    }

    if (ImGui::Button("Generate HLSL"))
    {
        std::string err;
        if (generateHlslFile(err))
        {
            s_state.status = std::format("Generated {}", kGeneratedHlslPath);
        }
        else
        {
            s_state.status = err.empty() ? "Generate failed." : std::format("Generate failed: {}", err);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Output: %s", kGeneratedHlslPath);

    drawGraphCanvas(*graph);

    ImGui::SeparatorText("Selected Node");
    drawSelectedNodeEditor(*graph);

    drawBindingEditor();

    if (!s_state.status.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", s_state.status.c_str());
    }
}
