#include "pch.h"
#include "SystemInfo.h"

using namespace SystemInfoHelper;
using namespace Platform;
using namespace Microsoft::WRL;
using namespace Windows::Storage;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;

Platform::String^ SystemInfo::ms_RenderScaleOverrideSaveFile =	L"RenderScaleOverride.json";
Platform::String^ SystemInfo::ms_renderScaleValueName =			L"render_scale";
Platform::String^ SystemInfo::ms_scaledVerticalResolution =		L"scaled_vertical_resolution";
Platform::String^ SystemInfo::ms_maxVerticalResolution =		L"max_vertical_resolution";

SystemInfo::SystemInfo(Windows::Graphics::Holographic::HolographicAdapterId adapterId) :
	m_bToMB(1.0f / (1024.f * 1024.f))
{
	DXGI_ADAPTER_DESC desc;
	DXGI_ADAPTER_DESC3 desc3;

	ComPtr<IDXGIAdapter> dxgiAdapter = this->GetAdapterFromId(adapterId);
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	bool dataRecieved = false;
	float dedicatedVideoMemory = 0.0f;
	float dedicatedSystemMemory = 0.0f;
	float sharedSystemMemory = 0.0f;

	if (dataRecieved = SUCCEEDED(dxgiAdapter.As(&dxgiAdapter4)))
	{
		if (dataRecieved = SUCCEEDED(dxgiAdapter4->GetDesc3(&desc3)))
		{
			m_description = ref new Platform::String(&desc3.Description[0]);
			m_vendorId = desc3.VendorId;
			dedicatedVideoMemory = static_cast<float>(desc3.DedicatedVideoMemory);
			sharedSystemMemory = static_cast<float>(desc3.SharedSystemMemory);
			dedicatedSystemMemory = static_cast<float>(desc3.DedicatedSystemMemory);
		}
	}

	if (!dataRecieved)
	{
		if (SUCCEEDED(dxgiAdapter->GetDesc(&desc)))
		{
			m_description = ref new Platform::String(&desc.Description[0]);
			m_vendorId = desc.VendorId;
			dedicatedVideoMemory = static_cast<float>(desc.DedicatedVideoMemory);
			sharedSystemMemory = static_cast<float>(desc.SharedSystemMemory);
			dedicatedSystemMemory = static_cast<float>(desc.DedicatedSystemMemory);
		}
	}

	m_renderScaleOverride = ref new RenderScaleOverride();

	m_dedicatedVideoMemoryMB = dedicatedVideoMemory * m_bToMB;
	m_sharedSystemMemoryMB = sharedSystemMemory * m_bToMB;
	m_dedicatedSystemMemoryMB = dedicatedSystemMemory * m_bToMB;
}

IAsyncOperation<RenderScaleOverride^>^ SystemInfo::ReadRenderScaleAsync()
{
	return concurrency::create_async(
		[this]()
		{
			if (m_renderScaleOverride->RenderScaleValue > 0)
			{
				return m_renderScaleOverride;
			}

			StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
			return concurrency::create_task(
				storageFolder->GetFileAsync(ms_RenderScaleOverrideSaveFile)
			).then(
				[](StorageFile^ file)
				{
					return FileIO::ReadTextAsync(file);
				}
			).then(
				[this](String ^ textData)
				{
					RenderScaleOverride^ localOverride = ref new RenderScaleOverride();
					JsonValue^ jsonValue = JsonValue::Parse(textData);
					localOverride->RenderScaleValue = (float)jsonValue->GetObject()->GetNamedNumber(ms_renderScaleValueName);
					localOverride->ScaledVerticalResolution = (unsigned int)jsonValue->GetObject()->GetNamedNumber(ms_scaledVerticalResolution);
					localOverride->MaxVerticalResolution = (unsigned int)jsonValue->GetObject()->GetNamedNumber(ms_maxVerticalResolution);

					// validate the values read from the file
					// TODO: shall we have some sane max values just in case
					if (localOverride->RenderScaleValue < 0 || localOverride->RenderScaleValue > 2 ||
						localOverride->ScaledVerticalResolution == 0 || localOverride->MaxVerticalResolution == 0)
					{
						return m_renderScaleOverride;
					}

					m_renderScaleOverride = localOverride;
					return m_renderScaleOverride;
				}
			).get();
		}
	);
}

IAsyncAction^ SystemInfo::WriteRenderScaleAsync(RenderScaleOverride^ renderOverride)
{
	return concurrency::create_async(
		[this, renderOverride]()
		{
			JsonObject^ jsonObj = ref new JsonObject();
			jsonObj->Insert(ms_renderScaleValueName, JsonValue::CreateNumberValue(renderOverride->RenderScaleValue));
			jsonObj->Insert(ms_scaledVerticalResolution, JsonValue::CreateNumberValue(renderOverride->ScaledVerticalResolution));
			jsonObj->Insert(ms_maxVerticalResolution, JsonValue::CreateNumberValue(renderOverride->MaxVerticalResolution));

			StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
			concurrency::create_task(
				storageFolder->CreateFileAsync(ms_RenderScaleOverrideSaveFile, CreationCollisionOption::ReplaceExisting)
			).then(
				[jsonObj](StorageFile^ renderScaleOverridesFile)
				{
					return FileIO::WriteTextAsync(renderScaleOverridesFile, jsonObj->Stringify());
				}
			).get();

			m_renderScaleOverride = renderOverride;
		}
	);
}

IDXGIAdapter* SystemInfo::GetAdapterFromId(Windows::Graphics::Holographic::HolographicAdapterId adapterId)
{
	ComPtr<IDXGIFactory1> spDXGIFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&spDXGIFactory);
	if (FAILED(hr))
		return nullptr;

	ComPtr<IDXGIAdapter> spDXGIAdapter;
	for (UINT i = 0;
		spDXGIFactory->EnumAdapters(i, &spDXGIAdapter) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		DXGI_ADAPTER_DESC desc;
		if (FAILED(spDXGIAdapter->GetDesc(&desc)))
		{
			spDXGIAdapter.Reset();
			continue;
		}

		if (desc.AdapterLuid.HighPart == adapterId.HighPart &&
			desc.AdapterLuid.LowPart == adapterId.LowPart)
		{
			break;
		}
	}

	return spDXGIAdapter.Detach();
}

void SystemInfo::FindSystemCpuInfo()
{
	SYSTEM_INFO systemInfo;
	ZeroMemory(&systemInfo, sizeof(SYSTEM_INFO));
	GetNativeSystemInfo(&systemInfo);

	// This will provide accurate information, even when called by a 32-bit process.
	switch (systemInfo.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		m_isAmd64 = true;
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		m_isAmd64 = false;
		break;
	}

	m_numPhysicalProcessorCores = 0;
	DWORD processorInfoCount = 0;
	GetLogicalProcessorInformation(nullptr, &processorInfoCount);
	{
		std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> processorInfo;
		processorInfo.resize(processorInfoCount);

		if (GetLogicalProcessorInformation(&processorInfo[0], &processorInfoCount))
		{
			for (auto info : processorInfo)
			{
				if (info.Relationship == RelationProcessorCore &&
					info.ProcessorCore.Flags)
				{
					++m_numPhysicalProcessorCores;
				}

				if (info.Relationship == RelationProcessorPackage)
				{
					++m_numProcessors;
				}
			}
		}
	}
}