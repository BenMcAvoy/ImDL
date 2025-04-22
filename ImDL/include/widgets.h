#pragma once

#include "pch.h"

namespace ImGui {
    enum class ToastLevel {
        Debug,  // New debug level
        Info,
        Warning,
        Error
    };

    class ToastSystem {
    public:
        class Toast {
        public:
            Toast(std::string message, ToastLevel level) : message(message), level(level) {}
            std::string message;
            ToastLevel level;

            // Animation variables
            float alpha = 1.0f;
            float fadeSpeed = 0.75f; // Reduced fade speed for better visibility
            float displayTime = 3.0f; // Time to display the toast
            float startTime = 0.0f; // Time when the toast was created
            bool isFading = false; // Whether the toast is currently fading out
            bool isVisible = true; // Whether the toast is currently visible
        };

    private:
        static std::vector<Toast> toastQueue;
        static std::mutex toastMutex;  // Add mutex for thread safety

    public:
        // Create a new toast notification (now thread-safe)
        static void Show(const std::string& message, ToastLevel level) {
            std::lock_guard<std::mutex> Lock(toastMutex);  // Lock during queue modification

            toastQueue.emplace_back(message, level);
            auto& toast = toastQueue.back();
            toast.startTime = (float)ImGui::GetTime();
            toast.isFading = false;
            toast.isVisible = true;
            toast.alpha = 1.0f; // Ensure full opacity to start
            toast.fadeSpeed = 0.75f;
            toast.displayTime = 3.0f;
        }

        // Helper to calculate wrapped text height
        static float CalculateTextHeight(const std::string& text, float wrapWidth) {
            ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
            return textSize.y;
        }

        // Render all active toasts (now thread-safe)
        static void RenderAll() {
            // Create a local copy of the toast queue to minimize lock time
            std::vector<Toast> localToastQueue;
            {
                std::lock_guard<std::mutex> Lock(toastMutex);
                localToastQueue = toastQueue;  // Make a copy to work with
            }

            const float screenWidth = ImGui::GetIO().DisplaySize.x;
            float yOffset = ImGui::GetIO().DisplaySize.y - 20.0f;  // Start from bottom
            const float padding = 10.0f;
            const float margin = 10.0f;
            const float toastWidth = 300.0f;  // Fixed width for toasts
            const float iconWidth = 20.0f;  // Space for icon

            // Save original ImGui state
            ImGuiStyle& style = ImGui::GetStyle();
            float originalAlpha = style.Alpha;

            // Process toasts in reverse order to display newest at the bottom
            std::vector<Toast> updatedToasts;
            updatedToasts.reserve(localToastQueue.size());

            for (auto it = localToastQueue.rbegin(); it != localToastQueue.rend(); ++it) {
                auto toast = *it;  // Work with a copy
                float currentTime = (float)ImGui::GetTime();
                float elapsedTime = currentTime - toast.startTime;

                // Check if toast should start fading
                if (elapsedTime > toast.displayTime && !toast.isFading) {
                    toast.isFading = true;
                }

                // Handle fading
                if (toast.isFading) {
                    toast.alpha -= toast.fadeSpeed * ImGui::GetIO().DeltaTime;
                    if (toast.alpha <= 0.0f) {
                        toast.isVisible = false;
                        continue;
                    }
                }

                // The toast is still visible, so add it to our updated list
                updatedToasts.push_back(toast);

                // Calculate available width for text (accounting for padding and icon)
                float textAreaWidth = toastWidth - (padding * 2.0f) - iconWidth;

                // Calculate wrapped text height
                float textHeight = CalculateTextHeight(toast.message, textAreaWidth);
                float toastHeight = std::max(textHeight, 24.0f) + (padding * 2.0f); // Ensure minimum height

                // Calculate position (right-aligned)
                ImVec2 position(screenWidth - toastWidth - margin, yOffset - toastHeight);

                // Set colors based on toast level - use ImGui's theme colors as a base
                ImVec4 bgColor;
                ImVec4 borderColor;
                ImVec4 textColor = style.Colors[ImGuiCol_Text];

                switch (toast.level) {
                case ToastLevel::Debug:
                    bgColor = ImVec4(0.1f, 0.1f, 0.3f, 1.0f);  // Dark blue
                    borderColor = ImVec4(0.2f, 0.2f, 0.5f, 1.0f);
                    break;
                case ToastLevel::Info:
                    bgColor = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
                    bgColor.w = 0.8f;
                    borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
                    textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    break;
                case ToastLevel::Warning:
                    bgColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
                    borderColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                    textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                    break;
                case ToastLevel::Error:
                    bgColor = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);
                    borderColor = ImVec4(0.7f, 0.2f, 0.2f, 1.0f);
                    textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    break;
                }

                // Draw standalone toast using ImDrawList directly
                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                // Calculate the area for the toast
                ImVec2 rectMin = position;
                ImVec2 rectMax(position.x + toastWidth, position.y + toastHeight);

                // We will directly manipulate colors and apply alpha ourselves
                ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(bgColor.x, bgColor.y, bgColor.z, bgColor.w * toast.alpha));
                ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(borderColor.x, borderColor.y, borderColor.z, borderColor.w * toast.alpha));
                ImU32 textColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(textColor.x, textColor.y, textColor.z, textColor.w * toast.alpha));

                // Draw background and border
                drawList->AddRectFilled(rectMin, rectMax, bgColorU32, 6.0f);
                drawList->AddRect(rectMin, rectMax, borderColorU32, 6.0f, 0, 2.0f);

                // Add icon based on toast level
                const char* icon = "";
                switch (toast.level) {
                using enum ToastLevel;
                case Debug:   icon = ICON_LC_CODE; break;
                case Info:    icon = ICON_LC_INFO; break;
                case Warning: icon = ICON_LC_OCTAGON_ALERT; break;
                case Error:   icon = ICON_LC_CIRCLE_X; break;
                }

                // Draw icon
                drawList->AddText(
                    ImVec2(rectMin.x + padding, rectMin.y + padding),
                    textColorU32,
                    icon
                );

                // Draw message text with proper wrapping
                float textX = rectMin.x + padding + iconWidth;
                float textY = rectMin.y + padding;

                // Store current clip rect
                ImVec4 clipRect = ImVec4(textX, textY, rectMax.x - padding, rectMax.y - padding);
                drawList->PushClipRect(
                    ImVec2(clipRect.x, clipRect.y),
                    ImVec2(clipRect.z, clipRect.w),
                    true
                );

                // Draw wrapped text
                ImGui::PushFont(ImGui::GetFont());
                drawList->AddText(
                    ImGui::GetFont(),
                    ImGui::GetFontSize(),
                    ImVec2(textX, textY),
                    textColorU32,
                    toast.message.c_str(),
                    NULL,
                    textAreaWidth
                );
                ImGui::PopFont();

                // Pop the clip rect
                drawList->PopClipRect();

                // Update y-offset for next toast
                yOffset -= (toastHeight + margin);
            }

            // Restore the original ImGui alpha
            style.Alpha = originalAlpha;

            // Update the toast queue with our processed toasts
            {
                std::lock_guard<std::mutex> Lock(toastMutex);
                // Reverse the order since we processed them backwards
                toastQueue.clear();
                for (auto it = updatedToasts.rbegin(); it != updatedToasts.rend(); ++it) {
                    toastQueue.push_back(*it);
                }
            }
        }
    };

    // Define static members
    std::vector<ToastSystem::Toast> ToastSystem::toastQueue;
    std::mutex ToastSystem::toastMutex;
} // namespace ImGui
