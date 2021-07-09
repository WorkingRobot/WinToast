/* * Copyright (C) 2016-2019 Mohammed Boujemaoui <mohabouje@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <functional>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl/implements.h>
#include <windows.ui.notifications.h>

namespace WinToastLib {
    using namespace ABI::Windows::Data::Xml::Dom;
    using namespace ABI::Windows::UI::Notifications;
    using namespace Microsoft::WRL;

    enum class Duration : uint8_t {
        System,
        Short,
        Long
    };

    enum class AudioOption : uint8_t {
        Default,
        Silent,
        Loop
    };

    enum class AudioSystemFile : uint8_t {
        DefaultSound,
        IM,
        Mail,
        Reminder,
        SMS,
        Alarm,
        Alarm2,
        Alarm3,
        Alarm4,
        Alarm5,
        Alarm6,
        Alarm7,
        Alarm8,
        Alarm9,
        Alarm10,
        Call,
        Call1,
        Call2,
        Call3,
        Call4,
        Call5,
        Call6,
        Call7,
        Call8,
        Call9,
        Call10
    };

    enum class TemplateType : int {
        // 1 text field
        ImageAndText01 = ToastTemplateType::ToastTemplateType_ToastImageAndText01,
        Text01 = ToastTemplateType::ToastTemplateType_ToastText01,
        // 2 text fields
        ImageAndText02 = ToastTemplateType::ToastTemplateType_ToastImageAndText02,
        Text02 = ToastTemplateType::ToastTemplateType_ToastText02,
        ImageAndText03 = ToastTemplateType::ToastTemplateType_ToastImageAndText03,
        Text03 = ToastTemplateType::ToastTemplateType_ToastText03,
        // 3 text fields
        ImageAndText04 = ToastTemplateType::ToastTemplateType_ToastImageAndText04,
        Text04 = ToastTemplateType::ToastTemplateType_ToastText04
    };

    enum class DismissalReason : int {
        UserCanceled = ToastDismissalReason::ToastDismissalReason_UserCanceled,
        ApplicationHidden = ToastDismissalReason::ToastDismissalReason_ApplicationHidden,
        TimedOut = ToastDismissalReason::ToastDismissalReason_TimedOut
    };

    enum class Error : uint8_t {
        Success,
        SystemNotSupported,
        ComInitFailed,
        InvalidAppUserModelID,
        NotInitialized,
        ComError,
        InvalidHandler,
        NotDisplayed,
        IdNotFound,
        CouldNotHide,
    };

    struct Template {
        TemplateType Type = TemplateType::Text01;
        std::vector<std::string> TextFields;
        std::vector<std::string> Actions;
        std::string ImagePath;
        std::string AudioPath;
        std::string AttributionText;
        int64_t Expiration = 0;
        AudioOption AudioOption = AudioOption::Default;
        Duration Duration = Duration::System;
    };

    struct Handler {
        std::function<void(int ActionIdx)> OnClicked = [](int) {};
        std::function<void(DismissalReason Reason)> OnDismissed = [](DismissalReason) {};
        std::function<void()> OnFailed = []() {};
    };

    std::string GetAudioSystemFilePath(AudioSystemFile File);

    class WinToast {
    public:
        WinToast(const std::string& Aumi);
        ~WinToast();

        static bool IsCompatible();
        static bool SupportsModernFeatures();

        Error Initialize();
        bool IsInitialized() const;

        Error ShowToast(const Template& Toast, const Handler& Handler, int64_t* Id = nullptr);
        Error HideToast(int64_t Id);
        Error ClearToasts();

    protected:
        bool Initialized;
        bool Coinitialized;
        std::string Aumi;
        std::unordered_map<int64_t, ComPtr<IToastNotification>> Buffer;
    };
}
