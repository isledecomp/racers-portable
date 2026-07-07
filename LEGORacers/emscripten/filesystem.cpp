// [library:filesystem] Web data delivery: stream the game archive, music and movies over
// HTTP ranges via the WASMFS fetch backend, and persist saves + the renderer preference
// in the browser's OPFS so they survive reloads. Modeled on isle-portable
// (ISLE/emscripten/filesystem.cpp), adapted to racers' flat, case-folded data set.
//
// The fetch backend cannot enumerate a directory, so case resolution does NOT use
// readdir: each file is registered (correct on-disk case) with miniwin, and
// MiniwinResolvePath maps the game's case-insensitive opens ("hvscmp.avi") to the
// registered absolute path ("/racers/HVSCmp.avi") -- the case the server actually hosts.

#include "emscripten/filesystem.h"

#include <SDL3/SDL_log.h>
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <miniwin/windows.h>
#include <string>

// Game data files streamed from the host, in their real on-disk (hosted) case.
static const char* const g_streamedFiles[] = {
	"LEGO.JAM",     "introcmp.avi", "HVSCmp.avi",   "lmicmp.avi",   "1st.tun",      "2nd.tun",      "3rd.tun",
	"alien.tun",    "amazon.tun",   "armor.tun",    "bonus.tun",    "builder.tun",  "cannon.tun",   "Circuit1.tun",
	"Circuit2.tun", "Circuit3.tun", "Circuit4.tun", "Circuit5.tun", "Circuit6.tun", "credits.tun",  "docks.tun",
	"forest.tun",   "GameIntr.tun", "gencar.tun",   "gold.tun",     "hook.tun",     "HvsIntro.tun", "ice.tun",
	"island.tun",   "knight.tun",   "Lose.tun",     "luck.tun",     "magma.tun",    "meteor.tun",   "moat.tun",
	"parrot.tun",   "pyrmid.tun",   "reward.tun",   "rocket.tun",   "royal.tun",    "RRacer.tun",   "RRCar.tun",
	"skull.tun",    "sphinx.tun",   "stars.tun",    "start.tun",    "test.tun",     "theme.tun",    "tomb.tun",
	"ufo.tun",      "VVCar.tun",    "whip.tun",     "win.tun",      "witch.tun",
};

// Create every ancestor directory of p_path on the given backend (mkdir -p).
static void CreateDirChain(backend_t p_backend, const char* p_path)
{
	std::string path = p_path;
	for (size_t i = 1; i < path.size(); i++) {
		if (path[i] == '/') {
			path[i] = '\0';
			wasmfs_create_directory(path.c_str(), 0755, p_backend);
			path[i] = '/';
		}
	}
	wasmfs_create_directory(path.c_str(), 0755, p_backend);
}

void Racers_SetupWebFilesystem()
{
	// --- Game data: HTTP-range streaming via the WASMFS fetch backend ---
	std::string baseUrl = std::string(Racers_streamHost) + Racers_dataMount;
	backend_t fetchfs = wasmfs_create_fetch_backend(baseUrl.c_str(), 512 * 1024);

	wasmfs_create_directory(Racers_dataMount, 0755, fetchfs);
	for (const char* name : g_streamedFiles) {
		std::string path = std::string(Racers_dataMount) + "/" + name;
		wasmfs_create_file(path.c_str(), 0644, fetchfs);
		// Register the correct-case absolute path so the game's case-insensitive data-file
		// opens resolve to it (and thus to the correctly-cased fetch URL).
		MiniwinRegisterWebDataFile(path.c_str());
	}

	// --- User data: saves + renderer preference persisted in OPFS ---
	if (MAIN_THREAD_EM_ASM_INT({ return !!Module["disableOpfs"]; })) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "web: OPFS unavailable; saves will not persist");
		return;
	}

	backend_t opfs = wasmfs_create_opfs_backend();
	CreateDirChain(opfs, Racers_userDataMount);
}
