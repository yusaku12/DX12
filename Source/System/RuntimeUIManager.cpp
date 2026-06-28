#include "pch.h"
#include "RuntimeUIManager.h"

#include "Component\CanvasComponent.h"
#include "Component\RectTransformComponent.h"
#include "Component\UIButtonComponent.h"
#include "Component\UITextComponent.h"

namespace
{
	ImVec4 toImVec4(const Vector4& value)
	{
		return ImVec4(value.x, value.y, value.z, value.w);
	}

	ImU32 toColorU32(const Vector4& value)
	{
		return ImGui::ColorConvertFloat4ToU32(toImVec4(value));
	}

	ImVec2 alignedTextPosition(const ImRect& rect, const ImVec2& textSize, UITextAlignment alignment)
	{
		constexpr float padding = 10.0f;

		ImVec2 pos = rect.Min;
		switch (alignment)
		{
		case UITextAlignment::TopLeft:
			pos = ImVec2(rect.Min.x + padding, rect.Min.y + padding);
			break;
		case UITextAlignment::TopCenter:
			pos = ImVec2(rect.GetCenter().x - textSize.x * 0.5f, rect.Min.y + padding);
			break;
		case UITextAlignment::TopRight:
			pos = ImVec2(rect.Max.x - textSize.x - padding, rect.Min.y + padding);
			break;
		case UITextAlignment::MiddleLeft:
			pos = ImVec2(rect.Min.x + padding, rect.GetCenter().y - textSize.y * 0.5f);
			break;
		case UITextAlignment::MiddleCenter:
			pos = ImVec2(rect.GetCenter().x - textSize.x * 0.5f, rect.GetCenter().y - textSize.y * 0.5f);
			break;
		case UITextAlignment::MiddleRight:
			pos = ImVec2(rect.Max.x - textSize.x - padding, rect.GetCenter().y - textSize.y * 0.5f);
			break;
		case UITextAlignment::BottomLeft:
			pos = ImVec2(rect.Min.x + padding, rect.Max.y - textSize.y - padding);
			break;
		case UITextAlignment::BottomCenter:
			pos = ImVec2(rect.GetCenter().x - textSize.x * 0.5f, rect.Max.y - textSize.y - padding);
			break;
		case UITextAlignment::BottomRight:
			pos = ImVec2(rect.Max.x - textSize.x - padding, rect.Max.y - textSize.y - padding);
			break;
		}

		return pos;
	}
}

void RuntimeUIManager::initialize()
{
	m_initialized = true;
	m_wantsMouseCapture = false;
	m_hoveredButtonId = 0;
	m_pressedButtonId = 0;
}

void RuntimeUIManager::shutdown()
{
	m_canvases.clear();
	m_initialized = false;
	m_wantsMouseCapture = false;
	m_hoveredButtonId = 0;
	m_pressedButtonId = 0;
}

void RuntimeUIManager::registerCanvas(CanvasComponent* canvas)
{
	if (!canvas)
	{
		return;
	}

	if (std::find(m_canvases.begin(), m_canvases.end(), canvas) == m_canvases.end())
	{
		m_canvases.push_back(canvas);
	}
}

void RuntimeUIManager::unregisterCanvas(CanvasComponent* canvas)
{
	m_canvases.erase(
		std::remove(m_canvases.begin(), m_canvases.end(), canvas),
		m_canvases.end());
}

void RuntimeUIManager::update()
{
	m_wantsMouseCapture = false;
	m_hoveredButtonId = 0;

	if (!m_initialized || m_canvases.empty())
	{
		if (!InputManager::Instance().isMouseHeld(0))
		{
			m_pressedButtonId = 0;
		}
		return;
	}

	if (!DX12::Instance().isSceneActive())
	{
		if (!InputManager::Instance().isMouseHeld(0))
		{
			m_pressedButtonId = 0;
		}
		return;
	}

	ImRect sceneRect;
	ImVec2 mousePosition;
	if (!tryGetSceneRect(sceneRect) || !tryGetMouseClientPosition(mousePosition) || !sceneRect.Contains(mousePosition))
	{
		if (!InputManager::Instance().isMouseHeld(0))
		{
			m_pressedButtonId = 0;
		}
		return;
	}

	std::vector<CanvasComponent*> canvases = m_canvases;
	std::sort(
		canvases.begin(),
		canvases.end(),
		[](const CanvasComponent* lhs, const CanvasComponent* rhs)
		{
			return lhs->getSortOrder() > rhs->getSortOrder();
		});

	ButtonHit hoveredHit{};
	for (CanvasComponent* canvas : canvases)
	{
		if (!canvas || !canvas->isActiveInHierarchy() || !canvas->receivesInput() || !canvas->gameObject())
		{
			continue;
		}

		if (findTopButtonRecursive(canvas->gameObject(), sceneRect, mousePosition, hoveredHit))
		{
			break;
		}
	}

	if (hoveredHit.button)
	{
		m_hoveredButtonId = hoveredHit.button->gameObject() ? hoveredHit.button->gameObject()->getInstanceId() : 0;
		m_wantsMouseCapture = hoveredHit.button->blocksMouseInput();
	}

	if (InputManager::Instance().isMousePressed(0) && hoveredHit.button)
	{
		m_pressedButtonId = m_hoveredButtonId;
	}

	if (InputManager::Instance().isMouseReleased(0))
	{
		if (hoveredHit.button && m_pressedButtonId != 0 && m_pressedButtonId == m_hoveredButtonId)
		{
			hoveredHit.button->invokeClick();
		}
		m_pressedButtonId = 0;
	}
	else if (m_pressedButtonId != 0)
	{
		m_wantsMouseCapture = true;
	}
}

