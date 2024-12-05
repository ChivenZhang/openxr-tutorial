// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#include <DebugOutput.h>
//#include <GraphicsAPI_D3D11.h>
//#include <GraphicsAPI_D3D12.h>
#include <GraphicsAPI_OpenGL.h>
//#include <GraphicsAPI_OpenGL_ES.h>
//#include <GraphicsAPI_Vulkan.h>
#include <OpenXRDebugUtils.h>
#include <memory>

#define XR_DOCS_CHAPTER_VERSION XR_DOCS_CHAPTER_2_3

class OpenXRTutorial
{
public:
	OpenXRTutorial(GraphicsAPI_Type apiType)
		: m_APIType(apiType) {
		// Check API compatibility with Platform.
		if (!CheckGraphicsAPI_TypeIsValidForPlatform(m_APIType)) {
			XR_TUT_LOG_ERROR("ERROR: The provided Graphics API is not valid for this platform.");
			DEBUG_BREAK;
		}
	}
	~OpenXRTutorial() = default;

	void Run()
	{
		CreateInstance();
		CreateDebugMessenger();

		GetInstanceProperties();
		GetSystemID();

		GetViewConfigurationViews();
		GetEnvironmentBlendModes();

		CreateSession();
		CreateReferenceSpace();
		CreateSwapchains();

		while (m_applicationRunning)
		{
			PollEvents();
			if (m_sessionRunning)
			{
				RenderFrame();
			}
		}

		DestroySwapchains();
		DestroyReferenceSpace();
		DestroySession();

		DestroyDebugMessenger();
		DestroyInstance();
	}

private:
	void CreateInstance()
	{
		// Add additional instance layers/extensions that the application wants.
		// Add both required and requested instance extensions.
		{
			m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
			// Ensure m_APIType is already defined when we call this line.
			m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_APIType));
		}

		// Get all the API Layers from the OpenXR runtime.
		uint32_t apiLayerCount = 0;
		std::vector<XrApiLayerProperties> apiLayerProperties;
		OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
		apiLayerProperties.resize(apiLayerCount, { XR_TYPE_API_LAYER_PROPERTIES });
		OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

		// Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
		for (auto& requestLayer : m_apiLayers) {
			for (auto& layerProperty : apiLayerProperties) {
				// strcmp returns 0 if the strings match.
				if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
					continue;
				}
				else {
					m_activeAPILayers.push_back(requestLayer.c_str());
					break;
				}
			}
		}

		// Get all the Instance Extensions from the OpenXR instance.
		uint32_t extensionCount = 0;
		std::vector<XrExtensionProperties> extensionProperties;
		OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
		extensionProperties.resize(extensionCount, { XR_TYPE_EXTENSION_PROPERTIES });
		OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

		// Check the requested Instance Extensions against the ones from the OpenXR runtime.
		// If an extension is found add it to Active Instance Extensions.
		// Log error if the Instance Extension is not found.
		for (auto& requestedInstanceExtension : m_instanceExtensions) {
			bool found = false;
			for (auto& extensionProperty : extensionProperties) {
				// strcmp returns 0 if the strings match.
				if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
					continue;
				}
				else {
					m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
					found = true;
					break;
				}
			}
			if (!found) {
				XR_TUT_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
			}
		}

		// Fill out an XrInstanceCreateInfo structure and create an XrInstance.
		XrInstanceCreateInfo instanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
		strncpy(instanceCreateInfo.applicationInfo.applicationName, "OpenXR Tutorial", XR_MAX_APPLICATION_NAME_SIZE);
		instanceCreateInfo.applicationInfo.applicationVersion = 1;
		instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
		instanceCreateInfo.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
		instanceCreateInfo.enabledApiLayerNames = m_activeAPILayers.data();
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
		instanceCreateInfo.enabledExtensionNames = m_activeInstanceExtensions.data();
		OPENXR_CHECK(xrCreateInstance(&instanceCreateInfo, &m_xrInstance), "Failed to create Instance.");
	}

	void DestroyInstance()
	{
		// Destroy the XrInstance.
		OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
	}

	void CreateDebugMessenger()
	{
		// Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
		if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
			m_DebugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
		}
	}

	void DestroyDebugMessenger()
	{
		// Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
		if (m_DebugUtilsMessenger != XR_NULL_HANDLE)
		{
			DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_DebugUtilsMessenger);  // From OpenXRDebugUtils.h.
		}
	}

	void GetInstanceProperties()
	{
		// Get the instance's properties and log the runtime name and version.
		XrInstanceProperties instanceProperties{ XR_TYPE_INSTANCE_PROPERTIES };
		OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

		XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
			<< XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
			<< XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
			<< XR_VERSION_PATCH(instanceProperties.runtimeVersion));
	}

	void GetSystemID()
	{
		// Get the XrSystemId from the instance and the supplied XrFormFactor.
		XrSystemGetInfo systemGI{ XR_TYPE_SYSTEM_GET_INFO };
		systemGI.formFactor = m_FormFactor;
		OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

		// Get the System's properties for some general information about the hardware and the vendor.
		OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
	}

	void CreateSession()
	{
		XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
		m_GraphicsAPI = std::make_unique<GraphicsAPI_OpenGL>(m_xrInstance, m_systemID);
		sessionCreateInfo.next = m_GraphicsAPI->GetGraphicsBinding();
		sessionCreateInfo.createFlags = 0;
		sessionCreateInfo.systemId = m_systemID;
		OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCreateInfo, &m_Session), "Failed to create Session.");
	}

	void DestroySession()
	{
		// Destroy the XrSession.
		OPENXR_CHECK(xrDestroySession(m_Session), "Failed to destroy Session.");
	}

	void PollEvents()
	{
		// Poll OpenXR for a new event.
		XrEventDataBuffer eventData{ XR_TYPE_EVENT_DATA_BUFFER };
		auto XrPollEvents = [&]() -> bool {
			eventData = { XR_TYPE_EVENT_DATA_BUFFER };
			return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
			};

		while (XrPollEvents())
		{
			switch (eventData.type)
			{
				// Log the number of lost events from the runtime.
			case XR_TYPE_EVENT_DATA_EVENTS_LOST:
			{
				XrEventDataEventsLost* eventsLost = reinterpret_cast<XrEventDataEventsLost*>(&eventData);
				XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
				break;
			}
			// Log that an instance loss is pending and shutdown the application.
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			{
				XrEventDataInstanceLossPending* instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending*>(&eventData);
				XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
				m_sessionRunning = false;
				m_applicationRunning = false;
				break;
			}
			// Log that the interaction profile has changed.
			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
			{
				XrEventDataInteractionProfileChanged* interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged*>(&eventData);
				XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
				if (interactionProfileChanged->session != m_Session) {
					XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
					break;
				}
				break;
			}
			// Log that there's a reference space change pending.
			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			{
				XrEventDataReferenceSpaceChangePending* referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&eventData);
				XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
				if (referenceSpaceChangePending->session != m_Session) {
					XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
					break;
				}
				break;
			}
			// Session State changes:
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				XrEventDataSessionStateChanged* sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
				if (sessionStateChanged->session != m_Session)
				{
					XR_TUT_LOG("XrEventDataSessionStateChanged for unknown Session");
					break;
				}

				if (sessionStateChanged->state == XR_SESSION_STATE_READY)
				{
					// SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
					XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
					sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
					OPENXR_CHECK(xrBeginSession(m_Session, &sessionBeginInfo), "Failed to begin Session.");
					m_sessionRunning = true;
				}
				if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING)
				{
					// SessionState is stopping. End the XrSession.
					OPENXR_CHECK(xrEndSession(m_Session), "Failed to end Session.");
					m_sessionRunning = false;
				}
				if (sessionStateChanged->state == XR_SESSION_STATE_EXITING)
				{
					// SessionState is exiting. Exit the application.
					m_sessionRunning = false;
					m_applicationRunning = false;
				}
				if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING)
				{
					// SessionState is loss pending. Exit the application.
					// It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
					m_sessionRunning = false;
					m_applicationRunning = false;
				}
				if (sessionStateChanged->state == XR_SESSION_STATE_SYNCHRONIZED)
				{

				}
				// Store state for reference across the application.
				m_SessionState = sessionStateChanged->state;
				break;
			}
			}
		}
	}

	void GetViewConfigurationViews()
	{
		// Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
		uint32_t viewConfigurationCount = 0;
		OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, 0, &viewConfigurationCount, nullptr), "Failed to enumerate View Configurations.");
		m_viewConfigurations.resize(viewConfigurationCount);
		OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, viewConfigurationCount, &viewConfigurationCount, m_viewConfigurations.data()), "Failed to enumerate View Configurations.");

		// Pick the first application supported View Configuration Type con supported by the hardware.
		for (const XrViewConfigurationType& viewConfiguration : m_applicationViewConfigurations) {
			if (std::find(m_viewConfigurations.begin(), m_viewConfigurations.end(), viewConfiguration) != m_viewConfigurations.end()) {
				m_viewConfiguration = viewConfiguration;
				break;
			}
		}
		if (m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM) {
			std::cerr << "Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO." << std::endl;
			m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		}

		// Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills out the array.
		uint32_t viewConfigurationViewCount = 0;
		OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, 0, &viewConfigurationViewCount, nullptr), "Failed to enumerate ViewConfiguration Views.");
		m_viewConfigurationViews.resize(viewConfigurationViewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
		OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, viewConfigurationViewCount, &viewConfigurationViewCount, m_viewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
	}

	void CreateSwapchains()
	{
		// Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
		uint32_t formatCount = 0;
		OPENXR_CHECK(xrEnumerateSwapchainFormats(m_Session, 0, &formatCount, nullptr), "Failed to enumerate Swapchain Formats");
		std::vector<int64_t> formats(formatCount);
		OPENXR_CHECK(xrEnumerateSwapchainFormats(m_Session, formatCount, &formatCount, formats.data()), "Failed to enumerate Swapchain Formats");
		if (m_GraphicsAPI->SelectDepthSwapchainFormat(formats) == 0)
		{
			std::cerr << "Failed to find depth format for Swapchain." << std::endl;
			DEBUG_BREAK;
		}

		//Resize the SwapchainInfo to match the number of view in the View Configuration.
		m_colorSwapchainInfos.resize(m_viewConfigurationViews.size());
		m_depthSwapchainInfos.resize(m_viewConfigurationViews.size());
		for (size_t i = 0; i < m_viewConfigurationViews.size(); i++)
		{
			SwapchainInfo& colorSwapchainInfo = m_colorSwapchainInfos[i];
			SwapchainInfo& depthSwapchainInfo = m_depthSwapchainInfos[i];

			// Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
			// Color.
			XrSwapchainCreateInfo swapchainCI{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
			swapchainCI.createFlags = 0;
			swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
			swapchainCI.format = m_GraphicsAPI->SelectColorSwapchainFormat(formats);                // Use GraphicsAPI to select the first compatible format.
			swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
			swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
			swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
			swapchainCI.faceCount = 1;
			swapchainCI.arraySize = 1;
			swapchainCI.mipCount = 1;
			OPENXR_CHECK(xrCreateSwapchain(m_Session, &swapchainCI, &colorSwapchainInfo.swapchain), "Failed to create Color Swapchain");
			colorSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

			// Depth.
			swapchainCI.createFlags = 0;
			swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			swapchainCI.format = m_GraphicsAPI->SelectDepthSwapchainFormat(formats);                // Use GraphicsAPI to select the first compatible format.
			swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
			swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
			swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
			swapchainCI.faceCount = 1;
			swapchainCI.arraySize = 1;
			swapchainCI.mipCount = 1;
			OPENXR_CHECK(xrCreateSwapchain(m_Session, &swapchainCI, &depthSwapchainInfo.swapchain), "Failed to create Depth Swapchain");
			depthSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

			// Get the number of images in the color/depth swapchain and allocate Swapchain image data via GraphicsAPI to store the returned array.
			uint32_t colorSwapchainImageCount = 0;
			OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, 0, &colorSwapchainImageCount, nullptr), "Failed to enumerate Color Swapchain Images.");
			XrSwapchainImageBaseHeader* colorSwapchainImages = m_GraphicsAPI->AllocateSwapchainImageData(colorSwapchainInfo.swapchain, GraphicsAPI::SwapchainType::COLOR, colorSwapchainImageCount);
			OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, colorSwapchainImageCount, &colorSwapchainImageCount, colorSwapchainImages), "Failed to enumerate Color Swapchain Images.");

			uint32_t depthSwapchainImageCount = 0;
			OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo.swapchain, 0, &depthSwapchainImageCount, nullptr), "Failed to enumerate Depth Swapchain Images.");
			XrSwapchainImageBaseHeader* depthSwapchainImages = m_GraphicsAPI->AllocateSwapchainImageData(depthSwapchainInfo.swapchain, GraphicsAPI::SwapchainType::DEPTH, depthSwapchainImageCount);
			OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo.swapchain, depthSwapchainImageCount, &depthSwapchainImageCount, depthSwapchainImages), "Failed to enumerate Depth Swapchain Images.");

			// Per image in the swapchains, fill out a GraphicsAPI::ImageViewCreateInfo structure and create a color/depth image view.
			for (uint32_t j = 0; j < colorSwapchainImageCount; j++) {
				GraphicsAPI::ImageViewCreateInfo imageViewCI;
				imageViewCI.image = m_GraphicsAPI->GetSwapchainImage(colorSwapchainInfo.swapchain, j);
				imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::RTV;
				imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D;
				imageViewCI.format = colorSwapchainInfo.swapchainFormat;
				imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::COLOR_BIT;
				imageViewCI.baseMipLevel = 0;
				imageViewCI.levelCount = 1;
				imageViewCI.baseArrayLayer = 0;
				imageViewCI.layerCount = 1;
				colorSwapchainInfo.imageViews.push_back(m_GraphicsAPI->CreateImageView(imageViewCI));
			}
			for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
				GraphicsAPI::ImageViewCreateInfo imageViewCI;
				imageViewCI.image = m_GraphicsAPI->GetSwapchainImage(depthSwapchainInfo.swapchain, j);
				imageViewCI.type = GraphicsAPI::ImageViewCreateInfo::Type::DSV;
				imageViewCI.view = GraphicsAPI::ImageViewCreateInfo::View::TYPE_2D;
				imageViewCI.format = depthSwapchainInfo.swapchainFormat;
				imageViewCI.aspect = GraphicsAPI::ImageViewCreateInfo::Aspect::DEPTH_BIT;
				imageViewCI.baseMipLevel = 0;
				imageViewCI.levelCount = 1;
				imageViewCI.baseArrayLayer = 0;
				imageViewCI.layerCount = 1;
				depthSwapchainInfo.imageViews.push_back(m_GraphicsAPI->CreateImageView(imageViewCI));
			}
		}
	}

	void DestroySwapchains()
	{
		// Per view in the view configuration:
		for (size_t i = 0; i < m_viewConfigurationViews.size(); i++)
		{
			SwapchainInfo& colorSwapchainInfo = m_colorSwapchainInfos[i];
			SwapchainInfo& depthSwapchainInfo = m_depthSwapchainInfos[i];

			// Destroy the color and depth image views from GraphicsAPI.
			for (void*& imageView : colorSwapchainInfo.imageViews)
			{
				m_GraphicsAPI->DestroyImageView(imageView);
			}
			for (void*& imageView : depthSwapchainInfo.imageViews)
			{
				m_GraphicsAPI->DestroyImageView(imageView);
			}

			// Free the Swapchain Image Data.
			m_GraphicsAPI->FreeSwapchainImageData(colorSwapchainInfo.swapchain);
			m_GraphicsAPI->FreeSwapchainImageData(depthSwapchainInfo.swapchain);

			// Destroy the swapchains.
			OPENXR_CHECK(xrDestroySwapchain(colorSwapchainInfo.swapchain), "Failed to destroy Color Swapchain");
			OPENXR_CHECK(xrDestroySwapchain(depthSwapchainInfo.swapchain), "Failed to destroy Depth Swapchain");
		}
	}

	void GetEnvironmentBlendModes()
	{
		// Retrieves the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
		uint32_t environmentBlendModeCount = 0;
		OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeCount, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
		m_environmentBlendModes.resize(environmentBlendModeCount);
		OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, environmentBlendModeCount, &environmentBlendModeCount, m_environmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

		// Pick the first application supported blend mode supported by the hardware.
		for (const XrEnvironmentBlendMode& environmentBlendMode : m_applicationEnvironmentBlendModes) {
			if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(), environmentBlendMode) != m_environmentBlendModes.end()) {
				m_environmentBlendMode = environmentBlendMode;
				break;
			}
		}
		if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
			XR_TUT_LOG_ERROR("Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
			m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		}
	}

	void CreateReferenceSpace()
	{
		// Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
		XrReferenceSpaceCreateInfo referenceSpaceCI{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		referenceSpaceCI.poseInReferenceSpace = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} };
		OPENXR_CHECK(xrCreateReferenceSpace(m_Session, &referenceSpaceCI, &m_localSpace), "Failed to create ReferenceSpace.");
	}

	void DestroyReferenceSpace()
	{
		// Destroy the reference XrSpace.
		OPENXR_CHECK(xrDestroySpace(m_localSpace), "Failed to destroy Space.")
	}

	void RenderFrame()
	{
		// Get the XrFrameState for timing and rendering info.
		XrFrameState frameState{ XR_TYPE_FRAME_STATE };
		XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
		OPENXR_CHECK(xrWaitFrame(m_Session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

		// Tell the OpenXR compositor that the application is beginning the frame.
		XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
		OPENXR_CHECK(xrBeginFrame(m_Session, &frameBeginInfo), "Failed to begin the XR Frame.");

		// Variables for rendering and layer composition.
		bool rendered = false;
		RenderLayerInfo renderLayerInfo;
		renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

		// Check that the session is active and that we should render.
		bool sessionActive = (m_SessionState == XR_SESSION_STATE_SYNCHRONIZED || m_SessionState == XR_SESSION_STATE_VISIBLE || m_SessionState == XR_SESSION_STATE_FOCUSED);
		if (sessionActive && frameState.shouldRender) {
			// Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
			rendered = RenderLayer(renderLayerInfo);
			if (rendered) {
				renderLayerInfo.layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&renderLayerInfo.layerProjection));
			}
		}

		// Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
		XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
		frameEndInfo.displayTime = frameState.predictedDisplayTime;
		frameEndInfo.environmentBlendMode = m_environmentBlendMode;
		frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
		frameEndInfo.layers = renderLayerInfo.layers.data();
		OPENXR_CHECK(xrEndFrame(m_Session, &frameEndInfo), "Failed to end the XR Frame.");
	}

	struct RenderLayerInfo;
	bool RenderLayer(RenderLayerInfo& renderLayerInfo)
	{
		// Locate the views from the view configuration within the (reference) space at the display time.
		std::vector<XrView> views(m_viewConfigurationViews.size(), { XR_TYPE_VIEW });

		XrViewState viewState{ XR_TYPE_VIEW_STATE };  // Will contain information on whether the position and/or orientation is valid and/or tracked.
		XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
		viewLocateInfo.viewConfigurationType = m_viewConfiguration;
		viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
		viewLocateInfo.space = m_localSpace;
		uint32_t viewCount = 0;
		XrResult result = xrLocateViews(m_Session, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data());
		if (result != XR_SUCCESS) {
			XR_TUT_LOG("Failed to locate Views.");
			return false;
		}

		// Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
		renderLayerInfo.layerProjectionViews.resize(viewCount, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });

		// Per view in the view configuration:
		for (uint32_t i = 0; i < viewCount; i++)
		{
			SwapchainInfo& colorSwapchainInfo = m_colorSwapchainInfos[i];
			SwapchainInfo& depthSwapchainInfo = m_depthSwapchainInfos[i];

			// Acquire and wait for an image from the swapchains.
			// Get the image index of an image in the swapchains.
			// The timeout is infinite.
			uint32_t colorImageIndex = 0;
			uint32_t depthImageIndex = 0;
			XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
			OPENXR_CHECK(xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex), "Failed to acquire Image from the Color Swapchian");
			OPENXR_CHECK(xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex), "Failed to acquire Image from the Depth Swapchian");

			XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
			waitInfo.timeout = XR_INFINITE_DURATION;
			OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Color Swapchain");
			OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Depth Swapchain");

			// Get the width and height and construct the viewport and scissors.
			const uint32_t& width = m_viewConfigurationViews[i].recommendedImageRectWidth;
			const uint32_t& height = m_viewConfigurationViews[i].recommendedImageRectHeight;
			GraphicsAPI::Viewport viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
			GraphicsAPI::Rect2D scissor = { {(int32_t)0, (int32_t)0}, {width, height} };
			float nearZ = 0.05f;
			float farZ = 100.0f;

			// Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view.
			// This also associates the swapchain image with this layer projection view.
			renderLayerInfo.layerProjectionViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
			renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
			renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
			renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
			renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
			renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
			renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
			renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
			renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = 0;  // Useful for multiview rendering.

			// Rendering code to clear the color and depth image views.
			m_GraphicsAPI->BeginRendering();

			if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE)
			{
				// VR mode use a background color.
				m_GraphicsAPI->ClearColor(colorSwapchainInfo.imageViews[colorImageIndex], 0.17f, 0.17f, 0.17f, 1.00f);
			}
			else
			{
				// In AR mode make the background color black.
				m_GraphicsAPI->ClearColor(colorSwapchainInfo.imageViews[colorImageIndex], 0.00f, 0.00f, 0.00f, 1.00f);
			}
			m_GraphicsAPI->ClearDepth(depthSwapchainInfo.imageViews[depthImageIndex], 1.0f);

			m_GraphicsAPI->EndRendering();

			// Give the swapchain image back to OpenXR, allowing the compositor to use the image.
			XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Color Swapchain");
			OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Depth Swapchain");
		}

		// Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
		renderLayerInfo.layerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		renderLayerInfo.layerProjection.space = m_localSpace;
		renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
		renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

		return true;
	}

