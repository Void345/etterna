#include "global.h"
#include "ActorUtil.h"
#include "AnnouncerManager.h"
#include "CodeDetector.h"
#include "CommonMetrics.h"
#include "CryptManager.h"
#include "GameConstantsAndTypes.h"
#include "GameManager.h"
#include "GameSoundManager.h"
#include "GameState.h"
#include "Grade.h"
#include "InputEventPlus.h"
#include "PlayerState.h"
#include "PrefsManager.h"
#include "ScoreManager.h"
#include "ProfileManager.h"
#include "RageDisplay.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "ScoreKeeperNormal.h"
#include "ScreenEvaluation.h"
#include "ScreenManager.h"
#include "Song.h"
#include "SongManager.h"
#include "StatsManager.h"
#include "StepMania.h"
#include "Steps.h"
#include "ThemeManager.h"
#include "GamePreferences.h"

#define CHEER_DELAY_SECONDS THEME->GetMetricF(m_sName, "CheerDelaySeconds")
#define BAR_ACTUAL_MAX_COMMAND THEME->GetMetricA(m_sName, "BarActualMaxCommand")

static ThemeMetric<TapNoteScore> g_MinScoreToMaintainCombo(
  "Gameplay",
  "MinScoreToMaintainCombo");
static const int NUM_SHOWN_RADAR_CATEGORIES = 5;

AutoScreenMessage(SM_PlayCheer);

REGISTER_SCREEN_CLASS(ScreenEvaluation);

ScreenEvaluation::ScreenEvaluation()
{
	GAMESTATE->m_AdjustTokensBySongCostForFinalStageCheck = false;
}

ScreenEvaluation::~ScreenEvaluation()
{
	GAMESTATE->m_AdjustTokensBySongCostForFinalStageCheck = true;
}

