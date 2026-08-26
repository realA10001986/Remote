/*  
 *  Release checklist:
 *  - New soundpack? Delete old on github before uploading!
 *  - Changes in src sub-directory? (WM? Audio-Lib? SD?)
 *  - Any files deleted? Need to remove from github first, before uploading
 *  - ChangeLog in .ino up-to-date?
 *  - REMOTE_DBG in remote_global.h?
 *  - global.h up-to-date (date, version number)?
 *  - Documentation up-to-date?
 *  - Cheatsheet up-to-date?
 *  - Update info via gandi
 *  
 *  --------------------------------------------------------
 *  mp3s in sound pack need "info" frame removed.
 *  mp3s in SC/TC do not, buildXc takes care of this.
 *  --------------------------------------------------------
 *   
 *  Changelog
 *
 *  2026/08/23 (A10001986)
 *    - Turn CSF_BUSY and CFS_BLOCKSCAN into ints to allow nesting
 *    - Set remBusy during boot to block incoming timetravel triggers while we are busy.
 *    - Adapt mqtt notifications to bttfn version. MQTT-ABORT also was set while TCDINP0 
 *      which is incorrect. Where did that come from?
 *    - BTTFN/MQTT-REENTRY: Set only if CSF_TT is set; otherwise we are somewhere
 *      between networkTimetravel=true and csf |= CSF_TT, and cancel time travel
 *      as we obviously missed picking up on networkTimetravel in time.
 *  2026/08/20 (A10001986)
 *    - Replace all speech files from fakeyou with new ones from 101soundboards. Those are
 *      much clearer (albeit lack Doc's "enthusiasm")
 *  2026/08/19 (A10001986)
 *    - Problem: If remote is fake-off when "time travel" notification is sent, it ignores 
 *      it (no "networktimetravel"). However, it enters "TCDINP0" mode as speed source is P0. 
 *      The Remote then counts up to 87mph, stops, and does nothing while the TCD performs 
 *      its TT. This is not desirable. In that case, the Remote should ignore P0 as well, 
 *      and simply wait for the TCD to finish its business.
 *      (Reason for stopping at 87 is because 88 is no longer from "speed source" P0 but P1). 
 *      Solution: TTMISSED flag, set if a TT notification (BTTFN & MQTT) arrives while fake-
 *      off. Flag blocks P0 speed to trigger TCDINP0. Flag cleared on TT-abort, TT-reentry,
 *      and when speed source is other than P0.
 *  2026/08/15 (A10001986)
 *    - New sound-pack (RM13) - so far no new sounds. Reasons:
 *    - Strip ID3 headers from all files in sound-pack
 *    - Strip first frame (LAME/Xing info frame) from all files in sound-pack.
 *      Libmad doesn't support it (and instead, allegedly, inserts a pause of 1152 samples)
 *    - audio: Skip ID3-check for files on flash FS, spares us a read and a seek
 *  2026/08/12 (A10001986)
 *    - Rename all variables containing "GPS" to "TCDS" since that what it means (not only
 *      GPS but also RotEnc and Remote)
 *  2026/08/11 (A10001986)
 *    - Put code to delete old files in remoteObsFiles()
 *  2026/08/09 (A10001986)
 *    - Change default volume from 6 to 8
 *  2026/08/02 (A10001986)
 *    - Increase SD SPI frequency to 20Mhz for the Remote.
 *  2026/07/20 (A10001986)
 *    - moveSettings: Insert obviously missing "return" inside "if(configSD && FlashROMode){}"
 *    - Re-order settings.cpp like TCD's version
 *  2026/07/18 (A10001986)
 *    - Check for 0-length JSON config file; proper error-handling.
 *    - Binary config files must be at least 2 bytes in size
 *  2026/07/17 (A10001986)
 *    - Bugfix: Avoid crash on 0-length config files
 *    - SD: Do two attempts on CRC_ON_OFF (like current IDF)
 *    - SD: Put power-up-sequence outside two-attempt-loop of GO_IDLE_STATE
 *  2026/07/15 (A10001986) [1.25] ----------------------------------------
 *    - Include SD library into project source tree. 
 *    - SD: Change parameter for APP_OP_COND (ACMD41) from 0x40100000 to 0x40000000 
 *      (and 0x00100000 to 0 for SDSC and MMC) to conform to SPI mode specs. This
 *      fixes the Sandisk Ultra 32GB problem.
 *    - SD: Do two attempts to put the card in SPI mode. This makes detection and 
 *      initialization for Intenso cards more rebust.
 *  2026/07/14 (A10001986)
 *    - Attempt to solve SanDisk Ultra 32 GB SD card problem by setting CS to high
 *      ahead of SPI.begin - albeit unsuccessful.
 *  2026/07/11 (A10001986)
 *    - Remove SETTINGS_TRANSITION stuff, was disabled anyway, and it's time.
 *      (SETTINGS_TRANSITION_2 still there, and will be for a while)
 *  2026/07/09 (A10001986)
 *    - Allow choosing between playing throttle-up sound at any speed, or just 0 (as 
 *      before)
 *  2026/07/08 (A10001986)
 *    - P0: Fix playing throttle-up-sound (wasn't played because p0 speed was 1 and not
 *      0 when executing the code; now we play it (once) if P0 is < 5mph at the start
 *      of P0.
 *  2026/06/26 (A10001986)
 *    - Change "song" to "track" in CP and in comments
 *  2026/06/24 (A10001986) [1.24] ----------------------------------------
 *    - Pre-scan music folders and display state in CP
 *    - Take mp_checkForFolder from TCD, was smarter there
 *  2026/06/23 (A10001986)
 *    - Force MP into "off" state ahead of all reboots (fw-update, carMode-switch,
 *      saving settings, ...)
 *  2026/06/21 (A10001986)
 *    - 888/888xxx now play regardless of mpActive; no point in "go to XXX" and not
 *      start playing.
 *  2026/06/20 (A10001986)
 *    - Remove SPIFFS remnants
 *  2026/06/19 (A10001986)
 *    - Disabled MQTT/MP backchannel stuff. Don't want to taint the Remote with even more
 *      network traffic. 
 *      Missing from implementation is: Calling mp_sendStatus on actual state changes 
 *      (off, P0, ...)
 *    - MQTT/MP: Check connection state before even building the status string
 *  2026/06/18 (A10001986)
 *    - Introduce csf and convert many bools to flags
 *    - MQTT/MP: !haveMusic now means "off" in backchannel
 *  2026/06/17 (A10001986)
 *    - Port audio changes (aud_state) to remote
 *  2026/06/15 (A10001986)
 *    - MP/MQTT: Extend status to show "O" if off or unavailable; "I" is now "idle" and
 *      used instead of "S".
 *    - MQTT: Add "VOLUME_SET_" command
 *  2026/06/13 (A10001986)
 *    - Change back delay() ahead of esp_restart to 1000 to make sure network
 *      transmissions are completed.
 *  2026/06/12 (A10001986)
 *    - Audio/Renamer: Switch from QuickSort to InsertionSort. QuickSort causes
 *      GURUs due to excessive recursion levels on large, mostly already sorted
 *      arrays
 *    - Import MQTT/Music Player changes from TCD (MQTT commands, volume levels, ...)
 *  2026/05/16 (A10001986)
 *    - Import Audio changes from TCD (audioLogger)
 *  2026/05/05 (A10001986)
 *    - Throttle on new board is ADC input #3
 *    - Sync CRSF from John
 *  2026/04/25 (A10001986) [1.23] ----------------------------------------
 *    - Eval TCD SSID-appendix and pw-marker sent in NOT-DATA and display in CP
 *  2026/04/24 (A10001986)
 *    - Behavior of Calibration button changed: Press is still a brief press. Holding
 *      the button to initiate Calibration (Fake power off), or to display IP address
 *      and battery state (Fake power on) means holding it until a double beep sounds, 
 *      then the button must be released. (Previously, the function to trigger started
 *      automatically after 2 seconds, without releasing the button, and no sound was 
 *      played.)
 *    - Holding Calib button for 6 seconds toggles car mode
 *    - New sound-pack (RM12)
 *  2026/04/23 (A10001986)
 *    - Add Car Mode: Enter TCD-AP SSID (usually TCD-AP) and password in WiFi Settings, 
 *      and you can switch between normal WiFi connection and Car (TCD) WiFi connection
 *      by <<<<TODO>>>>, or through the Config Portal.
 *    - Add validity check for IP address returned for update version
 *  2026/04/15 (A10001986) [1.22] ----------------------------------------
 *    - Add option to stay in AP mode (and not to attempt to connect to WiFi
 *      network) in CRSF mode.
 *    - Add FPOnWiFiHandler callback to re-enable WiFi on Fake-Power-On in
 *      CRSF mode.
 *  2026/04/09 (A10001986)
 *    - Add option to have holding RESET toggle auto-throttle instead of
 *      shuffle
 *  2026/04/04 (A10001986)
 *    - Minor optimization in display code (setSpeed)
 *  2026/04/03 (A10001986)
 *    - Allow assigning a button to send "REFILL" to Dash Gauges (TCD 3.22)
 *  2026/03/31 (A10001986)
 *    - Wifi: Make getServerParm more robust with limit-checks
 *  2026/03/29 (A10001986)
 *    - Change license to disallow removing/changing links to source
 *    - Make "Dr. Brown" graphics a link to circuitsetup
 *  2026/03/26 (A10001986) [1.21] ----------------------------------------
 *    - Check for Update after WiFi reconnection
 *    - Prepare for board version detection (Pins unknown yet, hence inop)
 *    - CRSF: Disable/skip everything CRSF-related if wrong board version detected
 *  2026/03/25 (A10001986)
 *    - WiFi: Optimize SelectBox and RadioButton builder
 *  2026/03/24 (A10001986)
 *    - Integrate preliminary CRSF code
 *  2026/03/23 (A10001986)
 *    - WiFi: Redo "shouldXXX" into a flag based system
 *    - WM: Kick out low-res QI icons. The browser does well at scaling by itself.
 *  2026/03/19 (A10001986)
 *    - Update: Animate "telephone" in Update button, remove mere notice at bottom
 *    - Update: Save new version in secSettings for displaying update notice even
 *      when later changing to AP mode.
 *  2026/03/16 (A10001986)
 *    - Add firmware update link to upload page
 *    - WM: More thriftly select which scripts and styles are required for each page
 *    - WiFi: Redo main menu footer (all-small-caps does not shrink digits in Windows)
 *  2026/03/15 (A10001986)
 *    - Spinner animation: Add 1.5s delay so that the spinner does not flash on
 *      each page load, but only if loading takes longer.
 *      Note: Safari on MacOS does not start delayed animations on page-loads.
 *      Adding 
 *      "@supports(hanging-punctuation:first)and(font:-apple-system-body)and(-webkit-appearance:none){img.sp{animation-delay:0s}}"
 *      would remove the animation-delay for Safari, but only helps on MacOS, 
 *      because Safari on iOS stops animations on page-load, even ones without delay.
 *      So I decided not to care about Safari.
 *  2026/03/14 (A10001986)
 *    - WM: Unify page structure (all have wrap(h1,h3) now), and simplyfy JS
 *      in xxx_wifi.cpp accordingly. Saves 750 bytes of bin size.
 *  2026/03/13 (A10001986)
 *    - Fix number of "brake" sounds
 *    - CP: Add spinner
 *  2026/03/12 (A10001986)
 *    - WM: Send "no-cache" headers for all settings pages.
 *    - CP/WM: Attempt to implement a page-restore upon browser-back for our
 *      "please wait" buttons. Works with Chrome and Safari, and basically
 *      with FF. However, Firefox ignores ::first-letter when setting innerHTML,
 *      (or innerText or textContent) so we cannot allow to cache the main menu,
 *      otherwise a click on the back button shows a "damaged" button where the
 *      styling is missing.
 *  2026/03/11 (A10001986)
 *    - millisNonZero: do -- instead of ++ to avoid subsequent incorrect timeout
 *      checks; some more millis()->millisNonZero() changes (mqtt, bttfn)
 *  2026/03/02 (A10001986)
 *    - WM: When disabling WiFi in AP mode, wait for AP_START event, not AP-STOP.
 *      Explanation in code.
 *    - WiFi: Disable AP for power-save only if there are no stations connected.
 *  2026/03/01 (A10001986)
 *    - WM: Initialize all callback pointers to NULL (!!!)
 *    - WM: Re-order functions for more ... order, and in the process rename some
 *      of them for even more ... order.
 *  2026/02/27 (A10001986)
 *    - Fix: If millis() are also used as a flag, they must not be zero. Hence
 *      millisNonZero(), and respective fixes in WM.
 *  2026/02/25 (A10001986)
 *    - WM: Move softAPsetHostname upwards before softAP().
 *    - WM: Show BSSID in footer regardless of given BSSID; show BSSID as tooltip
 *      to avoid visual errors if SSID is too long
 *  2026/02/23-24 (A10001986)
 *    - WM: Various fixes: timeout (millis() <!), unnecessary delay ahead of
 *      WiFi.begin(); mostly unnecessary delay in retry-loop; add DHCP-timeout
 *      error message; display BSSID in reportStatus; etc.
 *    - Remove Wifi Connection Timeout setting, don't think this is really required
 *  2026/02/22 (A10001986)
 *    - WiFi: Allow defining a BSSID (AP MAC address) to connect to a specific AP
 *      if multiple APs with identical SSID are available.
 *    - WM: Put inline scripts on Upload page into optionally added script section
 *  2026/02/21 (A10001986)
 *    - CP: Build text input fields for MQTT topics/messages dynamically, are no
 *      longer a cemetary of "WiFiMananagerParameter"s.
 *  2026/02/20 (A10001986)
 *    - wifi/settings: Shrink settings to required size (verified with WiFiParam)
 *    - WM: Add "setValue()" without length, for keeping old length
 *    - WiFi: Move storing parms using getServerParam() into saveParamsCallback()
 *    - Filter out single quote from some MQTT strings in WiFi save
 *    - MQTT: Disable MQTT if server can't be resolved; show proper error msg.
 *  2026/02/19 (A10001986)
 *    - Static IP: Remove strcmp-check in preSaveWiFiCallback(), rely on iphash
 *    - Minor IP settings change (strlen(ips.ip) -> *ipsettings.ip)
 *    - WM: IpAddress& instead of duplicate on setStatic() 
 *  2026/02/15 (A10001986) [1.20] ----------------------------------------
 *    - Add optional update notification at boot; 7053281 to toggle, also
 *      selectable in Config Portal.
 *    - loadConfigFile(): Return true regardless of length of valid data;
 *      old comparison meant we ignored the config file entirely.
 *    - MQTT: Fix buffer length check in subscribe (topic2)
 *  2026/02/14 (A10001986)
 *    - Remove "install sound-pack" banner on Settings pages
 *    - Show required sound-pack version in main page banner
 *    - Fix shuffle changes when there is no music (do not check haveMusic
 *      before calling makeshuffle).
 *  2026/02/13 (A10001986)
 *    - Add firmware update check & notification
 *  2026/02/12 (A10001986)
 *    - Reformat FS not depending on free space but rather whether a config
 *      file is present.
 *  2026/02/10 (A10001986)
 *    - Make shuffle persistent (and remove from CP settings) (SD required)
 *    - Convert most JSON settings to bin files.
 *    - There are 2 categories of formerly "secondary" settings: 1) secondary 
 *      (dep on ConfigOnSD), and 2) tertiary (SD only).
 *    - Remove MovieMode and displayGPS from visMode, and save those as sec,
 *      visMode as ter setting.
 *    - Settings: Re-do reformatting of Flash FS in case of audio-copy-error
 *    - Settings: Re-do moving settings from/to SD if user changes option. Files
 *      on "old" medium are now deleted.
 *    - Only save settings if actually changed.
 *  2026/02/03 (A10001986)
 *    - audio/haveKeySnd: Convert array of bool to int32_t bitmask
 *  2026/01/30 (A10001986)
 *    - WM: Fix param generation with SECTS/FOOT at a NULL-custom param.
 *  2026/01/28 (A10001986)
 *    - Comment-out sending combined packet if powerMaster when switching fake
 *      power on. Why was that there??? A packet is sent anyway (few lines down)
 *    - Audio/WAVGenerator: Add freeBuf() where applicable
 *    - Display: Put max_bufPos in array instead of looking at bufPosArr[].
 *  2026/01/26 (A10001986)
 *    - WM: Overrule "length" of checkbox params by "1".
 *    - WM: Build sections using flags instead of extra parms
 *  2026/01/25 (A10001986)
 *    - Once and for all fix checkboxes: Kill hack for "checked"; WM gets
 *      values "0" and "1" for unset/set, converts it into "checked" (or
 *      not), and delivers back "0" or "1" upon save.
 *    - WM: Fix "NO_LABEL" params: Have no label; avoid strlen(NULL)
 *    - Simplify/compact updateConfigPortalVisValues
 *    - Replace (atoi(setting) > 0) by evalBool
 *    - Audio: Pointless, but anyway: Upsample WAV 8bit to 16bit
 *  2026/01/22 (A10001986)
 *    - Update lastBTTFNpacket when returning to polling to avoid an immediate
 *      return to "stand-alone" in main_loop().
 *  2026/01/17 (A10001986)
 *    - HTML size reductions (wifi+WM)
 *  2026/01/14 (A10001986)
 *    - Import WM changes from TCD (wmBuildXXX with op)
 *  2026/01/09 (A10001986) [1.16] -----------------------------------------
 *    - BTTFN: Fall back to DISCOVER upon NOT_DATA timeout. TCD might have 
 *      got a new IP address.
 *    - BTTFN: Fix re-connection after a TCD-reboot.
 *  2026/01/09 (A10001986) [1.15] -----------------------------------------
 *    - WM: Add parameter to disabledWiFi() to wait for WIFI_OFF; avoids
 *      run-time errors.
 *  2026/01/04 (A10001986)
 *    - Refinements for NOT_DATA
 *  2026/01/03 (A10001986)
 *    - Add support for NOT_DATA, which eliminates the need to poll the TCD
 *      for data, and thereby reduces network traffic.
 *  2026/01/02 (A10001986)
 *    - Change BTTFN_NOT_BUSY to more apt BTTFN_NOT_INFO
 *  2026/01/01 (A10001986)
 *    - P0: Don't jump to TCD speed if it's lower than our currently displayed 
 *      one, instead wait until TCD caught up.
 *    - Issue a brake warning if brake is on when throttle-up TT should be
 *      triggered.
 *  2025/12/30 (A10001986)
 *    - Remove obsolete audio files from flash upon power-up.
 *  2025/12/26 (A10001986)
 *    - Cleanup: Silence compiler warnings; properly mark float literals
 *  2025/12/22 (A10001986)
 *    - Audio: Some further optimizations
 *  2025/12/21 (A10001986)
 *    - New sound-pack (RM10): Timetravel sounds changed.
 *    - Audio: Some speed optimizations
 *  2025/12/20 (A10001986)
 *    - Audio: StartPos simplification for WAV loop playback; add beginQuick 
 *      method for wav playback
 *  2025/12/19 (A10001986)
 *    - Allow uint32* casts to unaligned data on ESP32 since it supports unaligned
 *      memory access without any penalty (GET32/SET32 macros)
 *  2025/11/29 (A10001986) [1.14] [requires TCD 3.10 for proper operation] -----------------------------------------
 *    - Make TCD<->Remote communication more robust; timeout for throttle-
 *      triggered BTTFN-TT; stand-alone TT-on-throttle only if not connected 
 *      to TCD; timeout for brake warning; fix fake-power-off during TT
 *  2025/11/28 (A10001986)
 *    - P0: Fix speed-jumps on brake (Remote and TCD firmwares)
 *  2025/11/24 (A10001986)
 *    - Add internal command to react on brake-prohibited TT on behalf of the TCD
 *    - Add audio flag "no interrupt" 
 *    - Signal TCD when we want speed even when off; this flag is used on the
 *      TCD to accept the Remote as speedo-replacement even when not currently
 *      speedmaster (= when fake off), and do TT sequences with speedo parts
 *    - New sound pack (RM09): Re-do some sounds, add new.
 *  2025/11/22 (A10001986)
 *    - TCD-controlled TT: If brake is hit while accelerating, TT is aborted
 *    - Fix IP & SOC not being displayed while fake-off
 *  2025/11/21 (A10001986)
 *    - WM: Minor HTML tweaks; make page width dynamic for better display on
 *      handheld devices
 *  2025/11/19 (A10001986)
 *    - Add support for MQTT v5.0 (tested with mosquitto only). Has no advantages
 *      over 3.1.1 (but more overhead), only there to use brokers that lack
 *      support for 3.1.1.
 *    - Add MQTT connection state info on HA/MQTT Settings page
 *  2025/11/17 (A10001986)
 *    - If remote controlling is not allowed by the TCD, the Remote, unlike the
 *      other props, does not take part in BTTFN-wide time travel. It can, however, 
 *      still trigger a BTTFN-wide TT.
 *  2025/11/16 (A10001986)
 *    - WM: Require HTTP_POST for params save pages. Also, check if request 
 *      has parameter, do not overwrite current value with null (protects from 
 *      overwriting settings by errorneous page reloads)
 *  2025/11/14 (A10001986) [1.13] -----------------------------------------
 *    - Remove BTTFN_MC conditional
 *  2025/11/13 (A10001986)
 *    - Move Music Player init into main_setup()
 *    - Allow using TCD command codes 7xxx with MQTT-INJECT command
 *    - Block BTTFN/MQTT commands while busy
 *  2025/11/12 (A10001986)
 *    - Finish stand-alone TT (count-up version)
 *    - Cancel tcdIsInP0 if networkAbort
 *    - Skip button actions while TTrunning or tcdIsInP0
 *  2025/11/11 (A10001986)
 *    - New sound pack (RM08)
 *    - Add stand-alone time-travel sequence; triggered through O.O + throttle up
 *      or hitting 88 through throttle-up when not connected to a TCD.
 *    - Support "stalled P0" (TCD 3.9)
 *  2025/11/10 (A10001986)
 *    - Fixes for MQTT inter-prop communication; add support for MQTT TIMETRAVEL 
 *      command with variable lead (TCD 3.9)
 *  2025/11/09 (A10001986)
 *    - Add BTTFN_NOT_BUSY support (TCD 3.9)
 *  2025/11/08 (A10001986)
 *    - Allocate MQTT buffer only if MQTT is actually used
 *  2025/11/07 (A10001986)
 *    - MP3/File-Renamer: Ignore non-mp3 files; display number of files-to-do while
 *      renaming
 *    - Remove hack to skip web handling in on mp3-playback start, remove stopping
 *      sound in AP mode on CP access.
 *  2025/11/05 (A10001986)
 *    - Add MQTT command "INJECT_"
 *  2025/11/04 (A10001986)
 *    - Breaking change: Eliminated historical limitation of not having "key8.mp3". 
 *      User button #8 now plays key8, not key9 as before! Please re-upload
 *      your previous key9(l).mp3 as key8.
 *      Key9 can be played through HA/MQTT and TCD-commands.
 *  2025/11/03 (A10001986)
 *    - Make separate option for Level Meter on real or fake power
 *    - Add MQTT commands PLAYKEY_x (x=1-9), STOPKEY, AUTOTHROTTLE_xxx, COASTING_xxx,
 *      MOVIEACCEL_xxx, DISPTCDSPD_xxx.
 *    - Add TCD commands 7501-7509 (7511-7519) to play keyX.mp3 (keyXL.mp3) (x=1-9)
 *      (Old short-hands 700x for a subset of keyX playback remain as well.)
 *  2025/11/02 (A10001986) [1.12] -----------------------------------------
 *    - WM: Generate HTML for checkboxes on-the-fly.
 *  2025/11/01 (A10001986)
 *    - Tweak coasting and deceleration curves
 *    - No calibration when battery is low
 *    - Play sound when taking over/releasing TCD fake-power control
 *    - New sound pack (RM07)
 *  2025/10/31 (A10001986)
 *    - Move HA/MQTT settings to separate page in Config Portal; also, HA/MQTT
 *      settings are in separate file now.
 *    - Do not change bri/vol on first press, just display/show
 *  2025/10/30 (A10001986)
 *    - Add CP option to use O.O/RESET (when fake power is off) to either control
 *      brightness (as before) or take over/release TCD Fake-Power control (like 
 *      7095 command from TCD).
 *    - Fix deleting a bad .bin file after upload
 *    - Remove volume settings from Settings page
 *  2025/10/29 (A10001986)
 *    - Add "Remote fake power controls TCD fake power" option feature. Can be 
 *      toggled at any time using 7096 command from the TCD (which, however, 
 *      needs to be fake-on to accept commands).
 *      Requires TCD firmware >= 3.8.
 *    - Make coasting deceleration more realistic
 *  2025/10/28 (A10001986)
 *    - Allow coasting to be toggled by TCD command (7063)
 *  2025/10/27 (A10001986) [1.11.4] -----------------------------------------
 *    - Make reactivation of AP after power-save and reconnection attempts
 *      on Fake Power optional.
 *    - Fix display of configured power save timer value in CP
 *  2025/10/26 (A10001986) [1.11.3] -----------------------------------------
 *    - BTTFN: Fix hostname length issues; code optimizations; minor fix for mc 
 *      notifications. Breaks support for TCD firmwares < 3.2.
 *      Recommend to update all props' firmwares for similar fixes.
 *  2025/10/24 (A10001986) [1.11.2] -----------------------------------------
 *    - Add WiFi power saving for AP-mode, and user-triggered WiFi connect retry. 
 *      Fake power off, then on to 1) restart WiFi after entering PS mode, 2)
 *      trigger a connection attempt if configured WiFi could not be connected
 *      to during boot.
 *    - WM: Fix AP shutdown; handle mDNS
 *  2025/10/21 (A10001986) [1.11.1] -----------------------------------------
 *    - Reverse Enable/disable click sound commands for uniformity
 *    - HAVE_VOL_ROTENC compile-time option for RotEnc/Volume support
 *      (off in official builds)
 *  2025/10/17 (A10001986) [1.11] -----------------------------------------
 *    - Wipe flash FS if alien VER found; in case no VER is present, check
 *      available space for audio files, and wipe if not enough.
 *  2025/10/16 (A10001986)
 *    - Minor code optim (settings)
 *    - WM: More event-based waiting instead of delays
 *  2025/10/15 (A10001986)
 *    - Some more WM changes. Number of scanned networks listed is now restricted in 
 *      order not to run out of memory.
 *  2025/10/14 (A10001986) [1.10.2] -----------------------------------------
 *    - WM: Do not garble UTF8 SSID; skip SSIDs with non-printable characters
 *    - Fix regression in CP ("show password")
 *  2025/10/13 (A10001986)
 *    - Config Portal: Minor restyling (message boxes)
 *  2025/10/11 (A10001986) [1.10.1] -----------------------------------------
 *    - More WM changes: Simplify "Forget" using a checkbox; redo signal quality
 *      assessment; remove over-engineered WM debug stuff.
 *  2025/10/08 (A10001986)   
 *    - WM: Set "world safe" country info, limiting choices to 11 channels
 *    - WM: Add "show all", add channel info (when all are shown) and
 *      proposed AP WiFi channel on WiFi Configuration page.
 *    - Experimental: Change bttfn_checkmc() to return true as long as 
 *      a packet was received (as opposed to false if a packet was received
 *      but not for us, malformed, etc). Also, change the max packet counter
 *      in bttfn_loop(_quick)() from 10 to 100 to get more piled-up old 
 *      packets out of the way.
 *    - WM: Use events when connecting, instead of delays
 *  2025/10/07 (A10001986) [1.10] -----------------------------------------
 *    - Add emergency firmware update via SD (for dev purposes)
 *    - WM fixes (Upload, etc)
 *  2025/10/06 (A10001986)
 *    - WM: Skip setting static IP params in Save
 *    - Add "No SD present" banner in Config Portal if no SD present
 *  2025/10/05 (A10001986)
 *    - CP: Show msg instead of upload file input if no sd card is present
 *  2025/10/03-05 (A10001986) [1.09] -----------------------------------------
 *    - More WiFiManager changes. We no longer use NVS-stored WiFi configs, 
 *      all is managed by our own settings. (No details are known, but it
 *      appears as if the core saves some data to NVS on every reboot, this
 *      is totally not needed for our purposes, nor in the interest of 
 *      flash longevity.)
 *    - Save static IP only if changed
 *    - Disable MQTT when connected to "TCD-AP"
 *    - Let DNS server in AP mode only resolve our domain (hostname)
 *  2025/09/22-10/03 (A10001986)
 *    - WiFi Manager overhaul; many changes to Config Portal.
 *      WiFi-related settings moved to WiFi Configuration page.
 *      Note: If the Remote is in AP-mode, mp3 playback will be stopped when
 *      accessing Config Portal web pages from now on.
 *      This had lead to sound stutter and incomplete page loads in the past.
 *    - Various code optimizations to minimize code size and used RAM
 *  2025/09/22 (A10001986) [1.08] -----------------------------------------
 *    - Config Portal: Re-order settings; remove non-checkbox-code
 *    - Fix TCD hostname length field
 *  2025/09/20 (A10001986)
 *    - Config Portal: Add "install sound pack" banner to main menu
 *    - Remove HAVE_AUDIO conditional
 *  2025/09/19 (A10001986) [1.07] -----------------------------------------
 *    - Extend mp3 upload by allowing multiple (max 16) mp3 files to be uploaded
 *      at once. The REMA.bin file can be uploaded at the same time as well.
 *  2025/09/17 (A10001986)
 *    - WiFi Manager: Reduce page size by removing "quality icon" styles where
 *      not needed.
 *  2025/09/15 (A10001986) [1.06] -----------------------------------------
 *    - Refine mp3 upload facility; allow deleting files from SD by prefixing
 *      filename with "delete-".
 *    - WiFi manager: Remove lots of <br> tags; makes Safari display the
 *      pages better.
 *  2025/09/14 (A10001986)
 *    - Allow uploading .mp3 files to SD through config portal. Uses the same
 *      interface as audio container upload. Files are stored in the root
 *      folder of the SD; hence not suitable for music player.
 *    - WiFi manager: Remove (ie skip compilation of) unused code
 *    - WiFi manager: Add callback to Erase WiFi settings, before reboot
 *    - WiFi manager: Build param page with fixed size string to avoid memory 
 *      fragmentation; add functions to calculate String size beforehand.
 *  2025/02/13 (A10001986) [1.05.1] -----------------------------------------
 *    - Delete temp file after audio installation
 *  2025/01/26 (A10001986) [1.05] -----------------------------------------
 *    - Display battery charge level instead of "Wait" during cold start
 *      (if available).
 *  2025/01/15 (A10001986)
 *    - Minor audio code changes
 *  2025/01/13 (A10001986) [1.03] -----------------------------------------
 *    - BTTFN: Minor code optimization
 *    - Fix MQTT message length
 *  2024/12/18-19 (A10001986) [1.02] -----------------------------------------
 *    - Fixes for LC709204F battery fuel gauge IC support
 *  2024/11/22 (A10001986) [1.01] -----------------------------------------
 *    - Add (untested) support for LC709204F battery fuel gauge IC
 *  2024/11/16-20 (A10001986) 
 *    - Prepare for Battery Monitoring by adding "battery low" warning 
 *      mechanism.
 *  2024/11/14 (A10001986) [1.00] -----------------------------------------
 *    - Audio: Switch "personality" - new sound-pack.
 *  2024/11/13 (A10001986)
 *    - Calibration: Longer delay when using Level Meter or Power LED
 *    - Add audio notifications for unsuccessful button presses, as well
 *      as shuffle on/off
 *    - New sound-pack
 *  2024/11/08 (A10001986)
 *    - Fix some typos
 *  2024/10/26 (A10001986) [0.90] -----------------------------------------
 *    - Add support for TCD multicast notifications: This brings more immediate speed 
 *      updates (no more polling; TCD sends out speed info when appropriate), and 
 *      less network traffic in time travel sequences.
 *  2024/10/24 (A10001986)
 *    - Allow triggering BTTFN-wide TT via O.O followed by throttle-up; button function
 *      controlled by option (TT vs. MP/prev.song)
 *  2024/10/23 (A10001986)
 *    - Add sound played upon volume change (new sound pack)
 *  2024/10/10 (A10001986)
 *    - CB 1.5: Support for Futaba Battery Level Meter connector.
 *    - Add options for power LED and batt level meter usage
 *    - Add option to enable power LED and level meter on either fake or real power
 *    - Add "auto throttle": If enabled, an acceleration will continue when the throttle
 *      is released into neutral, until either 88mps is reached, or throttle is pulled
 *      below neutral.
 *  2024/10/05 (A10001986)
 *    - Buttons 1-8 can now be configured as maintained AND audio-on-ON-only. This
 *      supports use of three-position ON-OFF-(ON) switches, without audio restarted
 *      on OFF when flipping the switch from ON to (ON).
 *  2024/10/02 (A10001986)
 *    - New logic for button/switch 1-8 triggered sound playback
 *  2024/10/01 (A10001986)
 *    - Fix display.setText for "packed" display types
 *  2024/09/30 (A10001986)
 *    - Do not trigger events on initial position of maintained switches 1-8
 *  2024/09/28 (A10001986)
 *    - Switch i2c speed from 100kHz to 400kHz
 *    - Properly truncate UTF8 strings (MQTT user/topics/messages) if beyond buffer
 *      size. (Browsers' 'maxlength' is in characters, buffers are in bytes.
 *      MQTT is generally UTF8, and WiFiManager treats maxlength=buffer size,
 *      something to deal with at some point.)
 *  2024/09/27 (A10001986)
 *    - MQTT: Send user-configured messages to user-configured topics on
 *      buttons' 1-8 press/unpress events.
 *  2024/09/24 (A10001986)
 *    - Increase number of user buttons from 6 to 8 (CB 1.3)
 *  2024/09/23 (A10001986)
 *    - Re-add power LED (for CB 1.3, and people who want to use the Remote's
 *      native power LED, for instance)
 *    - Error check for calib procedure; store calib data as int, not float
 *  2024/09/21 (A10001986)
 *    - Minor modifications to display code
 *    - Remove redundancies, remove display of "." at boot
 *  2024/09/17 (A10001986)
 *    - Add "startup sequence" (counting from 0.0 to 11.0) and a power-up sound
 *    - Put "click" sound to lowest prio so that when another sound is played
 *      the click is suppressed.
 *  2024/09/16 (A10001986)
 *    - Fix PCA9554(A) reading
 *    - Switch to Dev Board 1.0 (still with prototype display)
 *  2024/09/14 (A10001986)
 *    - Rewrite the throttle control/acceleration logic
 *    - Speed up display of speed
 *  2024/09/13 (A10001986)
 *    - Put click sound into progmem
 *    - Set min accelDelay to 200ms/mph for linear acceleration
 *    - Add "movie-mode" setting to adapt accel pace (mostly) to movie.
 *      Movie times:
 *       0- 7:  90ms/mph
 *      20-24: 197ms/mph
 *      32-39: 200ms/mph
 *      55-59: 220ms/mph
 *      77-81: 300ms/mph
 *    - Add "display TCD speed" to CP and save it
 *  2024/09/12 (A10001986)
 *    - Add accleration "click" sound
 *    - Add default sounds for brake-on and throttle-up (from 0)
 *    - Some cleanup
 *  2024/09/11 (A10001986)
 *    - Fix C99-compliance
 *    - Fake .10ths while in TCD-controlled P0 mode
 *  2024/09/10 (A10001986)
 *    - Add coasting feature (off by default)
 *    - TCD overrules Remote during P0. So if a button-triggered TT occurs, the 
 *      Remote shows the speed the TCD displays on the speedo. For logical reasons,
 *      the brake is ignored in that scenario.
 *  2024/09/09 (A10001986)
 *    - Play sounds on fake-power on/off (poweron.mp3, poweroff.mp3), and on brake 
 *      hold/release (brakeon.mp3, brakeoff.mp3).
 *  2024/09/08 (A10001986)
 *    - Add feature to display GPS speed from TCD when Remote is fake-powered off
 *    - ButtonA = "O.O", button B = "RESET"
 *    - New procedure to reset WiFi AP password and fixed IP
 *  2024/09/07 (A10001986)
 *    - Save throttle Zero position when using an ADC
 *    - Fix throttle direction if sign of reading inversed
 *    - Add first version of TT sequence
 *    - Scale throttle when using an ADC
 *    - Buttons 1-6 can also be a maintained switch; in this case, no "hold" action 
 *      is possible.
 *    - Cleanup
 *  2024/09/04 (A10001986)
 *    - Hardware Spec changes:
 *      - Brake: Switch and LED hardwired (daisy chained), active high
 *      - Buttons A and B: Changed from act-high to act-low. Now all buttons are 
 *        active-low and can be wired with a "common" GND.
 *      - Removed LEDs for fake power and calibration
 *      - Use PCA9554 instead of PCA8574
 *    - Add support for PCA9554(A). If 8574 is connected, it needs to have i2c-A0 set in 
 *      order to be correctly recognized. PCA9554 all Ax -> GND, default i2c address used.
 *  2024/09/01 (A10001986)
 *    - Add support for PCA8574(A)-based button pack: Up to 8 add'l buttons for audio.
 *  2024/08/31 (A10001986)
 *    - Reduce command set (use only COMBINED)
 *    - Added WiFi-reconnect on fake-power-on if connection to configured WiFi network 
 *      failed
 *    - Fix latency calculation
 *    - Add buttons A and B, used for setup stuff when fake-powered down, for audio
 *      when fake-powered-up.
 *  2024/08/30 (A10001986)
 *    - Added preliminary ADS1015 ADC support (untested). i2c ADCs are handled in the
 *      RotEnc class, since their handling is identical.
 *  2024/08/29 (A10001986)
 *    - Add calibration mechanism
 *  2024/08/26 (A10001986)
 *    - Initial version (with RotEnc support for throttle)
 *
 */
