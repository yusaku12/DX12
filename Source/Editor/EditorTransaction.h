#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace EditorTransaction
{
    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void undo() = 0;
        virtual void redo() = 0;
        virtual const char* label() const = 0;
    };

    struct Step
    {
        std::function<void()> undo;
        std::function<void()> redo;
    };

    struct Record
    {
        std::string label;
        std::vector<Step> steps;
    };

    class Manager
    {
    public:
        static Manager& Instance()
        {
            static Manager instance;
            return instance;
        }

        void begin(const std::string& label);
        void addStep(std::function<void()> undo, std::function<void()> redo);
        void commit();
        void cancel();

        void record(const std::string& label, std::function<void()> undo, std::function<void()> redo);

        bool undo();
        bool redo();

        bool canUndo() const { return !m_undoStack.empty(); }
        bool canRedo() const { return !m_redoStack.empty(); }

        const char* nextUndoLabel() const;
        const char* nextRedoLabel() const;

        void clear();
        bool isReplaying() const { return m_isReplaying; }

    private:
        Manager() = default;
        ~Manager() = default;

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void pushRecord(Record&& record);
        void pushCommand(std::unique_ptr<ICommand> command);

        static constexpr size_t k_maxHistory = 256;

        std::vector<std::unique_ptr<ICommand>> m_undoStack;
        std::vector<std::unique_ptr<ICommand>> m_redoStack;

        Record m_current;
        bool m_hasCurrent = false;
        bool m_isReplaying = false;
    };
}