void
ScreenEvaluation::Init()
{
	LOG->Trace("ScreenEvaluation::Init()");

	// debugging
	// Only fill StageStats with fake info if we're the InitialScreen
	// (i.e. StageStats not already filled)
	if (PREFSMAN->m_sTestInitialScreen.Get() == m_sName) {
		PROFILEMAN->LoadFirstAvailableProfile(PLAYER_1);

		STATSMAN->m_vPlayedStageStats.clear();
		STATSMAN->m_vPlayedStageStats.push_back(StageStats());
		StageStats& ss = STATSMAN->m_vPlayedStageStats.back();

		GAMESTATE->m_PlayMode.Set(PLAY_MODE_REGULAR);
		GAMESTATE->SetCurrentStyle(
		  GAMEMAN->GameAndStringToStyle(GAMEMAN->GetDefaultGame(), "versus"),
		  PLAYER_INVALID);
		ss.m_playMode = GAMESTATE->m_PlayMode;
		ss.m_Stage = Stage_1st;
		enum_add(ss.m_Stage, random_up_to(3));
		GAMESTATE->SetMasterPlayerNumber(PLAYER_1);
		GAMESTATE->m_pCurSong.Set(SONGMAN->GetRandomSong());
		ss.m_vpPlayedSongs.push_back(GAMESTATE->m_pCurSong);
		ss.m_vpPossibleSongs.push_back(GAMESTATE->m_pCurSong);
		GAMESTATE->m_iCurrentStageIndex = 0;
		GAMESTATE->m_iPlayerStageTokens = 1;

		ss.m_player.m_pStyle = GAMESTATE->GetCurrentStyle(PLAYER_1);
		if (RandomInt(2))
			PO_GROUP_ASSIGN_N(GAMESTATE->m_pPlayerState->m_PlayerOptions,
								ModsLevel_Stage,
								m_bTransforms,
								PlayerOptions::TRANSFORM_ECHO,
								true); // show "disqualified"
		SO_GROUP_ASSIGN(
			GAMESTATE->m_SongOptions, ModsLevel_Stage, m_fMusicRate, 1.1f);

		GAMESTATE->JoinPlayer(PLAYER_1);
		GAMESTATE->m_pCurSteps.Set(
			GAMESTATE->m_pCurSong->GetAllSteps()[0]);
		ss.m_player.m_vpPossibleSteps.push_back(
			GAMESTATE->m_pCurSteps);
		ss.m_player.m_iStepsPlayed = 1;

		PO_GROUP_ASSIGN(GAMESTATE->m_pPlayerState->m_PlayerOptions,
						ModsLevel_Stage,
						m_fScrollSpeed,
						2.0f);
		PO_GROUP_CALL(GAMESTATE->m_pPlayerState->m_PlayerOptions,
						ModsLevel_Stage,
						ChooseRandomModifiers);

		for( float f = 0; f < 100.0f; f += 1.0f )
		{
			float fP1 = fmodf(f/100*4+.3f,1);
			ss.m_player.SetLifeRecordAt( fP1, f );
		}
		float fSeconds = GAMESTATE->m_pCurSong->GetStepsSeconds();
		ss.m_player.m_iActualDancePoints = RandomInt(3);
		ss.m_player.m_iPossibleDancePoints = 2;
		if (RandomInt(2))
			ss.m_player.m_iCurCombo = RandomInt(15000);
		else
			ss.m_player.m_iCurCombo = 0;
		ss.m_player.UpdateComboList(0, true);

		ss.m_player.m_iCurCombo += 50;
		ss.m_player.UpdateComboList(0.10f * fSeconds, false);

		ss.m_player.m_iCurCombo = 0;
		ss.m_player.UpdateComboList(0.15f * fSeconds, false);
		ss.m_player.m_iCurCombo = 1;
		ss.m_player.UpdateComboList(0.25f * fSeconds, false);
		ss.m_player.m_iCurCombo = 50;
		ss.m_player.UpdateComboList(0.35f * fSeconds, false);
		ss.m_player.m_iCurCombo = 0;
		ss.m_player.UpdateComboList(0.45f * fSeconds, false);
		ss.m_player.m_iCurCombo = 1;
		ss.m_player.UpdateComboList(0.50f * fSeconds, false);
		ss.m_player.m_iCurCombo = 100;
		ss.m_player.UpdateComboList(1.00f * fSeconds, false);
		if (RandomInt(5) == 0) {
			ss.m_player.m_bFailed = true;
		}
		ss.m_player.m_iTapNoteScores[TNS_W1] = RandomInt(3);
		ss.m_player.m_iTapNoteScores[TNS_W2] = RandomInt(3);
		ss.m_player.m_iTapNoteScores[TNS_W3] = RandomInt(3);
		ss.m_player.m_iPossibleGradePoints =
			4 * ScoreKeeperNormal::TapNoteScoreToGradePoints(TNS_W1, false);
		ss.m_player.m_fLifeRemainingSeconds = randomf(90, 580);
		ss.m_player.m_iScore = random_up_to(900 * 1000 * 1000);
		ss.m_player.m_iPersonalHighScoreIndex = (random_up_to(3)) - 1;
		ss.m_player.m_iMachineHighScoreIndex = (random_up_to(3)) - 1;

		FOREACH_ENUM(RadarCategory, rc)
		{
			switch (rc) {
				case RadarCategory_TapsAndHolds:
				case RadarCategory_Jumps:
				case RadarCategory_Holds:
				case RadarCategory_Mines:
				case RadarCategory_Hands:
				case RadarCategory_Rolls:
				case RadarCategory_Lifts:
				case RadarCategory_Fakes:
					ss.m_player.m_radarPossible[rc] =
						1 + (random_up_to(200));
					ss.m_player.m_radarActual[rc] = random_up_to(
						static_cast<int>(ss.m_player.m_radarPossible[rc]));
					break;
				default:
					break;
			}

			; // filled in by ScreenGameplay on start of notes
		}
	}

	if (STATSMAN->m_vPlayedStageStats.empty()) {
		LuaHelpers::ReportScriptError("PlayerStageStats is empty!  Do not use "
									  "SM_GoToNextScreen on ScreenGameplay, "
									  "use SM_DoNextScreen instead so that "
									  "ScreenGameplay can clean up properly.");
		STATSMAN->m_vPlayedStageStats.push_back(STATSMAN->m_CurStageStats);
	}
	m_pStageStats = &STATSMAN->m_vPlayedStageStats.back();

	m_bSavedScreenshot = false;



	// update persistent statistics
	if (GamePreferences::m_AutoPlay == PC_REPLAY) {
		m_pStageStats->m_player.m_HighScore.SetRadarValues(
		  m_pStageStats->m_player.m_radarActual);
	}

	// Run this here, so STATSMAN->m_CurStageStats is available to overlays.
	ScreenWithMenuElements::Init();

	// load sounds
	m_soundStart.Load(THEME->GetPathS(m_sName, "start"));

	// init records area
	bool bOneHasNewTopRecord = false;
	bool bOneHasFullW1Combo = false;
	bool bOneHasFullW2Combo = false;
	bool bOneHasFullW3Combo = false;
	bool bOneHasFullW4Combo = false;
	if (GAMESTATE->IsPlayerEnabled(PLAYER_1)) {
		if ((m_pStageStats->m_player.m_iMachineHighScoreIndex == 0 ||
				m_pStageStats->m_player.m_iPersonalHighScoreIndex == 0)) {
			bOneHasNewTopRecord = true;
		}

		if (m_pStageStats->m_player.FullComboOfScore(TNS_W4))
			bOneHasFullW4Combo = true;

		if (m_pStageStats->m_player.FullComboOfScore(TNS_W3))
			bOneHasFullW3Combo = true;

		if (m_pStageStats->m_player.FullComboOfScore(TNS_W2))
			bOneHasFullW2Combo = true;

		if (m_pStageStats->m_player.FullComboOfScore(TNS_W1))
			bOneHasFullW1Combo = true;
	}

	if (bOneHasNewTopRecord &&
		ANNOUNCER->HasSoundsFor("evaluation new record")) {
		SOUND->PlayOnceFromDir(ANNOUNCER->GetPathTo("evaluation new record"));
	} else if (bOneHasFullW4Combo && g_MinScoreToMaintainCombo == TNS_W4) {
		SOUND->PlayOnceFromDir(
		  ANNOUNCER->GetPathTo("evaluation full combo W4"));
	} else if ((bOneHasFullW1Combo || bOneHasFullW2Combo ||
				bOneHasFullW3Combo)) {
		RString sComboType =
		  bOneHasFullW1Combo ? "W1" : (bOneHasFullW2Combo ? "W2" : "W3");
		SOUND->PlayOnceFromDir(
		  ANNOUNCER->GetPathTo("evaluation full combo " + sComboType));
	}
}

