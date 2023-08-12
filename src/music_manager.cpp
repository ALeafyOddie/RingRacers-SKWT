// DR ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
// Copyright (C) 2023      by Kart Krew.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "music_manager.hpp"
#include "music_tune.hpp"

#include "d_clisrv.h"
#include "doomtype.h"
#include "i_sound.h"
#include "i_time.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"

using namespace srb2::music;

namespace
{

// How many tics music is allowed to drift away from game
// logic.
constexpr int kResyncTics = 2;

}; // namespace

void TuneManager::tick()
{
	if (S_MusicDisabled())
	{
		return;
	}

	Tune* tune = current_tune();

	std::string old_song = current_song_;
	current_song_ = tune && tune->playing() ? tune->song : std::string{};

	bool changed = current_song_ != old_song;

	if (stop_credit_)
	{
		if (changed)
		{
			// Only stop the music credit if the song actually
			// changed.
			S_StopMusicCredit();
		}

		stop_credit_ = false;
	}

	if (!tune)
	{
		if (changed)
		{
			I_UnloadSong();
		}

		return;
	}

	if (changed)
	{
		if (load())
		{
			I_FadeInPlaySong(tune->resume ? tune->resume_fade_in : tune->fade_in, tune->loop);
			seek(tune);

			adjust_volume();
			I_SetSongSpeed(tune->speed());

			if (tune->credit && !tune->resume)
			{
				S_ShowMusicCredit();
			}

			tune->resume = true;

			gme_ = !std::strcmp(I_SongType(), "GME");
		}
		else
		{
			I_UnloadSong();
		}
	}
	else if (tune->needs_seek || (tune->sync && resync()))
	{
		seek(tune);
	}
	else if (tune->time_remaining() <= detail::msec_to_tics(tune->fade_out))
	{
		if (tune->can_fade_out)
		{
			I_FadeSong(0, tune->fade_out, nullptr);
			tune->can_fade_out = false;
		}

		if (!tune->keep_open)
		{
			tune->ending = true;
		}
	}
}

void TuneManager::pause_unpause() const
{
	const Tune* tune = current_tune();

	if (tune)
	{
		if (tune->paused())
		{
			I_PauseSong();
		}
		else
		{
			I_ResumeSong();
		}
	}
}

bool TuneManager::load() const
{
	lumpnum_t lumpnum = W_CheckNumForLongName(fmt::format("O_{}", current_song_).c_str());

	if (lumpnum == LUMPERROR)
	{
		return false;
	}

	return I_LoadSong(static_cast<char*>(W_CacheLumpNum(lumpnum, PU_MUSIC)), W_LumpLength(lumpnum));
}

void TuneManager::adjust_volume() const
{
	UINT8 i;
	const musicdef_t* def = S_FindMusicDef(current_song_.c_str(), &i);

	if (!def)
	{
		return;
	}

	I_SetCurrentSongVolume(def->debug_volume != 0 ? def->debug_volume : def->volume);
}

bool TuneManager::resync()
{
	if (gme_)
	{
		// This is dodging the problem. GME can be very slow
		// for seeking, since it (probably) just emulates the
		// entire song up to where its seeking.
		//
		// The time loss is not easily predictable, and it
		// causes repeated resyncing, so just don't sync if
		// it's GME.
		return false;
	}

	if (hu_stopped)
	{
		// The server is not sending updates. Don't resync
		// because we know game logic is not moving anyway.
		return false;
	}

	long d_local = I_GetTime() - time_local_;
	long d_sync = detail::tic_time() - time_sync_;

	if (std::abs(d_local - d_sync) >= kResyncTics)
	{
		time_sync_ = detail::tic_time();
		time_local_ = I_GetTime();

		return true;
	}

	return false;
}

void TuneManager::seek(Tune* tune)
{
	uint32_t end = I_GetSongLength();
	uint32_t loop = I_GetSongLoopPoint();

	uint32_t pos = detail::tics_to_msec(tune->seek + tune->elapsed()) * tune->speed();

	if (pos > end && (end - loop) > 0u)
	{
		pos = loop + ((pos - end) % (end - loop));
	}

	I_SetSongPosition(pos);
	tune->needs_seek = false;
}
