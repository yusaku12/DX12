#include "pch.h"
#include "EditorTransaction.h"

namespace EditorTransaction
{
    namespace
    {
        class CompositeCommand final : public ICommand
        {
        public:
            explicit CompositeCommand(Record&& record)
                : m_record(std::move(record))
            {
            }

            void undo() override
            {
                for (auto it = m_record.steps.rbegin(); it != m_record.steps.rend(); ++it)
                {
                    if (it->undo)
                    {
                        it->undo();
                    }
                }
            }

            void redo() override
            {
                for (const Step& step : m_record.steps)
                {
                    if (step.redo)
                    {
                        step.redo();
                    }
                }
            }

            const char* label() const override
            {
                return m_record.label.c_str();
            }

        private:
            Record m_record;
        };

        class ReplayGuard final
        {
        public:
            explicit ReplayGuard(bool& flag)
                : m_flag(flag)
            {
                m_flag = true;
            }

            ~ReplayGuard()
            {
                m_flag = false;
            }

        private:
            bool& m_flag;
        };
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
        if (m_isReplaying)
        {
            return;
        }

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
        if (m_isReplaying)
        {
            return;
        }

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
        if (m_isReplaying)
        {
            return false;
        }

        if (m_hasCurrent)
        {
            commit();
        }

        if (m_undoStack.empty())
        {
            return false;
        }

        ReplayGuard guard(m_isReplaying);

        std::unique_ptr<ICommand> command = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        command->undo();
        m_redoStack.push_back(std::move(command));

        return true;
    }

    bool Manager::redo()
    {
        if (m_isReplaying || m_redoStack.empty())
        {
            return false;
        }

        ReplayGuard guard(m_isReplaying);

        std::unique_ptr<ICommand> command = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        command->redo();
        m_undoStack.push_back(std::move(command));

        return true;
    }

    const char* Manager::nextUndoLabel() const
    {
        if (m_undoStack.empty())
        {
            return "";
        }

        return m_undoStack.back()->label();
    }

    const char* Manager::nextRedoLabel() const
    {
        if (m_redoStack.empty())
        {
            return "";
        }

        return m_redoStack.back()->label();
    }

    void Manager::clear()
    {
        m_undoStack.clear();
        m_redoStack.clear();
        m_current = {};
        m_hasCurrent = false;
        m_isReplaying = false;
    }

    void Manager::pushRecord(Record&& record)
    {
        if (record.steps.empty())
        {
            return;
        }

        pushCommand(DXMem::makeUnique<CompositeCommand>(std::move(record)));
    }

    void Manager::pushCommand(std::unique_ptr<ICommand> command)
    {
        if (!command)
        {
            return;
        }

        if (m_undoStack.size() >= k_maxHistory)
        {
            m_undoStack.erase(m_undoStack.begin());
        }

        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
    }
}
