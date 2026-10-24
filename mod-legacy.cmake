# Two perks need core hooks that older cores don't have; the module still builds there and those
# parts do nothing. The probe reads the core header itself, so a fork that has a hook is found
# whatever its version.
#   Frequent Flier: OnPlayerTaxiCostDiscount (every leg cheaper) + OnPlayerTaxiFlightSpeed (faster)
#   Well Rested: OnPlayerGainRestBonus + OnPlayerGetRestBonusMax
file(READ "${CMAKE_SOURCE_DIR}/src/server/game/Scripting/ScriptDefines/PlayerScript.h" _legacy_player_script_h)
string(FIND "${_legacy_player_script_h}" "PLAYERHOOK_ON_TAXI_FLIGHT_SPEED" _legacy_taxi)
string(FIND "${_legacy_player_script_h}" "PLAYERHOOK_ON_GET_REST_BONUS_MAX" _legacy_rest)
string(FIND "${_legacy_player_script_h}" "PLAYERHOOK_ON_TAXI_COST_DISCOUNT" _legacy_taxi_cost)
foreach (LEGACY_TARGET modules mod_mod-legacy)
    if (TARGET ${LEGACY_TARGET})
        if (NOT _legacy_taxi EQUAL -1)
            target_compile_definitions(${LEGACY_TARGET} PRIVATE LEGACY_CORE_HAS_TAXI_SPEED=1)
        endif()
        if (NOT _legacy_rest EQUAL -1)
            target_compile_definitions(${LEGACY_TARGET} PRIVATE LEGACY_CORE_HAS_REST_BONUS=1)
        endif()
        if (NOT _legacy_taxi_cost EQUAL -1)
            target_compile_definitions(${LEGACY_TARGET} PRIVATE LEGACY_CORE_HAS_TAXI_COST=1)
        endif()
    endif()
endforeach()
