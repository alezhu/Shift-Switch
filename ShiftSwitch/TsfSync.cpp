#include "pch.h"
#include "TsfSync.h"
#include <msctf.h>

namespace TsfSync
{
namespace
{
	struct TSF_CACHE {
		ITfInputProcessorProfileMgr* profiles = nullptr;
	};

	TSF_CACHE& getTsfCache() {
		static thread_local TSF_CACHE cache;
		return cache;
	}

	bool isComInitialized() {
		typedef HRESULT(WINAPI* FN_CoGetApartmentType)(APTTYPE*, APTTYPEQUALIFIER*);
		const auto ole32 = GetModuleHandleW(L"ole32.dll");
		if (ole32 == nullptr)
			return true;
		const auto lpfn_CoGetApartmentType =
			reinterpret_cast<FN_CoGetApartmentType>(GetProcAddress(ole32, "CoGetApartmentType"));
		if (lpfn_CoGetApartmentType == nullptr)
			return true;
		APTTYPE type;
		APTTYPEQUALIFIER qualifier;
		return SUCCEEDED(lpfn_CoGetApartmentType(&type, &qualifier));
	}

	bool ensureTsfProfiles() {
		TSF_CACHE& cache = getTsfCache();
		if (cache.profiles != nullptr)
			return true;
		if (!isComInitialized())
			return false;

		ITfInputProcessorProfiles* legacy = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
			CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&legacy));
		if (SUCCEEDED(hr))
		{
			hr = legacy->QueryInterface(IID_PPV_ARGS(&cache.profiles));
			legacy->Release();
		}
		return SUCCEEDED(hr) && cache.profiles != nullptr;
	}
}

bool SyncTsfProfile(const HKL hkl) {
	if (hkl == nullptr || !ensureTsfProfiles())
		return false;

	const LANGID langid = LOWORD(reinterpret_cast<UINT_PTR>(hkl));
	const CLSID nullClsid = {};
	const GUID nullGuid = {};
	HRESULT hr = getTsfCache().profiles->ActivateProfile(
		TF_PROFILETYPE_KEYBOARDLAYOUT, langid, nullClsid, nullGuid, hkl,
		TF_IPPMF_FORPROCESS);
	if (FAILED(hr))
	{
		hr = getTsfCache().profiles->ActivateProfile(
			TF_PROFILETYPE_KEYBOARDLAYOUT, langid, nullClsid, nullGuid, hkl,
			TF_IPPMF_FORPROCESS | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
	}
	return SUCCEEDED(hr);
}
}
