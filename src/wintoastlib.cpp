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

#include "wintoastlib.h"

#include <wrl/event.h>
#include <filesystem>
#include <iostream>
#include <VersionHelpers.h>
#include <Shobjidl.h>

#pragma comment(lib,"shlwapi")
#pragma comment(lib,"user32")
#pragma comment(lib,"WindowsApp")

namespace Detail {
    using namespace ABI::Windows::Data::Xml::Dom;
    using namespace ABI::Windows::Foundation;
    using namespace ABI::Windows::UI::Notifications;
    using namespace Microsoft::WRL;

    int64_t GetCurrentTicks()
    {
        FILETIME Now;
        GetSystemTimeAsFileTime(&Now);
        return ((((INT64)Now.dwHighDateTime) << 32) | Now.dwLowDateTime);
    }

    class DateTimeImpl : public IReference<DateTime> {
    public:
        DateTimeImpl(DateTime DateTime) :
            Impl(DateTime)
        {

        }

        DateTimeImpl(int64_t MillisecondsFromNow) :
            Impl{ GetCurrentTicks() + MillisecondsFromNow * 10000 }
        {
            
        }

        //~DateTimeImpl() = default;

    protected:
        HRESULT STDMETHODCALLTYPE get_Value(DateTime* DateTime) override
        {
            *DateTime = Impl;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) override
        {
            if (!ppvObject) {
                return E_POINTER;
            }
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IReference<DateTime>)) {
                *ppvObject = static_cast<IUnknown*>(static_cast<IReference<DateTime>*>(this));
                return S_OK;
            }
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            return 1;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return 2;
        }

        HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) override
        {
            return E_NOTIMPL;
        }

    private:
        DateTime Impl;
    };

    std::wstring ToWide(const std::string& String)
    {
        return std::filesystem::path(String).wstring();
    }

    PCWSTR AsWidePtr(HSTRING HString) {
        return WindowsGetStringRawBuffer(HString, nullptr);
    }

    class StringWrapper {
    public:
        StringWrapper(const std::wstring& String) noexcept
        {
            HRESULT HResult = WindowsCreateString(String.c_str(), String.size(), &HString);
            if (FAILED(HResult)) {
                RaiseException(STATUS_INVALID_PARAMETER, EXCEPTION_NONCONTINUABLE, 0, NULL);
            }
        }

        StringWrapper(PCWSTR String, size_t Size) noexcept :
            StringWrapper(std::wstring(String, Size))
        {

        }

        StringWrapper(const std::string& String) noexcept :
            StringWrapper(ToWide(String))
        {

        }

        ~StringWrapper()
        {
            WindowsDeleteString(HString);
        }

        operator HSTRING() const noexcept
        {
            return HString;
        }

    private:
        HSTRING HString;
    };

    HRESULT SetNodeStringValue(const std::string& String, ComPtr<IXmlNode>& Node, ComPtr<IXmlDocument>& Document) {
        ComPtr<IXmlText> TextNode;
        auto Result = Document->CreateTextNode(StringWrapper(String), &TextNode);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> StringNode;
        Result = TextNode.As(&StringNode);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> AppendedChild;
        return Node->AppendChild(StringNode.Get(), &AppendedChild);
    }

    HRESULT AddAttribute(ComPtr<IXmlDocument>& Document, const std::string& Name, ComPtr<IXmlNamedNodeMap>& AttributeMap) {
        ComPtr<IXmlAttribute> SrcAttribute;
        auto Result = Document->CreateAttribute(StringWrapper(Name), &SrcAttribute);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Node;
        Result = SrcAttribute.As(&Node);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> PreviousNode;
        return AttributeMap->SetNamedItem(Node.Get(), &PreviousNode);
    }

    HRESULT CreateElement(ComPtr<IXmlDocument>& Document, const std::string& RootNode, const std::string& ElementName, const std::vector<std::string>& AttributeNames) {
        ComPtr<IXmlNodeList> RootList;
        HRESULT Result = Document->GetElementsByTagName(StringWrapper(RootNode), &RootList);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Root;
        Result = RootList->Item(0, &Root);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlElement> Element;
        Result = Document->CreateElement(StringWrapper(ElementName), &Element);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> NodeTmp;
        Result = Element.As(&NodeTmp);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Node;
        Result = Root->AppendChild(NodeTmp.Get(), &Node);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNamedNodeMap> Attributes;
        Result = Node->get_Attributes(&Attributes);
        if (FAILED(Result)) {
            return Result;
        }

        for (const auto& it : AttributeNames) {
            Result = AddAttribute(Document, it, Attributes);
        }

        return Result;
    }

    HRESULT SetTextFieldHelper(ComPtr<IXmlDocument>& Document, const std::string& Text, UINT32 Idx) {
        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"text"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Node;
        Result = NodeList->Item(Idx, &Node);
        if (FAILED(Result)) {
            return Result;
        }

        return SetNodeStringValue(Text, Node, Document);
    }

    HRESULT SetAttributionTextFieldHelper(ComPtr<IXmlDocument>& Document, const std::string& Text) {
        CreateElement(Document, "binding", "text", { "placement" });
        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"text"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        UINT32 NodeListLength;
        Result = NodeList->get_Length(&NodeListLength);
        if (FAILED(Result)) {
            return Result;
        }

        for (UINT32 Idx = 0; Idx < NodeListLength; ++Idx) {
            ComPtr<IXmlNode> Node;
            Result = NodeList->Item(Idx, &Node);
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlNamedNodeMap> Attributes;
            Result = Node->get_Attributes(&Attributes);
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlNode> NodeEdited;
            Result = Attributes->GetNamedItem(StringWrapper(L"placement"), &NodeEdited);
            if (!NodeEdited || FAILED(Result)) {
                continue;
            }

            Result = SetNodeStringValue("attribution", NodeEdited, Document);
            if (SUCCEEDED(Result)) {
                return SetTextFieldHelper(Document, Text, Idx);
            }
        }

        return Result;
    }

    HRESULT AddActionHelper(ComPtr<IXmlDocument>& Document, const std::string& Content, const std::string& Arguments) {
        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"actions"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        UINT32 NodeListLength;
        Result = NodeList->get_Length(&NodeListLength);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> ActionsNode;
        if (NodeListLength != 0) {
            Result = NodeList->Item(0, &ActionsNode);
            if (FAILED(Result)) {
                return Result;
            }
        }
        else {
            Result = Document->GetElementsByTagName(StringWrapper(L"toast"), &NodeList);
            if (FAILED(Result)) {
                return Result;
            }

            Result = NodeList->get_Length(&NodeListLength);
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlNode> ToastNode;
            Result = NodeList->Item(0, &ToastNode);
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlElement> ToastElement;
            Result = ToastNode.As(&ToastElement);
            if (FAILED(Result)) {
                return Result;
            }

            Result = ToastElement->SetAttribute(StringWrapper(L"template"), StringWrapper(L"ToastGeneric"));
            if (FAILED(Result)) {
                return Result;
            }

            Result = ToastElement->SetAttribute(StringWrapper(L"duration"), StringWrapper(L"long"));
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlElement> ActionsElement;
            Result = Document->CreateElement(StringWrapper(L"actions"), &ActionsElement);
            if (FAILED(Result)) {
                return Result;
            }

            Result = ActionsElement.As(&ActionsNode);
            if (FAILED(Result)) {
                return Result;
            }

            ComPtr<IXmlNode> AppendedChild;
            Result = ToastNode->AppendChild(ActionsNode.Get(), &AppendedChild);
            if (FAILED(Result)) {
                return Result;
            }
        }

        ComPtr<IXmlElement> ActionElement;
        Result = Document->CreateElement(StringWrapper(L"action"), &ActionElement);
        if (FAILED(Result)) {
            return Result;
        }

        Result = ActionElement->SetAttribute(StringWrapper(L"content"), StringWrapper(Content));
        if (FAILED(Result)) {
            return Result;
        }

        Result = ActionElement->SetAttribute(StringWrapper(L"arguments"), StringWrapper(Arguments));
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> ActionNode;
        Result = ActionElement.As(&ActionNode);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> AppendedChild;
        return ActionsNode->AppendChild(ActionNode.Get(), &AppendedChild);
    }

    HRESULT SetAudioFieldHelper(ComPtr<IXmlDocument>& Document, const std::string& Path, WinToastLib::AudioOption Option)
    {
        {
            std::vector<std::string> Attributes;
            if (!Path.empty()) {
                Attributes.emplace_back("src");
            }
            if (Option != WinToastLib::AudioOption::Default) {
                Attributes.emplace_back(Option == WinToastLib::AudioOption::Silent ? "silent" : "loop");
            }
            CreateElement(Document, "toast", "audio", Attributes);
        }

        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"audio"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Node;
        Result = NodeList->Item(0, &Node);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNamedNodeMap> Attributes;
        Result = Node->get_Attributes(&Attributes);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> NodeEdited;
        if (!Path.empty()) {
            Result = Attributes->GetNamedItem(StringWrapper(L"src"), &NodeEdited);
            if (FAILED(Result)) {
                return Result;
            }

            Result = SetNodeStringValue(Path, NodeEdited, Document);
            if (FAILED(Result)) {
                return Result;
            }
        }

        if (Option != WinToastLib::AudioOption::Default) {
            Result = Attributes->GetNamedItem(StringWrapper(Option == WinToastLib::AudioOption::Silent ? L"silent" : L"loop"), &NodeEdited);
            if (FAILED(Result)) {
                return Result;
            }

            Result = SetNodeStringValue("true", NodeEdited, Document);
            if (FAILED(Result)) {
                return Result;
            }
        }

        return Result;
    }

    HRESULT AddDurationHelper(ComPtr<IXmlDocument>& Document, const std::string& Duration)
    {
        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"toast"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        UINT32 NodeListLength;
        Result = NodeList->get_Length(&NodeListLength);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> ToastNode;
        Result = NodeList->Item(0, &ToastNode);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlElement> ToastElement;
        Result = ToastNode.As(&ToastElement);
        if (FAILED(Result)) {
            return Result;
        }

        return ToastElement->SetAttribute(StringWrapper(L"duration"), StringWrapper(Duration));
    }

    HRESULT SetImageFieldHelper(ComPtr<IXmlDocument>& Document, const std::string& Path)
    {
        ComPtr<IXmlNodeList> NodeList;
        auto Result = Document->GetElementsByTagName(StringWrapper(L"image"), &NodeList);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> Node;
        Result = NodeList->Item(0, &Node);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNamedNodeMap> Attributes;
        Result = Node->get_Attributes(&Attributes);
        if (FAILED(Result)) {
            return Result;
        }

        ComPtr<IXmlNode> NodeEdited;
        Result = Attributes->GetNamedItem(StringWrapper(L"src"), &NodeEdited);
        if (FAILED(Result)) {
            return Result;
        }

        SetNodeStringValue(Path, NodeEdited, Document);

        return Result;
    }

    HRESULT SetEventHandlers(ComPtr<IToastNotification>& Notification, const WinToastLib::Handler& EventHandler) {
        EventRegistrationToken ActivatedToken;
        auto Result = Notification->add_Activated(
            Callback<Implements<RuntimeClassFlags<ClassicCom>, ITypedEventHandler<ToastNotification*, IInspectable*>>>(
                [EventHandler](IToastNotification* Notification, IInspectable* Inspectable)
                {
                    IToastActivatedEventArgs* ActivatedEventArgs;
                    auto Result = Inspectable->QueryInterface(&ActivatedEventArgs);
                    if (FAILED(Result)) {
                        return Result;
                    }

                    HSTRING ArgumentsHandle;
                    Result = ActivatedEventArgs->get_Arguments(&ArgumentsHandle);
                    if (FAILED(Result)) {
                        return Result;
                    }

                    PCWSTR Arguments = AsWidePtr(ArgumentsHandle);
                    EventHandler.OnClicked(Arguments && *Arguments ? wcstol(Arguments, nullptr, 10) : -1);
                    return S_OK;
                }
            ).Get(),
            &ActivatedToken
        );
        if (FAILED(Result)) {
            return Result;
        }

        EventRegistrationToken DismissedToken;
        Result = Notification->add_Dismissed(
            Callback<Implements<RuntimeClassFlags<ClassicCom>, ITypedEventHandler<ToastNotification*, ToastDismissedEventArgs*>>>(
                [EventHandler](IToastNotification* Notification, IToastDismissedEventArgs* DismissedEventArgs)
                {
                    ToastDismissalReason Reason;
                    auto Result = DismissedEventArgs->get_Reason(&Reason);
                    if (FAILED(Result)) {
                        return Result;
                    }


                    DateTime ExpirationTime{};
                    {
                        IReference<DateTime>* ExpirationTimeRef = nullptr;
                        Notification->get_ExpirationTime(&ExpirationTimeRef);
                        if (ExpirationTimeRef != nullptr) {
                            ExpirationTimeRef->get_Value(&ExpirationTime);
                        }
                    }

                    if (Reason == ToastDismissalReason_UserCanceled && ExpirationTime.UniversalTime && GetCurrentTicks() >= ExpirationTime.UniversalTime) {
                        Reason = ToastDismissalReason_TimedOut;
                    }
                    EventHandler.OnDismissed(static_cast<WinToastLib::DismissalReason>(Reason));
                    return S_OK;
                }
            ).Get(),
            &DismissedToken
        );
        if (FAILED(Result)) {
            return Result;
        }

        EventRegistrationToken FailedToken;
        Result = Notification->add_Failed(
            Callback<Implements<RuntimeClassFlags<ClassicCom>, ITypedEventHandler<ToastNotification*, ToastFailedEventArgs*>>>(
                [EventHandler](IToastNotification* Notification, IToastFailedEventArgs* FailedEventArgs)
                {
                    EventHandler.OnFailed();
                    return S_OK;
                }
            ).Get(),
            &FailedToken
        );
        if (FAILED(Result)) {
            return Result;
        }

        return Result;
    }
}

