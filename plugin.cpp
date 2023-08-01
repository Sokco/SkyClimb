#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
}

/*
struct OurEventSink : public RE::BSTEventSink<SKSE::CameraEvent> {
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CameraEvent* event, RE::BSTEventSource<SKSE::CameraEvent> *) {
        
        
        RE::TESCameraState *furnitureState = RE::PlayerCamera::GetSingleton()->cameraStates[5].get();

        if (event->newState->id == furnitureState->id) {
            
             RE::PlayerCamera::GetSingleton()->SetState(event->oldState);
             logger::info("Camera state reverted");
        }
        
        
        return RE::BSEventNotifyControl::kContinue;



    }
};
*/

std::string SayHello(RE::StaticFunctionTag *) { 

    return "Hello from SkyClimb 5!"; 
}



RE::NiPoint3 CameraDirInternal() {

    const auto worldCamera = RE::Main::WorldRootCamera();

    

    RE::NiPoint3 output;
    output.x = worldCamera->world.rotate.entry[0][0];
    output.y = worldCamera->world.rotate.entry[1][0];
    output.z = worldCamera->world.rotate.entry[2][0];

    return output;
}

float getSign(float x) {
    if (x < 0) return -1;
    else return 1;
}

void ToggleJumpingInternal(bool enabled) {
    RE::ControlMap::GetSingleton()->ToggleControls(RE::ControlMap::UEFlag::kJumping, enabled);
}

void ToggleJumping(RE::StaticFunctionTag *, bool enabled) { ToggleJumpingInternal(enabled); }


//camera versus head 'to object angle'. Angle between the vectors 'camera to object' and 'player head to object'
float CameraVsHeadToObjectAngle(RE::NiPoint3 objPoint) {
    const auto player = RE::PlayerCharacter::GetSingleton();

    RE::NiPoint3 playerToObject = objPoint - (player->GetPosition() + RE::NiPoint3(0, 0, 120));

    playerToObject /= playerToObject.Length();

    RE::NiPoint3 camDir = CameraDirInternal();

    float dot = playerToObject.Dot(camDir);

    const float radToDeg = (float)57.2958;

    return acos(dot) * radToDeg;
}



float RayCast(RE::NiPoint3 rayStart, RE::NiPoint3 rayDir, float maxDist, RE::hkVector4 &normalOut) {

    RE::NiPoint3 rayEnd = rayStart + rayDir * maxDist;

    const auto bhkWorld = RE::PlayerCharacter::GetSingleton()->GetParentCell()->GetbhkWorld();
    if (!bhkWorld) {
        return maxDist;
    }

    RE::bhkPickData pickData;

    const auto havokWorldScale = RE::bhkWorld::GetWorldScale();

    pickData.rayInput.from = rayStart * havokWorldScale;
    pickData.rayInput.to = rayEnd * havokWorldScale;
    pickData.rayInput.enableShapeCollectionFilter = false;
    pickData.rayInput.filterInfo = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | SKSE::stl::to_underlying(RE::COL_LAYER::kLOS);

    if (bhkWorld->PickObject(pickData); pickData.rayOutput.HasHit()) {

        normalOut = pickData.rayOutput.normal;

        //fail if hit a character
        switch (static_cast<RE::COL_LAYER>(pickData.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo & 0x7F)) {
            case RE::COL_LAYER::kCharController:
            case RE::COL_LAYER::kBiped:
            case RE::COL_LAYER::kDeadBip:
            case RE::COL_LAYER::kBipedNoCC:
            case RE::COL_LAYER::kWater:

                return -1;

            default: {
                //hit something useful!
                return maxDist * pickData.rayOutput.hitFraction;

            } break;
        }

    }

    normalOut = RE::hkVector4(0, 0, 0, 0);

    //didn't hit anything!
    return maxDist;
}


float magnitudeXY(float x, float y) {

    return sqrt(x * x + y * y);

}






