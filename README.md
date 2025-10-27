This is the launcher for SpectreRevival. Here's how it works:

1) it checks if steam is installed (via registry keys).
2) it checks if Spectre Divide is installed (via steam vdf files).
3) it checks if the game's BEClient_x64.dll matches the one we need (if not it will clear that directory and download the one we need there. [the code for that dll](https://github.com/astroval0/SpectrePatcher) is also open)
4) it checks if steam is currently running (if not it will start it automatically).
5) it checks the currently signed in steam user's steamid64.
6) if all of those are good then it will launch SpectreDivide with the correct ENVs (steamid and appid)
