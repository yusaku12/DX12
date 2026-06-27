#include "pch.h"
#include "EditorTransaction.h"

namespace EditorTransaction
{
    namespace
    {
        void runUndo(const Record& record)
        {
            for (auto it = record.steps.rbegin(); it != record.steps.rend(); ++it)
            {
                if (it->undo)
                {
                    it->undo();
                }
            }
        }

        void runRedo(const Record& record)
        {
            for (const Step& step : record.steps)
            {
                if (step.redo)
                {
                    step.redo();
                }
            }
        }
    }

    void Manager::begin(const std::string& label)
    {
        if (m_isReplaying)
        {
            return;
        }

        if (m_hasCurrent)
        {
            commit();
        }

        m_current = {};
        m_current.label = label;
        m_hasCurrent = true;
    }

    void Manager::addStep(std::function<void()> undo, std::function<void()> redo)
    {
        if (m_isReplaying)
        {
            return;
        }

        if (!m_hasCurrent)
        {
            begin("Edit");
        }

        if (!undo || !redo)
        {
            return;
        }

        m_current.steps.push_back(Step{ std::move(undo), std::move(redo) });
    }

    void Manager::commit()
    {
        if (!m_hasCurrent)
        {
            return;
        }

        if (!m_current.steps.empty())
        {
            pushRecord(std::move(m_current));
        }

        m_current = {};
        m_hasCurrent = false;
    }

    void Manager::cancel()
    {
        if (!m_hasCurrent)
        {
            return;
        }

        m_current = {};
        m_hasCurrent = false;
    }

    void Manager::record(const std::string& label, std::function<void()> undo, std::function<void()> redo)
    {
        if (m_isReplaying || !undo || !redo)
        {
            return;
        }

        Record record;
        record.label = label;
        record.steps.push_back(Step{ std::move(undo), std::move(redo) });
        pushRecord(std::move(record));
    }

    bool Manager::undo()
    {
        if (m_isReplaying || m_undoStack.empty())
        {
            return false;
        }

        m_isReplaying = true;

        Record record = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        runUndo(record);
        m_redoStack.push_back(std::move(record));

        m_isReplaying = false;
        return true;
    }

    bool Manager::redo()
    {
        if (m_isReplaying || m_redoStack.empty())
        {
            return false;
        }

        m_isReplaying = true;

        Record record = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        runRedo(record);
        m_undoStack.push_back(std::move(record));

        m_isReplaying = false;
        return true;
    }

    const char* Manager::nextUndoLabel() const
    {
        if (m_undoStack.empty())
        {
            return "";
        }

        return m_undoStack.back().label.c_str();
    }

    const char* Manager::nextRedoLabel() const
    {
        if (m_redoStack.empty())
        {
            return "";
        }

        return m_redoStack.back().label.c_str();
    }

    void Manager::clear()
    {
        m_undoStack.clear();
        m_redoStack.clear();
        m_current = {};
        m_hasCurrent = false;
    }

    void Manager::pushRecord(Record&& record)
    {
        m_undoStack.push_back(std::move(record));
        m_redoStack.clear();
    }
}