bool
ScreenEvaluation::Input(const InputEventPlus& input)
{
	if (IsTransitioning())
		return false;

	if (input.GameI.IsValid()) {
		if (CodeDetector::EnteredCode(input.GameI.controller,
									  CODE_SAVE_SCREENSHOT1) ||
			CodeDetector::EnteredCode(input.GameI.controller,
									  CODE_SAVE_SCREENSHOT2)) {
			PlayerNumber pn = input.pn;
			bool bHoldingShift = (INPUTFILTER->IsBeingPressed(
									DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT)) ||
								  INPUTFILTER->IsBeingPressed(
									DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT)));
			RString sDir;
			RString sFileName;
			// To save a screenshot to your own profile you must hold shift
			// and press the button it saves compressed so you don't end up
			// with an inflated profile size
			// Otherwise, you can tap away at the screenshot button without
			// holding shift.
			if (bHoldingShift && PROFILEMAN->IsPersistentProfile(pn)) {
				if (!m_bSavedScreenshot) {
					Profile* pProfile = PROFILEMAN->GetProfile(pn);
					sDir = PROFILEMAN->GetProfileDir((ProfileSlot)pn) +
						   "Screenshots/";
					sFileName = StepMania::SaveScreenshot(
					  sDir, bHoldingShift, true, "", "");
					if (!sFileName.empty()) {
						RString sPath = sDir + sFileName;

						const HighScore& hs =
						  m_pStageStats->m_player.m_HighScore;
						Screenshot screenshot;
						screenshot.sFileName = sFileName;
						screenshot.sMD5 =
						  BinaryToHex(CRYPTMAN->GetMD5ForFile(sPath));
						screenshot.highScore = hs;
						pProfile->AddScreenshot(screenshot);
					}
					m_bSavedScreenshot = true;
				}
			} else {
				sDir = "Screenshots/";
				sFileName =
				  StepMania::SaveScreenshot(sDir, bHoldingShift, true, "", "");
			}
			return true; // handled
		}
	}

	return ScreenWithMenuElements::Input(input);
}

