#include "pch.h"
#include "AnimatorControllerAsset.h"

namespace
{
    constexpr int kControllerFileVersion = 1;

    struct TokenReader
    {
        explicit TokenReader(std::istream& input)
            : in(input)
        {
        }

        bool expect(const char* token)
        {
            std::string value;
            if (!(in >> value)) return false;
            return value == token;
        }

        bool readString(std::string& out)
        {
            return static_cast<bool>(in >> std::quoted(out));
        }

        bool readInt(int& out)
        {
            return static_cast<bool>(in >> out);
        }

        bool readFloat(float& out)
        {
            return static_cast<bool>(in >> out);
        }

        bool readBool(bool& out)
        {
            int v = 0;
            if (!(in >> v)) return false;
            out = (v != 0);
            return true;
        }

        std::istream& in;
    };

    bool writeState(std::ostream& out, const AnimationState& state)
    {
        out << "STATE " << std::quoted(state.getName()) << "\n";
        out << "ANIM " << state.getAnimationIndex() << "\n";
        out << "LOOP " << static_cast<int>(state.getLoopMode()) << "\n";
        out << "SPEED " << state.getSpeed() << "\n";
        out << "POS " << state.getNodePosition().x << " " << state.getNodePosition().y << "\n";

        const BlendTreeData* blendTree = state.getBlendTree();
        out << "BLEND " << (blendTree ? 1 : 0) << "\n";
        if (blendTree)
        {
            out << "BLEND_TYPE " << static_cast<int>(blendTree->type) << "\n";
            out << "BLEND_PARAM_X " << std::quoted(blendTree->parameterX) << "\n";
            out << "BLEND_PARAM_Y " << std::quoted(blendTree->parameterY) << "\n";
            out << "BLEND_CHILD_COUNT " << blendTree->children.size() << "\n";
            for (const auto& child : blendTree->children)
            {
                out << "CHILD "
                    << child.animationIndex << " "
                    << child.threshold << " "
                    << child.position.x << " "
                    << child.position.y << " "
                    << child.timeScale << "\n";
            }
        }

        const auto& transitions = state.getTransitions();
        out << "TRANSITION_COUNT " << transitions.size() << "\n";
        for (const auto& t : transitions)
        {
            out << "TRANS "
                << std::quoted(t.destStateName) << " "
                << t.fadeDuration << " "
                << t.exitTime << " "
                << (t.hasExitTime ? 1 : 0) << " "
                << (t.interruptible ? 1 : 0) << " "
                << t.conditions.size() << "\n";

            for (const auto& cond : t.conditions)
            {
                out << "COND "
                    << std::quoted(cond.paramName) << " "
                    << static_cast<int>(cond.op) << " "
                    << cond.threshold << "\n";
            }
        }

        const auto& events = state.getEvents();
        out << "EVENT_COUNT " << events.size() << "\n";
        for (const auto& evt : events)
        {
            out << "EVENT " << std::quoted(evt.name) << " " << evt.normalizedTime << "\n";
        }

        out << "END_STATE\n";
        return out.good();
    }

