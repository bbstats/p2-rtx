<div align="center" markdown="1">

If you want to support my work:

<a href="https://patreon.com/xoxor4d"><img src=".github/img/patreon.png" width="12%"></a>  
<a href="https://ko-fi.com/xoxor4d"><img src=".github/img/kofi.png" width="10%"></a>

</div>

<br>

------

# Installation:

### Install using the installer:
1. Download <LINK_TO_MOD_ZIP>
2. Download <LINK_TO_INSTALLER>

3. Place both files in the same folder (_no need to copy them to your game folder_) and run `Portal2-Remix-CompMod-Installer.exe` 
4. Use the File Dialog to select your `portal2.exe` which is located in your Portal2 install folder

<br>

> [!IMPORTANT]
> If you want to use DLSS 5 (NR), you'll need to source and place `nvngx_dlssnr.dll` into the `.trex` folder on your own.  
> You can then use the NR settings found within the Post-Processing tab (Alt+X - Advanced Menu) 

<br>

### OR Install manually (no need to do this if you've used the installer):
1. Download <LINK_TO_MOD_ZIP>

2. Open the zip and extract all files contained inside the `Portal2-Remix-CompatibilityMod` folder into your Portal2 directory (next to the `portal2.exe`). Overwrite all when prompted.
3. Go into your `.trex` folder and find `d3d9_runtime.zip`. Extract the content (`d3d9.dll`) into your `.trex` folder.

4. Download the [`base remix-mod`](https://github.com/xoxor4d/p2-rtx-base-mod/archive/refs/heads/master.zip) - repo: (https://github.com/xoxor4d/p2-rtx-base-mod)
5. Extract and place the `mods` folder __into__ the `rtx-remix` folder found in the Portal 2 __root__ folder  
(create the `rtx-remix` folder if it does not exist) (see [this](#folder-structure))

> [!Note]
> Only maps up until Act 2 (catapult intro) are touched up. Other maps might look weird / dark. That also includes water surfaces.
> I suggest grabbing [Digital Additions](https://www.moddb.com/mods/digital-additions) which overhauls basically the entire game.

<br>

# Usage / Running the game
- Run the game via the provided `run-p2-rtx.bat` batch file!
- Launching via Steam will **NOT** work.  
- An external console window should spawn and let you know if there are any issues.

> - Press `Alt + X` to open the Remix menu  
> - Press `F5` to open the Compatibility Mod menu (rebind or disable it in `Game Settings > Input`, or use the `xo_devgui_toggle` console command)

### Using DLSS 5
- Open the Remix menu via `Alt + X` and open the Advanced Settings Menu
- Scroll down to the Post Processing tab and find the Neural Rendering section
- Enable NR via the toggle and adjust to your liking

<br>

> [!Important]
> **Usage Info / Guides** -- Look into the **Wiki**:  
> https://github.com/xoxor4d/p2-rtx/wiki  

<br>

## <a id="folder-structure"></a>Folder Structure
</div>

```
.  
├─ ...
├─ 📁 steamapps
│  └─📁 common
│     └─📁 Portal 2
│       ├── 📜 run-p2-rtx.bat
│       ├── 📜 portal2.exe
│       ├── 📜 p2-rtx-disable.bat
│       ├── 📜 p2-rtx.dll
│       ├── 📜 ...
│       │
│       ├── 📁 bin
│       │   └─📁 .trex
│       │
│       ├── 📁 portal2_dlc2
│       ├── 📁 portal2_dlc3
│       ├── 📁 ...
│       └── 📁 rtx-remix
│           └─📁 mods
│             └─📁 portal2rtx
│               ├── 📜 _compatibility.usda
│               ├── 📜 _highquality.usda
│               └── ...
└── ...  
```

<br>
<br>

----

## ✳️ Info:
- Current releases ship with a custom build of the [remix-dxvk runtime](https://github.com/xoxor4d/dxvk-remix/tree/game/p2) including necessary changes for Portal 2
- Some engine tweaks required to make the game compatible with RTX Remix result in CPU bottlenecks on some maps (software skinning instead of HW skinning, drawcall amount). This may or may not improve in future updates.
- There are clever ways to optimize many aspects of this game but that takes time and lots of effort.