bool UpdateParkourPointInternal(RE::TESObjectREFR* medMarkerRef, RE::TESObjectREFR* highMarkerRef, RE::TESObjectREFR* indicatorRef, float minLedgeHeight, float maxLedgeHeight) { 
    
    
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto playerPos = player->GetPosition();

    RE::NiPoint3 cameraDir = CameraDirInternal();

    float cameraDirTotal = magnitudeXY(cameraDir.x, cameraDir.y);
    RE::NiPoint3 cameraDirFlat;
    cameraDirFlat.x = cameraDir.x / cameraDirTotal;
    cameraDirFlat.y = cameraDir.y / cameraDirTotal;
    cameraDirFlat.z = 0;


    float startZOffset = 100; // how high to start the raycast above the feet of the player
    float playerHeight = 120; // how much headroom is needed
    float minUpCheck = 100; // how low can the roof be relative to the raycast starting point?
    float maxUpCheck = (maxLedgeHeight - startZOffset) + 20; // how high do we even check for a roof?
    float fwdCheck = 10; // how much each incremental forward check steps forward
    int fwdCheckIterations = 12; // how many incremental forward checks do we make?
    float minLedgeFlatness = 0.5; // 1 is perfectly flat, 0 is completely perpendicular

    RE::hkVector4 normalOut(0, 0, 0, 0);

    //grounded check
    float groundedCheckDist = 128 + 20;

    RE::NiPoint3 groundedRayStart;
    groundedRayStart.x = playerPos.x;
    groundedRayStart.y = playerPos.y;
    groundedRayStart.z = playerPos.z + 128;

    RE::NiPoint3 groundedRayDir(0, 0, -1);

    float groundedRayDist = RayCast(groundedRayStart, groundedRayDir, groundedCheckDist, normalOut);

    if (groundedRayDist == groundedCheckDist || groundedRayDist == -1) {
    

        logger::info("player is not grounded.");
        return false;

    }


    //raycast upwards to sky, making sure no roof is too low above us
    RE::NiPoint3 upRayStart;
    upRayStart.x = playerPos.x;
    upRayStart.y = playerPos.y;
    upRayStart.z = playerPos.z + startZOffset;

    RE::NiPoint3 upRayDir(0, 0, 1);

    float upRayDist = RayCast(upRayStart, upRayDir, maxUpCheck, normalOut);

    if (upRayDist < minUpCheck) {
    
        return false;

    }

    RE::NiPoint3 fwdRayStart = upRayStart + upRayDir * (upRayDist - 10);
    RE::NiPoint3 fwdRayDir = cameraDirFlat;

    RE::NiPoint3 downRayStart;
    RE::NiPoint3 downRayDir(0, 0, -1);

    RE::NiPoint3 ledgePoint;
    bool foundLedge = false;

    // if nothing above, raycast forwards then down to find ledge
    // incrementally step forward to find closest ledge in front
    for (int i = 0; i < fwdCheckIterations; i++)
    {
        // raycast forward

        float fwdRayDist = RayCast(fwdRayStart, fwdRayDir, fwdCheck * (float)i, normalOut);

        if (fwdRayDist < fwdCheck * (float)i) {

            continue;
        }

        // if nothing forward, raycast back down

        downRayStart = fwdRayStart + fwdRayDir * fwdRayDist;

        float downRayDist = RayCast(downRayStart, downRayDir, startZOffset + maxUpCheck, normalOut);

        ledgePoint = downRayStart + downRayDir * downRayDist;


        float normalZ = normalOut.quad.m128_f32[2];

        //if found ledgePoint is too low/high, or the normal is too steep, or the ray hit oddly soon, skip and increment forward again
        if (ledgePoint.z < playerPos.z + minLedgeHeight || ledgePoint.z > playerPos.z + maxLedgeHeight || downRayDist < 10 || normalZ < minLedgeFlatness) {
            continue;
        } 
        else {
            foundLedge = true;
            break;
        }
    }

    //if no ledge found, return false
    if (foundLedge == false) {

        return false;
    }

    //if camera facing too far away from ledgepoint
    if (CameraVsHeadToObjectAngle(ledgePoint) > 80) {
        return false;
    }

    //make sure player can stand on top
    float headroomBuffer = 10;

    RE::NiPoint3 headroomRayStart = ledgePoint + upRayDir * headroomBuffer;

    float headroomRayDist = RayCast(headroomRayStart, upRayDir, playerHeight - headroomBuffer, normalOut);

    if (headroomRayDist < playerHeight - headroomBuffer)
    {
        return false;
    }


    
    //adjust the EVG marker for correct positioning
    float zAdjust = -155;
    float toCameraAdjust = -50;

    RE::NiPoint3 medAdjustedPos;
    medAdjustedPos.x = ledgePoint.x + cameraDirFlat.x * toCameraAdjust;
    medAdjustedPos.y = ledgePoint.y + cameraDirFlat.y * toCameraAdjust;
    medAdjustedPos.z = ledgePoint.z + zAdjust;

    zAdjust = -200;
    toCameraAdjust = -50;

    RE::NiPoint3 highAdjustedPos;
    highAdjustedPos.x = ledgePoint.x + cameraDirFlat.x * toCameraAdjust;
    highAdjustedPos.y = ledgePoint.y + cameraDirFlat.y * toCameraAdjust;
    highAdjustedPos.z = ledgePoint.z + zAdjust;


    //it seems we have to MoveTo first in order to get the references into the same cell

    if (indicatorRef->GetParentCell() != player->GetParentCell())
    {
        indicatorRef->MoveTo(player->AsReference());
    }
    highMarkerRef->MoveTo(player->AsReference());
    medMarkerRef->MoveTo(player->AsReference());

    //place the references at the ledge point
    indicatorRef->data.location = ledgePoint + upRayDir * 5;
    highMarkerRef->SetPosition(highAdjustedPos);
    medMarkerRef->SetPosition(medAdjustedPos);

    indicatorRef->Update3DPosition(true);

    //rotate to face camera
    float zAngle = atan2(cameraDirFlat.x, cameraDirFlat.y);
    highMarkerRef->data.angle = RE::NiPoint3(0, 0, zAngle);
    medMarkerRef->data.angle = RE::NiPoint3(0, 0, zAngle);
    indicatorRef->data.angle = RE::NiPoint3(0, 0, zAngle);


    return true;


}