    bool readState(TokenReader& reader, AnimationStateMachine& sm)
    {
        std::string stateName;
        if (!reader.readString(stateName)) return false;

        if (!reader.expect("ANIM")) return false;
        int animIndex = -1;
        if (!reader.readInt(animIndex)) return false;

        AnimationState* state = sm.addState(stateName, animIndex);
        if (!state) return false;

        if (!reader.expect("LOOP")) return false;
        int loopMode = 0;
        if (!reader.readInt(loopMode)) return false;
        state->setLoopMode(static_cast<LoopMode>(loopMode));

        if (!reader.expect("SPEED")) return false;
        float speed = 1.0f;
        if (!reader.readFloat(speed)) return false;
        state->setSpeed(speed);

        if (!reader.expect("POS")) return false;
        float posX = 0.0f;
        float posY = 0.0f;
        if (!reader.readFloat(posX) || !reader.readFloat(posY)) return false;
        state->setNodePosition(Vector2(posX, posY));

        if (!reader.expect("BLEND")) return false;
        bool hasBlendTree = false;
        if (!reader.readBool(hasBlendTree)) return false;
        if (hasBlendTree)
        {
            if (!reader.expect("BLEND_TYPE")) return false;
            int blendType = 0;
            if (!reader.readInt(blendType)) return false;

            BlendTreeData& tree = state->createBlendTree(static_cast<BlendTreeType>(blendType));

            if (!reader.expect("BLEND_PARAM_X")) return false;
            if (!reader.readString(tree.parameterX)) return false;

            if (!reader.expect("BLEND_PARAM_Y")) return false;
            if (!reader.readString(tree.parameterY)) return false;

            if (!reader.expect("BLEND_CHILD_COUNT")) return false;
            int childCount = 0;
            if (!reader.readInt(childCount) || childCount < 0) return false;

            tree.children.clear();
            tree.children.reserve(static_cast<size_t>(childCount));
            for (int i = 0; i < childCount; ++i)
            {
                if (!reader.expect("CHILD")) return false;

                BlendTreeChild child;
                if (!reader.readInt(child.animationIndex)) return false;
                if (!reader.readFloat(child.threshold)) return false;
                if (!reader.readFloat(child.position.x)) return false;
                if (!reader.readFloat(child.position.y)) return false;
                if (!reader.readFloat(child.timeScale)) return false;

                tree.children.push_back(child);
            }
        }
        else
        {
            state->clearBlendTree();
        }

        if (!reader.expect("TRANSITION_COUNT")) return false;
        int transitionCount = 0;
        if (!reader.readInt(transitionCount) || transitionCount < 0) return false;

        auto& transitions = state->getTransitions();
        transitions.clear();
        transitions.reserve(static_cast<size_t>(transitionCount));
        for (int i = 0; i < transitionCount; ++i)
        {
            if (!reader.expect("TRANS")) return false;

            AnimationTransition t;
            if (!reader.readString(t.destStateName)) return false;
            if (!reader.readFloat(t.fadeDuration)) return false;
            if (!reader.readFloat(t.exitTime)) return false;
            if (!reader.readBool(t.hasExitTime)) return false;
            if (!reader.readBool(t.interruptible)) return false;

            int conditionCount = 0;
            if (!reader.readInt(conditionCount) || conditionCount < 0) return false;

            t.conditions.reserve(static_cast<size_t>(conditionCount));
            for (int c = 0; c < conditionCount; ++c)
            {
                if (!reader.expect("COND")) return false;

                TransitionCondition cond;
                int op = 0;
                if (!reader.readString(cond.paramName)) return false;
                if (!reader.readInt(op)) return false;
                if (!reader.readFloat(cond.threshold)) return false;
                cond.op = static_cast<CompareOp>(op);
                t.conditions.push_back(cond);
            }

            transitions.push_back(t);
        }

        if (!reader.expect("EVENT_COUNT")) return false;
        int eventCount = 0;
        if (!reader.readInt(eventCount) || eventCount < 0) return false;

        auto& events = state->getEvents();
        events.clear();
        events.reserve(static_cast<size_t>(eventCount));
        for (int i = 0; i < eventCount; ++i)
        {
            if (!reader.expect("EVENT")) return false;

            AnimationEvent evt;
            if (!reader.readString(evt.name)) return false;
            if (!reader.readFloat(evt.normalizedTime)) return false;
            evt.callback = nullptr;
            evt.fired = false;
            events.push_back(std::move(evt));
        }

        if (!reader.expect("END_STATE")) return false;
        return true;
    }
}

namespace AnimatorControllerAsset
{
    bool save(const std::filesystem::path& filePath, const AnimationStateMachine& stateMachine)
    {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            LOG_ERROR("[AnimatorControllerAsset] Failed to open for write: %s", filePath.string().c_str());
            return false;
        }

        out << "ANIMCTRL " << kControllerFileVersion << "\n";
        out << "DEFAULT " << std::quoted(stateMachine.getDefaultStateName()) << "\n";

        const auto& parameters = stateMachine.getParameters();
        out << "PARAM_COUNT " << parameters.size() << "\n";

        std::vector<std::string> parameterNames;
        parameterNames.reserve(parameters.size());
        for (const auto& [name, _] : parameters)
        {
            parameterNames.push_back(name);
        }
        std::sort(parameterNames.begin(), parameterNames.end());

        for (const auto& paramName : parameterNames)
        {
            const auto it = parameters.find(paramName);
            if (it == parameters.end()) continue;

            const AnimationParameter& param = it->second;
            out << "PARAM "
                << std::quoted(param.name) << " "
                << static_cast<int>(param.type) << " "
                << param.floatValue << " "
                << param.intValue << " "
                << (param.boolValue ? 1 : 0) << "\n";
        }

