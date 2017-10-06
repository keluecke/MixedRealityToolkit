#pragma once

namespace SystemInfoHelper
{
	public ref struct RenderScaleOverride sealed
	{
		RenderScaleOverride()
		{
			RenderScaleValue = 0;
			ScaledVerticalResolution = 0;
			MaxVerticalResolution = 0;
		}

		property float RenderScaleValue;
		property unsigned int ScaledVerticalResolution;
		property unsigned int MaxVerticalResolution;
	};

	public ref class SystemInfo sealed
	{
	public:
		SystemInfo(Windows::Graphics::Holographic::HolographicAdapterId adapterId);

		property unsigned int VendorId
		{
			unsigned int get() { return m_vendorId; }
		}

		property Platform::String^ Description
		{
			Platform::String^ get() { return m_description; }
		}

		property float DedicatedVideoMemoryMB
		{
			float get() { return m_dedicatedVideoMemoryMB; }
		}

		property float DedicatedSystemMemoryMB
		{
			float get() { return m_dedicatedSystemMemoryMB; }
		}

		property float SharedSystemMemoryMB
		{
			float get() { return m_sharedSystemMemoryMB; }
		}

		property unsigned int CpuCoreCount
		{
			unsigned int get() { return m_numPhysicalProcessorCores; };
		}

		property unsigned int CpuCount
		{
			unsigned int get() { return m_numProcessors; }
		}

		Windows::Foundation::IAsyncOperation<RenderScaleOverride^>^ ReadRenderScaleAsync();
		Windows::Foundation::IAsyncAction^ WriteRenderScaleAsync(RenderScaleOverride^ renderOverride);

	private:
		IDXGIAdapter* GetAdapterFromId(Windows::Graphics::Holographic::HolographicAdapterId adapterId);
		void FindSystemCpuInfo();

		RenderScaleOverride^ m_renderScaleOverride;
		Platform::String^ m_description;
		static Platform::String^ ms_RenderScaleOverrideSaveFile;
		static Platform::String^ ms_renderScaleValueName;
		static Platform::String^ ms_scaledVerticalResolution;
		static Platform::String^ ms_maxVerticalResolution;

		unsigned int m_vendorId = 0;
		float m_dedicatedVideoMemoryMB = 0.0f;
		float m_dedicatedSystemMemoryMB = 0.0f;
		float m_sharedSystemMemoryMB = 0.0f;

		float m_bToMB;

		bool m_isAmd64 = false;
		uint32_t m_numProcessors = 0;
		uint32_t m_numPhysicalProcessorCores = 0;
	};
}
