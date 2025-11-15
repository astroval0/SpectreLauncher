It is currently ready, but there are no backend servers consistently up right now so this is just here doing nothing.
This is the launcher for SpectreRevival. Here's how it works:

1) it checks if steam is installed (via registry keys).
2) it checks if Spectre Divide is installed (via steam vdf files).
3) it checks if the game's BEClient_x64.dll matches the one we need (if not it will clear that directory and download the one we need there. 
[the code for that dll](https://github.com/astroval0/SpectrePatcher) is also open src)
4) it checks if steam is currently running (if not it will start it automatically).
5) it checks the currently signed in steam user's steamid64.
6) if all of those are good then it will launch SpectreDivide with the correct ENVs (steamid and appid)
7) it will then wait for a specific mem page to become available, that is the signal to make a post request to the backend with the users steamid. this is needed for logging in.


if you do download, download the 1.1 release from the releases tab, simply just the EXE and run it. this is how you will play the game.
