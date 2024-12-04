#include <iostream>
#include <DebugOutput.h>
#include <GraphicsAPI_OpenGL.h>
#include <OpenXRDebugUtils.h>

class OpenXRTutorial {
public:
	OpenXRTutorial(GraphicsAPI_Type apiType)
	{
	}
	~OpenXRTutorial() = default;
	void Run()
	{
		CreateInstance();
		CreateDebugMessenger();

		GetInstanceProperties();
		GetSystemID();

		DestroyDebugMessenger();
		DestroyInstance();
	}
private:
	void CreateInstance() {

	}
	void DestroyInstance() {

	}
	void CreateDebugMessenger() {

	}
	void DestroyDebugMessenger() {

	}
	void GetInstanceProperties() {

	}
	void GetSystemID() {

	}
	void PollSystemEvents()
	{
	}
private:
	XrInstance m_XRInstance = {};
	bool m_applicationRunning = true;
	bool m_sessionRunning = false;
};

void OpenXRTutorial_Main(GraphicsAPI_Type apiType) {
	DebugOutput debugOutput;  // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
	XR_TUT_LOG("OpenXR Tutorial Chapter 2");
	OpenXRTutorial app(apiType);
	app.Run();
}

int main()
{
	OpenXRTutorial_Main(OPENGL);
	return 0;
}