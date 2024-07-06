pianobar is a console client for the personalized web radio [Pandora](http://www.pandora.com) ported to Windows.

![pianobar](https://user-images.githubusercontent.com/1197433/79893843-e7af9180-8404-11ea-993e-90ed37b072c1.png)

# Releases

[![Build status](https://ci.appveyor.com/api/projects/status/6n5qa9bs7aiy8e52?svg=true)](https://ci.appveyor.com/project/thedmd/pianobar-windows)

Releases:
 * [2022.04.16](https://github.com/thedmd/pianobar-windows/releases/tag/2022.10.09) (builds: [x64](https://github.com/thedmd/pianobar-windows/releases/download/2022.10.09/pianobar-2022.10.09-x64.zip), [x86](https://github.com/thedmd/pianobar-windows/releases/download/2022.10.09/pianobar-2022.10.09-x86.zip))

Older releases can be found at [GitHub Release page](https://github.com/thedmd/pianobar-windows/releases).

### Features

* Play and manage (create, add more music, delete, rename, ...) your stations.
* Rate played songs and let pandora explain why they have been selected.
* Show upcoming songs/song history.
* Configure keybindings.
* last.fm scrobbling support (external application)
* Proxy support for listeners outside the USA.
* Execute eventcmd to script notification for song being played

### Source Code

Original source code can be downloaded at [github.com](http://github.com/PromyLOPh/pianobar/)
or [6xq.net](http://6xq.net/projects/pianobar/).

### Building

Checkout [pianobar-windows-build](https://github.com/thedmd/pianobar-windows-build) where
you will find configured solution for Visual Studio 2015.

This repository is linked by GitHub submodule.


## Configuration

Please examine `pianobar.cfg.example` for more details or ask in issue if you find something missing.


### Available actions
 - `act_help` - show act_help
 - `act_songlove` - love song
 - `act_songban` - ban song
 - `act_stationaddmusic` - add music to station
 - `act_stationcreate` - create new station
 - `act_stationdelete` - delete station
 - `act_songexplain` - explain why this song is played
 - `act_stationaddbygenre` - add genre station
 - `act_history` - song history
 - `act_songinfo` - print information about song/station
 - `act_addshared` - add shared station
 - `act_songnext` - next song
 - `act_songpausetoggle` - pause/resume playback
 - `act_songpausetoggle2` - same as above, but allow second bind
 - `act_quit` - quit
 - `act_stationrename` - rename station
 - `act_stationchange` - change station
 - `act_songtired` - tired (ban song for 1 month)
 - `act_upcoming` - upcoming songs
 - `act_stationselectquickmix` - select quickmix stations
 - `act_bookmark` - bookmark song/artist
 - `act_voldown` - decrease volume
 - `act_volup` - increase volume
 - `act_managestation` - delete seeds/feedback
 - `act_songplay` - resume playback
 - `act_songpause` - pause playback
 - `act_volreset` - reset volume
 - `act_settings` - change settings

### Hotkeys

You can now bind hotkey to an action, for example:
```
    hk_act_songlove = c + shift + ctrl
      or
    hk_act_songlove = f9
```

Hotkey use format:
    `hk_<action_name> = <key> + [modifier1] + [modifier2]...`

<key> can be any printable character on your keyboard or one of the
named keys:
```
    numpad0, numpad1, numpad2, numpad3, numpad4, numpad5, numpad6, numpad7,
    numpad8, numpad9, multiply, add, separator, subtract, decimal, divide,
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16,
    f17, f18, f19, f20, f21, f22, f23, f24, browser_back, browser_forward,
    browser_refresh, browser_stop, browser_search, browser_favorites,
    browser_home, volume_mute, volume_down, volume_up, media_next_track,
    media_prev_track, media_stop, media_play_pause, launch_mail,
    launch_media_select, launch_app1, launch_app2
```
Example bindings:
```
#hk_act_songpausetoggle = media_play_pause
#hk_act_songnext = media_next_track
```
