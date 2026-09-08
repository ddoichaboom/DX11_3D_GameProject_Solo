#include "IntentResolver.h"

void CIntentResolver::Resolve(const PLAYER_RAW_INPUT_FRAME& Raw, _float fCameraYaw, PLAYER_INTENT_FRAME* pOutIntent)
{
    if (nullptr == pOutIntent)
        return;

    const _float ax = Compute_Axis(Raw.bMoveLeftHeld, Raw.bMoveRightHeld);
    const _float az = Compute_Axis(Raw.bMoveBackwardHeld, Raw.bMoveForwardHeld);
    const _bool bHasMoveIntent = ((0.f != ax) || (0.f != az));

    pOutIntent->lLookDeltaX = Raw.lMouseDeltaX;
    pOutIntent->bDashRequested = Raw.bDashPressed;
    pOutIntent->bAttackRequested = Raw.bLButtonPressed;
    pOutIntent->bGuardHeld = Raw.bRButtonHeld;
    pOutIntent->vMoveDirWorld = { 0.f, 0.f, 0.f };
    pOutIntent->bWeaponSwapRequested = Raw.bWeaponSwapPressed;
    pOutIntent->bSkillFRequested = Raw.bSkillFPressed;
    pOutIntent->bSkillQRequested = Raw.bSkillQPressed;
    pOutIntent->bSkillERequested = Raw.bSkillEPressed;
    pOutIntent->bQTERequested = Raw.bShiftPressed;

    if (true == bHasMoveIntent)
    {
        const _float fSin = sinf(fCameraYaw);
        const _float fCos = cosf(fCameraYaw);

        _float vx = az * fSin + ax * fCos;
        _float vz = az * fCos - ax * fSin;

        const _float fLenSq = vx * vx + vz * vz;
        if (fLenSq > 0.f)
        {
            const _float fInvLen = 1.f / sqrtf(fLenSq);
            vx *= fInvLen;
            vz *= fInvLen;
        }

        pOutIntent->vMoveDirWorld = { vx, 0.f, vz };
    }
}

_float CIntentResolver::Compute_Axis(_bool bNegativeHeld, _bool bPositiveHeld) const
{
    if (bNegativeHeld == bPositiveHeld)
        return 0.f;

    return bPositiveHeld ? 1.f : -1.f;
}

CIntentResolver* CIntentResolver::Create()
{
    CIntentResolver* pInstance = new CIntentResolver();

    if (nullptr == pInstance)
        return nullptr;

    return pInstance;
}

void CIntentResolver::Free()
{
    __super::Free();
}