namespace WinToastLib {
    using namespace Windows::Foundation;

    std::string GetAudioSystemFilePath(AudioSystemFile File)
    {
        switch (File)
        {
        case AudioSystemFile::DefaultSound:
            return "ms-winsoundevent:Notification.Default";
        case AudioSystemFile::IM:
            return "ms-winsoundevent:Notification.IM";
        case AudioSystemFile::Mail:
            return "ms-winsoundevent:Notification.Mai";
        case AudioSystemFile::Reminder:
            return "ms-winsoundevent:Notification.Reminder";
        case AudioSystemFile::SMS:
            return "ms-winsoundevent:Notification.SMS";
        case AudioSystemFile::Alarm:
            return "ms-winsoundevent:Notification.Looping.Alarm";
        case AudioSystemFile::Alarm2:
            return "ms-winsoundevent:Notification.Looping.Alarm2";
        case AudioSystemFile::Alarm3:
            return "ms-winsoundevent:Notification.Looping.Alarm3";
        case AudioSystemFile::Alarm4:
            return "ms-winsoundevent:Notification.Looping.Alarm4";
        case AudioSystemFile::Alarm5:
            return "ms-winsoundevent:Notification.Looping.Alarm5";
        case AudioSystemFile::Alarm6:
            return "ms-winsoundevent:Notification.Looping.Alarm6";
        case AudioSystemFile::Alarm7:
            return "ms-winsoundevent:Notification.Looping.Alarm7";
        case AudioSystemFile::Alarm8:
            return "ms-winsoundevent:Notification.Looping.Alarm8";
        case AudioSystemFile::Alarm9:
            return "ms-winsoundevent:Notification.Looping.Alarm9";
        case AudioSystemFile::Alarm10:
            return "ms-winsoundevent:Notification.Looping.Alarm10";
        case AudioSystemFile::Call:
            return "ms-winsoundevent:Notification.Looping.Cal";
        case AudioSystemFile::Call1:
            return "ms-winsoundevent:Notification.Looping.Call1";
        case AudioSystemFile::Call2:
            return "ms-winsoundevent:Notification.Looping.Call2";
        case AudioSystemFile::Call3:
            return "ms-winsoundevent:Notification.Looping.Call3";
        case AudioSystemFile::Call4:
            return "ms-winsoundevent:Notification.Looping.Call4";
        case AudioSystemFile::Call5:
            return "ms-winsoundevent:Notification.Looping.Call5";
        case AudioSystemFile::Call6:
            return "ms-winsoundevent:Notification.Looping.Call6";
        case AudioSystemFile::Call7:
            return "ms-winsoundevent:Notification.Looping.Call7";
        case AudioSystemFile::Call8:
            return "ms-winsoundevent:Notification.Looping.Call8";
        case AudioSystemFile::Call9:
            return "ms-winsoundevent:Notification.Looping.Call9";
        case AudioSystemFile::Call10:
            return "ms-winsoundevent:Notification.Looping.Call10";
        default:
            return "";
        }
    }