        const auto& states = stateMachine.getStates();
        out << "STATE_COUNT " << states.size() << "\n";
        for (const auto& state : states)
        {
            if (!state) continue;
            if (!writeState(out, *state))
            {
                LOG_ERROR("[AnimatorControllerAsset] Failed while writing state data: %s", filePath.string().c_str());
                return false;
            }
        }

        LOG_INFO("[AnimatorControllerAsset] Saved: %s", filePath.string().c_str());
        return true;
    }

    bool load(const std::filesystem::path& filePath, AnimationStateMachine& stateMachine)
    {
        std::ifstream in(filePath, std::ios::binary);
        if (!in)
        {
            LOG_ERROR("[AnimatorControllerAsset] Failed to open for read: %s", filePath.string().c_str());
            return false;
        }

        TokenReader reader(in);

        if (!reader.expect("ANIMCTRL"))
        {
            LOG_ERROR("[AnimatorControllerAsset] Invalid header: %s", filePath.string().c_str());
            return false;
        }

        int version = 0;
        if (!reader.readInt(version) || version != kControllerFileVersion)
        {
            LOG_ERROR("[AnimatorControllerAsset] Unsupported version (%d): %s", version, filePath.string().c_str());
            return false;
        }

        if (!reader.expect("DEFAULT"))
        {
            LOG_ERROR("[AnimatorControllerAsset] Missing DEFAULT section: %s", filePath.string().c_str());
            return false;
        }

        std::string defaultStateName;
        if (!reader.readString(defaultStateName))
        {
            LOG_ERROR("[AnimatorControllerAsset] Failed to read default state: %s", filePath.string().c_str());
            return false;
        }

        if (!reader.expect("PARAM_COUNT"))
        {
            LOG_ERROR("[AnimatorControllerAsset] Missing PARAM_COUNT section: %s", filePath.string().c_str());
            return false;
        }

        int parameterCount = 0;
        if (!reader.readInt(parameterCount) || parameterCount < 0)
        {
            LOG_ERROR("[AnimatorControllerAsset] Invalid parameter count: %s", filePath.string().c_str());
            return false;
        }

        stateMachine.clearController();

        for (int i = 0; i < parameterCount; ++i)
        {
            if (!reader.expect("PARAM"))
            {
                LOG_ERROR("[AnimatorControllerAsset] Broken parameter block: %s", filePath.string().c_str());
                return false;
            }

            AnimationParameter param;
            int type = 0;
            if (!reader.readString(param.name)) return false;
            if (!reader.readInt(type)) return false;
            if (!reader.readFloat(param.floatValue)) return false;
            if (!reader.readInt(param.intValue)) return false;
            if (!reader.readBool(param.boolValue)) return false;
            param.type = static_cast<AnimParamType>(type);
            param.triggerFired = false;

            stateMachine.addParameter(param.name, param.type);
            auto& params = stateMachine.getParameters();
            auto it = params.find(param.name);
            if (it != params.end())
            {
                it->second.floatValue = param.floatValue;
                it->second.intValue = param.intValue;
                it->second.boolValue = param.boolValue;
                it->second.triggerFired = false;
            }
        }

        if (!reader.expect("STATE_COUNT"))
        {
            LOG_ERROR("[AnimatorControllerAsset] Missing STATE_COUNT section: %s", filePath.string().c_str());
            return false;
        }

        int stateCount = 0;
        if (!reader.readInt(stateCount) || stateCount < 0)
        {
            LOG_ERROR("[AnimatorControllerAsset] Invalid state count: %s", filePath.string().c_str());
            return false;
        }

        for (int i = 0; i < stateCount; ++i)
        {
            if (!reader.expect("STATE"))
            {
                LOG_ERROR("[AnimatorControllerAsset] Broken state block: %s", filePath.string().c_str());
                return false;
            }

            if (!readState(reader, stateMachine))
            {
                LOG_ERROR("[AnimatorControllerAsset] Failed to parse state: %s", filePath.string().c_str());
                return false;
            }
        }

        if (!defaultStateName.empty() && stateMachine.findState(defaultStateName))
        {
            stateMachine.setDefaultState(defaultStateName);
            stateMachine.forceTransition(defaultStateName, 0.0f);
        }
        else if (!stateMachine.getStates().empty())
        {
            const std::string& firstName = stateMachine.getStates().front()->getName();
            stateMachine.setDefaultState(firstName);
            stateMachine.forceTransition(firstName, 0.0f);
        }

        LOG_INFO("[AnimatorControllerAsset] Loaded: %s", filePath.string().c_str());
        return true;
    }
}
