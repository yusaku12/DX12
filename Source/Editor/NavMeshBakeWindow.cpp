#include "pch.h"
#include "NavMeshBakeWindow.h"

#include "Component/ColliderComponent.h"
#include "Component/RigidbodyComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject/GameObjectRegistry.h"
#include "System/NavMeshSystem.h"

namespace
{
    struct BakeState
    {
        char outputPath[260] = "Data/NavMesh/Default.navmesh";
        float linkDistance = 6.0f;
        bool includeDynamic = false;
        bool autoLoadAfterBake = true;
        std::string status;
    };

    BakeState s_state{};

    std::vector<Vector3> collectBakePoints(bool includeDynamic)
    {
        std::vector<Vector3> points;

        const auto& objects = GameObjectRegistry::Instance().getAll();
        points.reserve(objects.size() * 2);

        for (GameObject* object : objects)
        {
            if (!object || object->isDestroyed())
            {
                continue;
            }

            auto* collider = object->getComponent<ColliderComponent>();
            if (!collider || collider->isTrigger())
            {
                continue;
            }

            auto* transform = object->getComponent<TransformComponent>();
            if (!transform)
            {
                continue;
            }

            auto* rigidbody = object->getComponent<RigidbodyComponent>();
            if (!includeDynamic && rigidbody && rigidbody->getType() == RigidbodyType::Dynamic)
            {
                continue;
            }

            const Vector3 base = transform->getPosition() + collider->getCenter();
            points.push_back(base);

            switch (collider->getShapeType())
            {
            case ColliderShapeType::Box:
            {
                const Vector3 ext = collider->getBoxHalfExtents();
                points.push_back(base + Vector3(ext.x, 0.0f, 0.0f));
                points.push_back(base + Vector3(-ext.x, 0.0f, 0.0f));
                points.push_back(base + Vector3(0.0f, 0.0f, ext.z));
                points.push_back(base + Vector3(0.0f, 0.0f, -ext.z));
                break;
            }
            case ColliderShapeType::Sphere:
            {
                const float r = collider->getSphereRadius();
                points.push_back(base + Vector3(r, 0.0f, 0.0f));
                points.push_back(base + Vector3(-r, 0.0f, 0.0f));
                break;
            }
            case ColliderShapeType::Capsule:
            {
                const float r = collider->getCapsuleRadius();
                points.push_back(base + Vector3(r, 0.0f, 0.0f));
                points.push_back(base + Vector3(-r, 0.0f, 0.0f));
                points.push_back(base + Vector3(0.0f, 0.0f, r));
                points.push_back(base + Vector3(0.0f, 0.0f, -r));
                break;
            }
            case ColliderShapeType::Plane:
            default:
                break;
            }
        }

        return points;
    }

    bool bakeNavMesh(const std::filesystem::path& outputPath, float linkDistance, bool includeDynamic, size_t& outNodeCount, size_t& outEdgeCount)
    {
        outNodeCount = 0;
        outEdgeCount = 0;

        std::vector<Vector3> points = collectBakePoints(includeDynamic);
        if (points.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);

        std::ofstream out(outputPath, std::ios::trunc);
        if (!out)
        {
            return false;
        }

        out << "version 1\n";
        out << "nodes " << points.size() << "\n";
        for (size_t i = 0; i < points.size(); ++i)
        {
            out << "node " << i << " "
                << points[i].x << " "
                << points[i].y << " "
                << points[i].z << "\n";
        }

        const float linkDistanceSq = linkDistance * linkDistance;
        size_t edgeCount = 0;
        for (size_t i = 0; i < points.size(); ++i)
        {
            for (size_t j = i + 1; j < points.size(); ++j)
            {
                const float distanceSq = (points[i] - points[j]).LengthSquared();
                if (distanceSq <= linkDistanceSq)
                {
                    out << "edge " << i << " " << j << "\n";
                    ++edgeCount;
                }
            }
        }

        if (!out.good())
        {
            return false;
        }

        outNodeCount = points.size();
        outEdgeCount = edgeCount;
        return true;
    }
}

void drawNavMeshBakeWindow()
{
    ImGui::InputText("Output", s_state.outputPath, IM_ARRAYSIZE(s_state.outputPath));
    ImGui::DragFloat("Link Distance", &s_state.linkDistance, 0.1f, 0.2f, 100.0f);
    ImGui::Checkbox("Include Dynamic Rigidbody", &s_state.includeDynamic);
    ImGui::Checkbox("Auto Load After Bake", &s_state.autoLoadAfterBake);

    if (ImGui::Button("Bake NavMesh"))
    {
        size_t nodeCount = 0;
        size_t edgeCount = 0;
        const std::filesystem::path outputPath(s_state.outputPath);

        if (bakeNavMesh(outputPath, s_state.linkDistance, s_state.includeDynamic, nodeCount, edgeCount))
        {
            if (s_state.autoLoadAfterBake)
            {
                NavMeshSystem::Instance().loadNavMesh(outputPath);
            }

            s_state.status = std::format("Bake success: nodes={} edges={} file={}", nodeCount, edgeCount, outputPath.string());
        }
        else
        {
            s_state.status = "Bake failed: no collider points or write error.";
        }
    }

    if (!s_state.status.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", s_state.status.c_str());
    }
}
