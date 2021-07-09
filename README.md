WinToast
===================
>WinToast is a lightly library written in C++ which brings a complete integration of the modern **toast notifications** of **Windows 8** &  **Windows 10**. 
>
>Toast notifications allows your app to inform the users about relevant information and timely events that they should see and take action upon inside your app, such as a new instant message, a new friend request, breaking news, or a calendar event. 

### This is a major rewrite and refactor of the original [WinToast](https://github.com/mohabouje/WinToast) with the following changes:
- No `using namespace` poisoning
- Cleaned up includes to speed up build time
- Changed `enum`s to `enum class`es
- Templates can now be value initialized
- Handlers no longer have to be made a virtual class with overridden methods; it's all lambdas and `std::function`
- Removed the required shortcut handling with AUMIs (**however, this is now up to you!**) A shortcut must be made to your application with its AUMI set to the one provided in the constructor. Failing to do so will cause callbacks to not be called.
- Improved code readability/complexity (no more huge nested if statements)
- Naming scheme now somewhat uses UE4 style (consider it EGL3 style)
- Removed the runtime DLL loading in favor of simply checking if the OS version is at least Windows 8 (or 10 for modern features)
- Switched to switch/case instead of using unordered maps and asserts for enum to string lookups
- Everything is now handled in `std::string` instead of `std::wstring` and changed to wide strings before being passed onto Windows's API
- A couple other bug fixes