void RuntimeUIManager::render()
{
	if (!m_initialized || m_canvases.empty())
	{
		return;
	}

	ImRect sceneRect;
	if (!tryGetSceneRect(sceneRect))
	{
		return;
	}

	ImDrawList* drawList = DX12::Instance().getSceneDrawList();
	if (!drawList)
	{
		return;
	}

	std::vector<CanvasComponent*> canvases = m_canvases;
	std::sort(
		canvases.begin(),
		canvases.end(),
		[](const CanvasComponent* lhs, const CanvasComponent* rhs)
		{
			return lhs->getSortOrder() < rhs->getSortOrder();
		});

	drawList->PushClipRect(sceneRect.Min, sceneRect.Max, true);
	for (CanvasComponent* canvas : canvases)
	{
		if (!canvas || !canvas->isActiveInHierarchy() || !canvas->gameObject())
		{
			continue;
		}

		drawCanvasRecursive(canvas->gameObject(), sceneRect, drawList);
	}
	drawList->PopClipRect();
}

bool RuntimeUIManager::tryGetSceneRect(ImRect& outRect) const
{
	const ImVec2 scenePos = DX12::Instance().getSceneWindowPos();
	const ImVec2 sceneSize = DX12::Instance().getSceneWindowSize();
	if (sceneSize.x <= 1.0f || sceneSize.y <= 1.0f)
	{
		return false;
	}

	outRect = ImRect(scenePos, ImVec2(scenePos.x + sceneSize.x, scenePos.y + sceneSize.y));
	return true;
}

bool RuntimeUIManager::tryGetMouseClientPosition(ImVec2& outPosition) const
{
	POINT cursor = InputManager::Instance().getMousePosition();
	ScreenToClient(DX12::Instance().getHwnd(), &cursor);
	outPosition = ImVec2(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
	return true;
}

ImRect RuntimeUIManager::resolveObjectRect(GameObject* object, const ImRect& parentRect) const
{
	if (!object)
	{
		return parentRect;
	}

	if (object->getComponent<CanvasComponent>())
	{
		return parentRect;
	}

	if (RectTransformComponent* rectTransform = object->getComponent<RectTransformComponent>())
	{
		return rectTransform->calculateRect(parentRect);
	}

	return parentRect;
}

bool RuntimeUIManager::findTopButtonRecursive(GameObject* object, const ImRect& parentRect, const ImVec2& point, ButtonHit& outHit) const
{
	if (!object || object->isDestroyed() || !object->isEnabled())
	{
		return false;
	}

	const ImRect currentRect = resolveObjectRect(object, parentRect);

	const auto& children = object->getChildren();
	for (auto it = children.rbegin(); it != children.rend(); ++it)
	{
		if (findTopButtonRecursive(*it, currentRect, point, outHit))
		{
			return true;
		}
	}

	UIButtonComponent* button = object->getComponent<UIButtonComponent>();
	if (!button || !button->isActiveInHierarchy() || !button->isInteractable())
	{
		return false;
	}

	if (!currentRect.Contains(point))
	{
		return false;
	}

	outHit.button = button;
	outHit.rect = currentRect;
	return true;
}

void RuntimeUIManager::drawCanvasRecursive(GameObject* object, const ImRect& parentRect, ImDrawList* drawList) const
{
	if (!object || !drawList || object->isDestroyed() || !object->isEnabled())
	{
		return;
	}

	const ImRect currentRect = resolveObjectRect(object, parentRect);

	if (UIButtonComponent* button = object->getComponent<UIButtonComponent>())
	{
		const bool hovered = object->getInstanceId() == m_hoveredButtonId;
		const bool pressed = object->getInstanceId() == m_pressedButtonId && hovered;
		const Vector4& fill = pressed
			? button->getPressedColor()
			: (hovered ? button->getHoverColor() : button->getNormalColor());

		drawList->AddRectFilled(currentRect.Min, currentRect.Max, toColorU32(fill), button->getCornerRounding());
		drawList->AddRect(currentRect.Min, currentRect.Max, IM_COL32(255, 255, 255, 38), button->getCornerRounding(), 0, 1.0f);

		const std::string& label = button->getLabel();
		if (!label.empty())
		{
			ImFont* font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize() * button->getFontScale();
			const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());
			const ImVec2 textPos = alignedTextPosition(currentRect, textSize, UITextAlignment::MiddleCenter);
			drawList->AddText(font, fontSize, textPos, toColorU32(button->getTextColor()), label.c_str());
		}
	}

	if (UITextComponent* text = object->getComponent<UITextComponent>())
	{
		const std::string& content = text->getText();
		if (!content.empty())
		{
			ImFont* font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize() * text->getFontScale();
			const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, content.c_str());
			const ImVec2 textPos = alignedTextPosition(currentRect, textSize, text->getAlignment());
			drawList->AddText(font, fontSize, textPos, toColorU32(text->getColor()), content.c_str());
		}
	}

	for (GameObject* child : object->getChildren())
	{
		drawCanvasRecursive(child, currentRect, drawList);
	}
}