    WinToast::WinToast(const std::string& Aumi) :
        Initialized(false),
        Coinitialized(false),
        Aumi(Aumi)
    {

    }

    WinToast::~WinToast()
    {
        if (Coinitialized) {
            CoUninitialize();
        }
    }

    bool WinToast::IsCompatible()
    {
        return IsWindows8OrGreater();
    }

    bool WinToast::SupportsModernFeatures()
    {
        return IsWindows10OrGreater();
    }

    Error WinToast::Initialize()
    {
        if (Initialized) {
            return Error::Success;
        }

        if (!IsCompatible()) {
            return Error::SystemNotSupported;
        }

        if (!Coinitialized) {
            auto Result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

            if (Result == CO_E_NOTINITIALIZED) {
                return Error::ComInitFailed;
            }
            Coinitialized = true;
        }

        if (FAILED(SetCurrentProcessExplicitAppUserModelID(Detail::ToWide(Aumi).c_str()))) {
            return Error::InvalidAppUserModelID;
        }

        Initialized = true;
        return Error::Success;
    }

    bool WinToast::IsInitialized() const
    {
        return Initialized;
    }

    Error WinToast::ShowToast(const Template& Toast, const Handler& Handler, int64_t* Id)
    {
        if (!IsInitialized()) {
            return Error::NotInitialized;
        }

        ComPtr<IToastNotificationManagerStatics> Manager;
        auto Result = GetActivationFactory(Detail::StringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager), &Manager);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        ComPtr<IToastNotifier> Notifier;
        Result = Manager->CreateToastNotifierWithId(Detail::StringWrapper(Aumi), &Notifier);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        ComPtr<IToastNotificationFactory> Factory;
        Result = GetActivationFactory(Detail::StringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotification), &Factory);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        ComPtr<IXmlDocument> Document;
        Result = Manager->GetTemplateContent(ToastTemplateType(Toast.Type), &Document);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        for (size_t Idx = 0; Idx < Toast.TextFields.size(); ++Idx) {
            Result = Detail::SetTextFieldHelper(Document, Toast.TextFields[Idx], Idx);

            if (FAILED(Result)) {
                return Error::ComError;
            }
        }

        if (SupportsModernFeatures()) {
            if (!Toast.AttributionText.empty()) {
                Result = Detail::SetAttributionTextFieldHelper(Document, Toast.AttributionText);

                if (FAILED(Result)) {
                    return Error::ComError;
                }
            }

            for (size_t Idx = 0; Idx < Toast.Actions.size(); ++Idx) {
                Result = Detail::AddActionHelper(Document, Toast.Actions[Idx], std::to_string(Idx));

                if (FAILED(Result)) {
                    return Error::ComError;
                }
            }

            if (!Toast.AudioPath.empty() || Toast.AudioOption != AudioOption::Default) {
                Result = Detail::SetAudioFieldHelper(Document, Toast.AudioPath, Toast.AudioOption);

                if (FAILED(Result)) {
                    return Error::ComError;
                }
            }

            if (Toast.Duration != Duration::System) {
                Result = Detail::AddDurationHelper(Document, Toast.Duration == Duration::Short ? "short" : "long");

                if (FAILED(Result)) {
                    return Error::ComError;
                }
            }
        }

        if (Toast.Type <= TemplateType::ImageAndText04) {
            Result = Detail::SetImageFieldHelper(Document, Toast.ImagePath);

            if (FAILED(Result)) {
                return Error::ComError;
            }
        }

        ComPtr<IToastNotification> Notification;
        Result = Factory->CreateToastNotification(Document.Get(), &Notification);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        if (Toast.Expiration != 0) {
            Detail::DateTimeImpl Expiration(Toast.Expiration);
            Result = Notification->put_ExpirationTime(&Expiration);
            if (FAILED(Result)) {
                return Error::ComError;
            }
        }

        Result = Detail::SetEventHandlers(Notification, Handler);
        if (FAILED(Result)) {
            return Error::InvalidHandler;
        }

        GUID Guid;
        Result = CoCreateGuid(&Guid);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        int64_t IdVal = Guid.Data1;

        Buffer.emplace(IdVal, Notification);

        Result = Notifier->Show(Notification.Get());
        if (FAILED(Result)) {
            return Error::NotDisplayed;
        }

        if (Id != nullptr) {
            *Id = IdVal;
        }

        return Error::Success;
    }

    Error WinToast::HideToast(int64_t Id)
    {
        if (!IsInitialized()) {
            return Error::NotInitialized;
        }

        auto Itr = Buffer.find(Id);
        if (Itr == Buffer.end()) {
            return Error::IdNotFound;
        }

        ComPtr<IToastNotificationManagerStatics> Manager;
        auto Result = GetActivationFactory(Detail::StringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager), &Manager);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        ComPtr<IToastNotifier> Notifier;
        Result = Manager->CreateToastNotifierWithId(Detail::StringWrapper(Aumi), &Notifier);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        Result = Notifier->Hide(Itr->second.Get());
        Buffer.erase(Itr);

        return FAILED(Result) ? Error::CouldNotHide : Error::Success;
    }

    Error WinToast::ClearToasts()
    {
        if (!IsInitialized()) {
            return Error::NotInitialized;
        }

        ComPtr<IToastNotificationManagerStatics> Manager;
        auto Result = GetActivationFactory(Detail::StringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager), &Manager);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        ComPtr<IToastNotifier> Notifier;
        Result = Manager->CreateToastNotifierWithId(Detail::StringWrapper(Aumi), &Notifier);
        if (FAILED(Result)) {
            return Error::ComError;
        }

        bool FailedOnce = false;
        for (auto& [Id, Notification] : Buffer) {
            Result = Notifier->Hide(Notification.Get());
            FailedOnce = FailedOnce || FAILED(Result);
        }
        Buffer.clear();

        return FailedOnce ? Error::CouldNotHide : Error::Success;
    }
}