void
ScreenEvaluation::HandleScreenMessage(const ScreenMessage SM)
{
	if (SM == SM_PlayCheer) {
		SOUND->PlayOnceFromDir(ANNOUNCER->GetPathTo("evaluation cheer"));
	}

	ScreenWithMenuElements::HandleScreenMessage(SM);
}

bool
ScreenEvaluation::MenuBack(const InputEventPlus& input)
{
	return MenuStart(input);
}

bool
ScreenEvaluation::MenuStart(const InputEventPlus& input)
{
	if (IsTransitioning())
		return false;

	m_soundStart.Play(true);

	HandleMenuStart();
	return true;
}

void
ScreenEvaluation::HandleMenuStart()
{
	StepsID stepsid;
	stepsid.FromSteps(GAMESTATE->m_pCurSteps);
	SongID songid;
	songid.FromSong(GAMESTATE->m_pCurSong);
	if (GAMEMAN->m_bResetModifiers) {
		float oldRate = GAMEMAN->m_fPreviousRate;
		const RString mods = GAMEMAN->m_sModsToReset;
		/* // Reset mods
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetSong().FromString("clearall");
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetCurrent().FromString("clearall");
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetPreferred().FromString("clearall");
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetSong().FromString(mods);
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetCurrent().FromString(mods);
		GAMESTATE->m_pPlayerState->m_PlayerOptions.GetPreferred().FromString(mods);
		*/
		FailType failreset = GAMEMAN->m_iPreviousFail;
		GAMESTATE->m_pPlayerState
		  ->m_PlayerOptions.GetSong()
		  .m_FailType = failreset;
		GAMESTATE->m_pPlayerState
		  ->m_PlayerOptions.GetCurrent()
		  .m_FailType = failreset;
		GAMESTATE->m_pPlayerState
		  ->m_PlayerOptions.GetPreferred()
		  .m_FailType = failreset;
		GAMESTATE->m_SongOptions.GetSong().m_fMusicRate = oldRate;
		GAMESTATE->m_SongOptions.GetCurrent().m_fMusicRate = oldRate;
		GAMESTATE->m_SongOptions.GetPreferred().m_fMusicRate = oldRate;
		GAMEMAN->m_bResetModifiers = false;
		
		const vector<RString> oldturns = GAMEMAN->m_vTurnsToReset;
		if (GAMEMAN->m_bResetTurns) {
			GAMESTATE->m_pPlayerState
			  ->m_PlayerOptions.GetSong()
			  .ResetModsToStringVector(oldturns);
			GAMESTATE->m_pPlayerState
			  ->m_PlayerOptions.GetCurrent()
			  .ResetModsToStringVector(oldturns);
			GAMESTATE->m_pPlayerState
			  ->m_PlayerOptions.GetPreferred()
			  .ResetModsToStringVector(oldturns);
			GAMEMAN->m_bResetTurns = false;
			GAMEMAN->m_vTurnsToReset.clear();
		}
		GAMEMAN->m_sModsToReset = "";
		MESSAGEMAN->Broadcast("RateChanged");
	}
	StartTransitioningScreen(SM_GoToNextScreen);
}

// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the ScreenEvaluation. */
class LunaScreenEvaluation : public Luna<ScreenEvaluation>
{
  public:
	static int GetStageStats(T* p, lua_State* L)
	{
		LuaHelpers::Push(L, p->GetStageStats());
		return 1;
	}
	LunaScreenEvaluation() { ADD_METHOD(GetStageStats); }
};

LUA_REGISTER_DERIVED_CLASS(ScreenEvaluation, ScreenWithMenuElements)

// lua end

/*
 * (c) 2001-2004 Chris Danford
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