bool UpdateParkourPoint(RE::StaticFunctionTag *, RE::TESObjectREFR *medMarkerRef, RE::TESObjectREFR *highMarkerRef, RE::TESObjectREFR *indicatorRef, float minLedgeHeight, float maxLedgeHeight) {
    bool foundParkourPoint = UpdateParkourPointInternal(medMarkerRef, highMarkerRef, indicatorRef, minLedgeHeight, maxLedgeHeight);

    if (foundParkourPoint) {
        ToggleJumpingInternal(false);
    }

    return foundParkourPoint;
}


bool PapyrusFunctions(RE::BSScript::IVirtualMachine * vm) { 
    vm->RegisterFunction("SayHello", "SkyClimbPapyrus", SayHello);

    vm->RegisterFunction("ToggleJumping", "SkyClimbPapyrus", ToggleJumping);

    vm->RegisterFunction("UpdateParkourPoint", "SkyClimbPapyrus", UpdateParkourPoint);

    
    return true; 
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);

    //////// PLUGIN START /////////


    SetupLog();
    logger::info("SkyClimb Papyrus Started!");

    SKSE::GetPapyrusInterface()->Register(PapyrusFunctions);


    //auto *eventSink = new OurEventSink();

    //SKSE::GetCameraEventSource()->AddEventSink(eventSink);

    //////// PLUGIN END /////////

    return true;
}