protected:
	XrInstance m_xrInstance = XR_NULL_HANDLE;
	std::vector<const char*> m_activeAPILayers = {};
	std::vector<const char*> m_activeInstanceExtensions = {};
	std::vector<std::string> m_apiLayers = {};
	std::vector<std::string> m_instanceExtensions = {};

	XrDebugUtilsMessengerEXT m_DebugUtilsMessenger = XR_NULL_HANDLE;

	XrSystemId m_systemID = {};
	XrFormFactor m_FormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrSystemProperties m_systemProperties = { XR_TYPE_SYSTEM_PROPERTIES };

	GraphicsAPI_Type m_APIType = UNKNOWN;
	std::unique_ptr<GraphicsAPI> m_GraphicsAPI = nullptr;

	XrSession m_Session = {};
	XrSessionState m_SessionState = XR_SESSION_STATE_UNKNOWN;

	bool m_applicationRunning = true;
	bool m_sessionRunning = false;

	std::vector<XrViewConfigurationType> m_applicationViewConfigurations = { XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO };
	std::vector<XrViewConfigurationType> m_viewConfigurations;
	XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
	std::vector<XrViewConfigurationView> m_viewConfigurationViews;

	struct SwapchainInfo
	{
		XrSwapchain swapchain = XR_NULL_HANDLE;
		int64_t swapchainFormat = 0;
		std::vector<void*> imageViews;
	};
	std::vector<SwapchainInfo> m_colorSwapchainInfos = {};
	std::vector<SwapchainInfo> m_depthSwapchainInfos = {};

	std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = { XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE };
	std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
	XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

	XrSpace m_localSpace = XR_NULL_HANDLE;
	struct RenderLayerInfo
	{
		XrTime predictedDisplayTime;
		std::vector<XrCompositionLayerBaseHeader*> layers;
		XrCompositionLayerProjection layerProjection = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
		std::vector<XrCompositionLayerProjectionView> layerProjectionViews;
	};
};

int main(int argc, char** argv)
{
	OpenXRTutorial app(OPENGL);
	app.Run();
}