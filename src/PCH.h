#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <spdlog/sinks/basic_file_sink.h>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
namespace WinAPI = SKSE::WinAPI;

#ifdef SKYRIM_AE
    #define OFFSET(se, ae) ae
    #define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
    #define OFFSET(se, ae) se
    #define OFFSET_3(se, ae, vr) vr
#else
    #define OFFSET(se, ae) se
    #define OFFSET_3(se, ae, vr) se
#endif

#include "Version.h"

namespace stl {
    template <class T>
    void write_thunk_call(std::uintptr_t a_src) {
        SKSE::AllocTrampoline(14);

        auto& trampoline = SKSE::GetTrampoline();
        T::func = trampoline.write_call<5>(a_src, T::thunk);
    }
}