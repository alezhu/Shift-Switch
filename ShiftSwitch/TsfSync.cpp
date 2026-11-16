#include "pch.h"
#include "TsfSync.h"
#include <msctf.h>

namespace TsfSync
{
namespace
{
	struct ScopedApartment {
		HRESULT hr = E_FAIL;
		bool shouldUninit = false;
		ScopedApartment() {
			hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			if (hr == S_OK || hr == S_FALSE) {
				shouldUninit = true;
			}
		}
		~ScopedApartment() {
			if (shouldUninit) {
				CoUninitialize();
			}
		}
	};
}

bool SyncTsfProfile(const HKL hkl) {
	if (hkl == nullptr)
		return false;

	const LANGID langid = LOWORD(reinterpret_cast<UINT_PTR>(hkl));

	ScopedApartment com;

	ITfInputProcessorProfiles* profiles = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
	if (FAILED(hr) || profiles == nullptr)
		return false;

	// 1. ChangeCurrentLanguage and ActivateLanguageProfile notify TSF sinks (including Language Bar / Taskbar)
	profiles->ChangeCurrentLanguage(langid);
	profiles->ActivateLanguageProfile(CLSID_NULL, langid, GUID_NULL);

	// 2. ActivateProfile updates the text service profile for the process and session
	ITfInputProcessorProfileMgr* profileMgr = nullptr;
	hr = profiles->QueryInterface(IID_PPV_ARGS(&profileMgr));
	if (SUCCEEDED(hr) && profileMgr != nullptr)
	{
		const CLSID nullClsid = {};
		const GUID nullGuid = {};
		const DWORD flags = TF_IPPMF_FORSESSION | TF_IPPMF_FORPROCESS;

		hr = profileMgr->ActivateProfile(
			TF_PROFILETYPE_KEYBOARDLAYOUT, langid, nullClsid, nullGuid, hkl,
			flags);
		if (FAILED(hr))
		{
			hr = profileMgr->ActivateProfile(
				TF_PROFILETYPE_KEYBOARDLAYOUT, langid, nullClsid, nullGuid, hkl,
				flags | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
		}
		if (FAILED(hr))
		{
			profileMgr->ActivateProfile(
				TF_PROFILETYPE_KEYBOARDLAYOUT, langid, nullClsid, nullGuid, hkl,
				TF_IPPMF_FORPROCESS);
		}
		profileMgr->Release();
	}

	profiles->Release();
	return true;
}
}
