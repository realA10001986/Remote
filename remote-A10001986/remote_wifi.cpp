/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * WiFi and Config Portal handling
 *
 * -------------------------------------------------------------------
 * License: MIT NON-AI
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "remote_global.h"

#include <Arduino.h>

#include "src/WiFiManager/WiFiManager.h"

#ifndef WM_MDNS
#define REMOTE_MDNS
#include <ESPmDNS.h>
#endif

#include "display.h"
#include "remote_audio.h"
#include "remote_settings.h"
#include "remote_wifi.h"
#include "remote_main.h"
#ifdef REMOTE_HAVEMQTT
#include "mqtt.h"
#endif

#define STRLEN(x) (sizeof(x)-1)

Settings settings;

IPSettings ipsettings;

WiFiManager wm;
bool wifiSetupDone = false;

#ifdef REMOTE_HAVEMQTT
WiFiClient mqttWClient;
PubSubClient mqttClient(mqttWClient);
#endif

static const char R_updateacdone[] = "/uac";

static const char acul_part3[]  = "</head><body><div class='wrap'><h1>";
static const char acul_part5[]  = "</h1><h3>";
static const char acul_part6[]  = "</h3><div class='msg";
static const char acul_part7[]  = " S' id='lc'><strong>Upload successful.</strong><br/>Device rebooting.";
static const char acul_part7a[] = "<br>Installation will proceed after reboot.";
static const char acul_part71[] = " D'><strong>Upload failed.</strong><br>";
static const char acul_part8[]  = "</div></div></body></html>";
static const char *acul_errs[]  = { 
    "Can't open file on SD",
    "No SD card found",
    "Write error",
    "Bad file",
    "Not enough memory",
    "Unrecognized type",
    "Extraneous .bin file"
};

static const char *apChannelCustHTMLSrc[14] = {
    "'>WiFi channel",
    "apchnl",
    ">Random%s1'",
    ">1%s2'",
    ">2%s3'",
    ">3%s4'",
    ">4%s5'",
    ">5%s6'",
    ">6%s7'",
    ">7%s8'",
    ">8%s9'",
    ">9%s10'",
    ">10%s11'",
    ">11%s"
};

static const char *oorstCustHTMLSrc[5] = {
    "",
    "Holding O.O/RESET when Fake-Power off</legend>",
    "oorst",
    "adjusts display brightness",
    "takes/releases control of TCD Fake Power"
};

static const char *oottCustHTMLSrc[5] = {
    "mt5",
    "Pressing O.O when Fake-Power on</legend>",
    "oott",
    "plays previous song in Music Player",
    "makes throttle-up trigger a time travel"
};

#ifdef HAVE_PM
static const char *batTypeHTMLSrc[7] = {
    "'>Battery type'",
    "bty",
    ">3.7V/4.2V LiPo%s3'",
    ">3.8V/4.35V LiPo%s4'",
    ">3.85V/4.4V LiPo%s1'",
    ">UR18650ZY%s2'",
    ">ICR18650-26H%s"
};
static const char *wmBuildBatType(const char *dest, int op);
#endif

#ifdef REMOTE_HAVEMQTT
static const char *mqttpCustHTMLSrc[4] = {
    "'>Protocol version",
    "mprot",
    ">3.1.1%s1'",
    ">5.0%s"
};
static const char mqttMsgDisabled[] = "Disabled";
static const char mqttMsgConnecting[] = "Connecting...";
static const char mqttMsgTimeout[] = "Connection time-out";
static const char mqttMsgFailed[] = "Connection failed";
static const char mqttMsgDisconnected[] = "Disconnected";
static const char mqttMsgConnected[] = "Connected";
static const char mqttMsgBadProtocol[] = "Protocol error";
static const char mqttMsgUnavailable[] = "Server unavailable/busy";
static const char mqttMsgBadCred[] = "Login failed";
static const char mqttMsgGenError[] = "Error";
#endif

static const char *wmBuildApChnl(const char *dest, int op);
static const char *wmBuildBestApChnl(const char *dest, int op);

static const char *wmBuildOORST(const char *dest, int op);
static const char *wmBuildOOTT(const char *dest, int op);
static const char *wmBuildHaveSD(const char *dest, int op);

#ifdef REMOTE_HAVEMQTT
static const char *wmBuildMQTTprot(const char *dest, int op);
static const char *wmBuildMQTTstate(const char *dest, int op);
#endif

// double-% since this goes through sprintf!
static const char bannerStart[] = "<div class='c' style='background-color:#";
static const char bannerMid[] = ";color:#fff;font-size:80%;border-radius:5px'>";

static const char bestAP[]   = "%s%s%sProposed channel at current location: %d<br>%s(Non-WiFi devices not taken into account)</div>";
static const char badWiFi[]  = "<br><i>Operating in AP mode not recommended</i>";

static const char bannerGen[] = "%s%s%s%s</div>";
static const char haveNoSD[] = "<i>No SD card present</i>";

#ifdef REMOTE_HAVEMQTT
static const char mqttStatus[] = "%s%s%s%s%s (%d)</div>";
#endif

static const char custHTMLHdr1[] = "<div class='cmp0'";
static const char custHTMLHdr2[] = "><label class='mp0' for='";
static const char custHTMLHdrI[] = " style='margin-left:20px'";
static const char custHTMLSHdr[] = "</label><select class='sel0' value='";
static const char osde[] = "</option></select></div>";
static const char ooe[]  = "</option><option value='";
static const char custHTMLSel[] = " selected";
static const char custHTMLSelFmt[] = "' name='%s' id='%s' autocomplete='off'><option value='0'";
static const char col_g[] = "609b71";
static const char col_r[] = "dc3630";
static const char col_gr[] = "777";
static const char rad0[] = "<div class='cmp0'><fieldset class='%s' style='border:none;padding:0;'><legend style='padding:0;margin-bottom:2px'>";
static const char rad1[] = "<input type='radio' id='%s%d' name='%s' value='%d'%s style='margin:5px 5px 5px 10px'><label class='mp0' for='%s%d'>%s</label><br>";
static const char radchk[] = " checked";
static const char rad99[] = "</fieldset></div>";

// WiFi Configuration

#if defined(REMOTE_MDNS) || defined(WM_MDNS)
#define HNTEXT "Hostname<br><span>The Config Portal is accessible at http://<i>hostname</i>.local<br>(Valid characters: a-z/0-9/-)</span>"
#else
#define HNTEXT "Hostname<br><span>(Valid characters: a-z/0-9/-)</span>"
#endif
WiFiManagerParameter custom_hostName("hostname", HNTEXT, settings.hostName, 31, "pattern='[A-Za-z0-9\\-]+' placeholder='Example: dtmremote'", WFM_LABEL_BEFORE|WFM_SECTS_HEAD);

WiFiManagerParameter custom_sectstart_wifi("WiFi connection: Other settings", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_wifiConRetries("wifiret", "Connection attempts (1-10)", settings.wifiConRetries, 2, "type='number' min='1' max='10'");
WiFiManagerParameter custom_wifiConTimeout("wificon", "Connection timeout (7-25[seconds])", settings.wifiConTimeout, 2, "type='number' min='7' max='25'");
WiFiManagerParameter custom_reconAtmp("recAtmt", "Re-attempt connection on Fake Power", settings.reconOnFP, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_sectstart_ap("Access point (AP) mode settings", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_sysID("sysID", "Network name (SSID) appendix<br><span>Will be appended to \"REM-AP\" [a-z/0-9/-]</span>", settings.systemID, 7, "pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_appw("appw", "Password<br><span>Password to protect REM-AP. Empty or 8 characters [a-z/0-9/-]</span>", settings.appw, 8, "minlength='8' pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_apch(wmBuildApChnl);
WiFiManagerParameter custom_bapch(wmBuildBestApChnl);
WiFiManagerParameter custom_wifiAPOffDelay("wifiAPoff", "Power save timer<br><span>(10-99[minutes]; 0=off)</span>", settings.wifiAPOffDelay, 2, "type='number' min='0' max='99'");
WiFiManagerParameter custom_reactAP("reactAP", "Re-enable WiFi on Fake Power", settings.reactAPOnFP, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_FOOT);

// Settings

WiFiManagerParameter custom_at("at", "Auto throttle<br><span>Accleration will continue on trottle release. Has precedence over Coasting.</span>", settings.autoThrottle, "class='mt5' style='margin-bottom:5px;'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS_HEAD);
WiFiManagerParameter custom_coast("cst", "Coasting when throttle in neutral", settings.coast, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_sStrict("sStrict", "Movie-like acceleration<br><span>Check to set the acceleration pace to what is shown in the movie. This slows down acceleration at higher speeds.</span>", settings.movieMode, "class='mb0'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_playclick("plyCLK", "Play acceleration 'click' sound", settings.playClick, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_playALSnd("plyALS", "Play TCD-alarm sound", settings.playALsnd, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_dGPS("dGPS", "Display TCD speed when Fake-Power is off", settings.dgps, "class='mb10'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_sectstart_mp("MusicPlayer", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_musicFolder("mfol", "Music folder (0-9)", settings.musicFolder, 2, "type='number' min='0' max='9'");

WiFiManagerParameter custom_sectstart_nw("Wireless communication (BTTF-Network)", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_tcdIP("tcdIP", "IP address or hostname of TCD", settings.tcdIP, 31, "pattern='(^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$)|([A-Za-z0-9\\-]+)' placeholder='Example: timecircuits'");
WiFiManagerParameter custom_pwrMst("pwM", "Remote Fake-Power controls TCD Fake-Power<br><span>Remote Fake-Power will overrule TFC switch and control TCD Fake-Power. Can be toggled by O.O/RESET if so configured below.</span>", settings.pwrMst, "class='mb0'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_haveSD(wmBuildHaveSD, WFM_SECTS);
WiFiManagerParameter custom_CfgOnSD("CfgOnSD", "Save secondary settings on SD<br><span>Check this to avoid flash wear</span>", settings.CfgOnSD, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
//WiFiManagerParameter custom_sdFrq("sdFrq", "4MHz SD clock speed<br><span>Checking this might help in case of SD card problems</span>", settings.sdFreq, "style='margin-top:12px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_upd("upd", "Show update notifications on power-up", settings.upd, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_oorst(wmBuildOORST, WFM_SECTS);
WiFiManagerParameter custom_oott(wmBuildOOTT);

WiFiManagerParameter custom_sectstart_hw("User Buttons", WFM_SECTS|WFM_HL);
#ifdef ALLOW_DIS_UB
WiFiManagerParameter custom_dBP("dBP", "Disable User Buttons", settings.disBPack, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
#endif
WiFiManagerParameter custom_b0mt("b0mt", "Button 1 is maintained switch", settings.bPb0Maint, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b1mt("b1mt", "Button 2 is maintained switch", settings.bPb1Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b2mt("b2mt", "Button 3 is maintained switch", settings.bPb2Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b3mt("b3mt", "Button 4 is maintained switch", settings.bPb3Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b4mt("b4mt", "Button 5 is maintained switch", settings.bPb4Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b5mt("b5mt", "Button 6 is maintained switch", settings.bPb5Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b6mt("b6mt", "Button 7 is maintained switch", settings.bPb6Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b7mt("b7mt", "Button 8 is maintained switch", settings.bPb7Maint, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b0mtoo("b0mto", "Maintained: Play audio on ON only", settings.bPb0MtO, "title='Check to play audio when switch is in ON position only. If unchecked, audio is played on each flip.' class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b1mtoo("b1mto", "Maintained: Play audio on ON only", settings.bPb1MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b2mtoo("b2mto", "Maintained: Play audio on ON only", settings.bPb2MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b3mtoo("b3mto", "Maintained: Play audio on ON only", settings.bPb3MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b4mtoo("b4mto", "Maintained: Play audio on ON only", settings.bPb4MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b5mtoo("b5mto", "Maintained: Play audio on ON only", settings.bPb5MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b6mtoo("b6mto", "Maintained: Play audio on ON only", settings.bPb6MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_b7mtoo("b7mto", "Maintained: Play audio on ON only", settings.bPb7MtO, "class='mt5' style='margin-left:20px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_uPL("uPL", "Use Futaba power LED", settings.usePwrLED, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS);
WiFiManagerParameter custom_PLD("PLD", "Power LED on fake power", settings.pwrLEDonFP, "class='mt5' style='margin-left:20px' title='If unchecked, LED follows real power'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_uLM("uMt", "Use Futaba battery level meter", settings.useLvlMtr, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_PMD("PMD", "Level meter on fake power", settings.LvLMtronFP, "class='mt5' style='margin-left:20px' title='If unchecked, meter follows real power'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_FOOT);

#ifdef HAVE_PM
WiFiManagerParameter custom_UPM("UPM", "Battery monitoring/warnings", settings.usePwrMon, "title='If unchecked, no battery-low warnings will be given' class='mt5 mb10'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS_HEAD);
WiFiManagerParameter custom_bty(wmBuildBatType);  // batt type
WiFiManagerParameter custom_bca("bCa", "Capacity per cell (1000-6000)", settings.batCap, 5, "type='number' min='1000' max='6000' autocomplete='off'", WFM_LABEL_BEFORE|WFM_FOOT);
#endif

#ifdef REMOTE_HAVEMQTT
WiFiManagerParameter custom_useMQTT("uMQTT", "Home Assistant support (MQTT)", settings.useMQTT, "class='mt5 mb10'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS_HEAD);
WiFiManagerParameter custom_state(wmBuildMQTTstate);
WiFiManagerParameter custom_mqttServer("ha_server", "Broker IP[:port] or domain[:port]", settings.mqttServer, 79, "pattern='[a-zA-Z0-9\\.:\\-]+' placeholder='Example: 192.168.1.5'");
WiFiManagerParameter custom_mqttVers(wmBuildMQTTprot);
WiFiManagerParameter custom_mqttUser("ha_usr", "User[:Password]", settings.mqttUser, 63, "placeholder='Example: ronald:mySecret' class='mb15'");

WiFiManagerParameter custom_mqttb1t("ha_b1t", "Button 1 topic", settings.mqttbt[0], 127, "placeholder='Example: home/lights/1/'", WFM_LABEL_BEFORE|WFM_SECTS);
WiFiManagerParameter custom_mqttb1o("ha_b1o", "Button 1 message on ON", settings.mqttbo[0], 63, "placeholder='Example: ON'");
WiFiManagerParameter custom_mqttb1f("ha_b1f", "Button 1 message on OFF", settings.mqttbf[0], 63, "placeholder='Example: OFF' class='mb15'");
WiFiManagerParameter custom_mqttb2t("ha_b2t", "Button 2 topic", settings.mqttbt[1],127);
WiFiManagerParameter custom_mqttb2o("ha_b2o", "Button 2 message on ON", settings.mqttbo[1], 63);
WiFiManagerParameter custom_mqttb2f("ha_b2f", "Button 2 message on OFF", settings.mqttbf[1], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb3t("ha_b3t", "Button 3 topic", settings.mqttbt[2],127);
WiFiManagerParameter custom_mqttb3o("ha_b3o", "Button 3 message on ON", settings.mqttbo[2], 63);
WiFiManagerParameter custom_mqttb3f("ha_b3f", "Button 3 message on OFF", settings.mqttbf[2], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb4t("ha_b4t", "Button 4 topic", settings.mqttbt[3], 127);
WiFiManagerParameter custom_mqttb4o("ha_b4o", "Button 4 message on ON", settings.mqttbo[3], 63);
WiFiManagerParameter custom_mqttb4f("ha_b4f", "Button 4 message on OFF", settings.mqttbf[3], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb5t("ha_b5t", "Button 5 topic", settings.mqttbt[4], 127);
WiFiManagerParameter custom_mqttb5o("ha_b5o", "Button 5 message on ON", settings.mqttbo[4], 63);
WiFiManagerParameter custom_mqttb5f("ha_b5f", "Button 5 message on OFF", settings.mqttbf[4], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb6t("ha_b6t", "Button 6 topic", settings.mqttbt[5], 127);
WiFiManagerParameter custom_mqttb6o("ha_b6o", "Button 6 message on ON", settings.mqttbo[5], 63);
WiFiManagerParameter custom_mqttb6f("ha_b6f", "Button 6 message on OFF", settings.mqttbf[5], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb7t("ha_b7t", "Button 7 topic", settings.mqttbt[6], 127);
WiFiManagerParameter custom_mqttb7o("ha_b7o", "Button 7 message on ON", settings.mqttbo[6], 63);
WiFiManagerParameter custom_mqttb7f("ha_b7f", "Button 7 message on OFF", settings.mqttbf[6], 63, "class='mb15'");
WiFiManagerParameter custom_mqttb8t("ha_b8t", "Button 8 topic", settings.mqttbt[7], 127);
WiFiManagerParameter custom_mqttb8o("ha_b8o", "Button 8 message on ON", settings.mqttbo[7], 63);
WiFiManagerParameter custom_mqttb8f("ha_b9f", "Button 8 message on OFF", settings.mqttbf[7], 63, WFM_LABEL_BEFORE|WFM_FOOT);
#endif // HAVEMQTT

#ifdef REMOTE_HAVEMQTT
#define TC_MENUSIZE 8
#else
#define TC_MENUSIZE 7
#endif
static const int8_t wifiMenu[TC_MENUSIZE] = { 
    WM_MENU_WIFI,
    WM_MENU_PARAM,
    #ifdef REMOTE_HAVEMQTT
    WM_MENU_PARAM2,
    #endif
    WM_MENU_SEP,
    WM_MENU_UPDATE,
    WM_MENU_SEP,
    WM_MENU_CUSTOM,
    WM_MENU_END
};

#define AA_TITLE "DTM Remote"
#define AA_ICON "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQBAMAAADt3eJSAAAAFVBMVEVJSkrMy8e2v70AAADa5ePyJT320zT0Sr0YAAAAQElEQVQI12MQhAIGAQYwYAQzHFAZTEoKUEYalMFsDAIghhIQKDMyiAaDGSARZSMlYxAjGMZASIEZCO1wS+HOAADWkAscxWroAQAAAABJRU5ErkJggg=="
#define AA_CONTAINER "REMA"
#define UNI_VERSION REMOTE_VERSION 
#define UNI_VERSION_EXTRA REMOTE_VERSION_EXTRA
#define WEBHOME "remote"
#define PARM2TITLE WM_PARAM2_TITLE
#define CURRVERSION REMOTE_VERSION

static const char myTitle[] = AA_TITLE;
static const char apName[]  = "REM-AP";
static const char myHead[]  = "<link rel='shortcut icon' type='image/png' href='data:image/png;base64," AA_ICON "'><script>window.onload=function(){xx=false;document.title=xxx='" AA_TITLE "';id=-1;ar=['/u','/uac','/wifisave','/paramsave','/param2save'];ti=['Firmware upload','','WiFi Configuration','Settings','" PARM2TITLE "'];if(ge('s')&&ge('dns')){xx=true;yyy=ti[2]}if(ge('uploadbin')||(id=ar.indexOf(wlp()))>=0){xx=true;if(id>=2){yyy=ti[id]}else{yyy=ti[0]};aa=gecl('wrap');if(aa.length>0){if(ge('uploadbin')){aa[0].style.textAlign='center';}aa=getn('H3');if(aa.length>0){aa[0].remove()}aa=getn('H1');if(aa.length>0){aa[0].remove()}}}if(ge('ttrp')||wlp()=='/param'||wlp()=='/param2'){xx=true;yyy=ti[3];}if(ge('ebnew')){xx=true;bb=getn('H3');aa=getn('H1');yyy=bb[0].innerHTML;ff=aa[0].parentNode;ff.style.position='relative';}if(xx){zz=(Math.random()>0.8);dd=document.createElement('div');dd.classList.add('tpm0');dd.innerHTML='<div class=\"tpm\" onClick=\"window.location=\\'/\\'\"><div class=\"tpm2\"><img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABAAQMAAACQp+OdAAAABlBMVEUAAABKnW0vhlhrAAAAAXRSTlMAQObYZgAAA'+(zz?'GBJREFUKM990aEVgCAABmF9BiIjsIIbsJYNRmMURiASePwSDPD0vPT12347GRejIfaOOIQwigSrRHDKBK9CCKoEqQF2qQMOSQAzEL9hB9ICNyMv8DPKgjCjLtAD+AV4dQM7O4VX9m1RYAAAAABJRU5ErkJggg==':'HtJREFUKM990bENwyAUBuFnuXDpNh0rZIBIrJUqMBqjMAIlBeIihQIF/fZVX39229PscYG32esCzeyjsXUzNHZsI0ocxJ0kcZIOsoQjnxQJT3FUiUD1NAloga6wQQd+4B/7QBQ4BpLAOZAn3IIy4RfUibCgTTDq+peG6AvsL/jPTu1L9wAAAABJRU5ErkJggg==')+'\" class=\"tpm3\"></div><H1 class=\"tpmh1\"'+(zz?' style=\"margin-left:1.4em\"':'')+'>'+xxx+'</H1>'+'<H3 class=\"tpmh3\"'+(zz?' style=\"padding-left:5em\"':'')+'>'+yyy+'</div></div>';}if(ge('ebnew')){bb[0].remove();aa[0].replaceWith(dd);}else if(xx){aa=gecl('wrap');if(aa.length>0){aa[0].insertBefore(dd,aa[0].firstChild);aa[0].style.position='relative';}}var lc=ge('lc');if(lc){lc.style.transform='rotate('+(358+[0,1,3,4,5][Math.floor(Math.random()*4)])+'deg)'}}</script><style type='text/css'>H1,H2{margin-top:0px;margin-bottom:0px;text-align:center;}H3{margin-top:0px;margin-bottom:5px;text-align:center;}button{transition-delay:250ms;margin-top:10px;margin-bottom:10px;font-variant-caps:all-small-caps;border-bottom:0.2em solid #225a98}input{border:thin inset}em > small{display:inline}form{margin-block-end:0;}.tpm{cursor:pointer;border:1px solid black;border-radius:5px;padding:0 0 0 0px;min-width:18em;}.tpm2{position:absolute;top:-0.7em;z-index:130;left:0.7em;}.tpm3{width:4em;height:4em;}.tpmh1{font-variant-caps:all-small-caps;font-weight:normal;margin-left:2.2em;overflow:clip;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI Semibold',Roboto,'Helvetica Neue',Verdana,Helvetica}.tpmh3{background:#000;font-size:0.6em;color:#ffa;padding-left:7.2em;margin-left:0.5em;margin-right:0.5em;border-radius:5px}.tpm0{position:relative;width:20em;padding:5px 0px 5px 0px;margin:0 auto 0 auto;}.cmp0{margin:0;padding:0;}.sel0{font-size:90%;width:auto;margin-left:10px;vertical-align:baseline;}.mt5{margin-top:5px!important}.mb10{margin-bottom:10px!important}.mb0{margin-bottom:0px!important}.mb15{margin-bottom:15px!important}.ss>label span{font-size:80%}</style>";
static const char* myCustMenu = "<img id='ebnew' style='display:block;margin:10px auto 5px auto;' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAR8AAAAyCAMAAABSzC8jAAAAUVBMVEUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABcqRVCAAAAGnRSTlMAv4BAd0QgzxCuMPBgoJBmUODdcBHumYgzVQmpHc8AAAf3SURBVGje7Jjhzp0gDIYFE0BQA/Ef93+hg7b4wvQ7R5Nl2Y812fzgrW15APE4eUW2rxOZNJfDcRu2q2Zjv9ygfe+1xSY7bXNWHH3lm13NJ01P/5PcrqyIeepfcLeCraOfpN7nPoSuLWjxHCSVa7aQs909Zxcf8mDBTNOcxWwlgmbw02gqNxv7z+5t8FIM2IdO1OUPzzmUNPl/K4F0vbIiNnMCf7pnmO79kBq57sviAiq3GKT3QFyqbG2NFUC4SDSDeckn68FLkWpPEXVFCbKUJDIQ84XP/pgPvO/LWlCHC60zjnzMKczkC4p9c3vLJ8GLYmMiBIGnGeHS2VdJ6/jCJ73ik10fIrhB8yefA/4jn/1syGLXWlER3DzmuNS4Vz4z2YWPnWfNqcVrTTKLtkaP0Q4IdhlQcdpkIPbCR3K1yn3jUzvr5JWLoa6j+SkuJNAkiESp1qYdiXPMALrUOyT7RpG8CL4Iin01jQRopWkufNCCyVbakbO0jCxUGjqugYgoLAzdJtpc+HQJ4Hj2aHBEgVRIFG/s5f3UPJUFPjxGE8+YyOiqMIPPWnmDDzI/5BORE70clHFjR1kaMEGLjc/xhY99yofCbpC4ENGmkQ/2yIWP5b/Ax1PYP8tHomB1bZSYFwSnIp9E3R/5ZPOIj6jLUz7Z3/EJlG/kM9467W/311aubuTDnQYD4SG6nEv/QkRFssXtE58l5+PN+tGP+Cw1sx/4YKjKf+STbp/PutqVT9I60e3sJVF30CIWK19c0XR11uCzF3XkI7kqXNbtT3w28gOflVMJHwc+eDYN55d25zTXSCuFJWHkk5gPZdzTh/P9ygcvmEJx645cyYLCYqk/Ffoab4k+5+X2fJ+FRl1g93zgp2iiqEwjfJiWbtqWr4dQESKGwSW5xIJH5XwEju+H7/gEP11exEY+7Dzr8q8IVVxkjHVy3Cc+R87HAz5iWqSDT/vYa9sEPiagcvAp5kUwHR97rh/Ae7V+wtp7be6OTyiXvbAo/7zCQKa6wT7xMTnbx3w0pMtr6z6BTwG08Mof+JCgWLh7/oDz/fvh3fPZrYmXteorHvkc3FF3QK2+dq2NT91g6ub90DUatlR0z+cQP6Q2I5/YazP4cGGJXPB+KMtCfpv5Cx/KqPgwen5+CWehGBtfiYPTZCnONtsplizdmwQ9/ez1/AKNg/Rv55edD54I8Alr07gs8GFzlqNh9fbCcfJx5brIrXwGvOAj16V5WeaC+jVg0FEyF+fOh98nPvHxpD8430Mh0R1t0UGrZQXwEYv3fOTRLnzGo49hveejmtdBfHGdGoy1LRPilMHCf+EzpYd8NtoVkKBxX/ydj/+Jzzzw2fgeuVU2hqNfgVc+hrb8wMf0fIzw9XJ1IefEOQVDyOQPFukLn/0ZH/nBdc/Hj+eXoyHsFz4ibB0fV8MF3MrbmMULHyQHn7iQK3thg4Xa68zSdr7rPkaMfPYvfPwjPpwyQRq1NA4yrG6ig2Ud+ehUOtYwfP8Z0RocbuDTbB75wFbhg421Q/TsLXw2xgEWceTTDDOb7vnATxgsnOvKR8qJ+H1x+/0nd0MN7IvvSOP3jVd88CFq3FhiSxeljezo10r4wmd/yGflDXblg7JkkAEvRSMfRB0/OIMPb7CXfGK3C5NssIgfH2Ttw9tKgXo+2xc+/gkf2cLpjg/K4kH6jNoGPnM/p9Kwm5nARx63b/ioGgB89nZyeSKyuW7kqqU1PZ/4hc+UnvGRDXblg7JkkPMWam3ajdPchKSnv2PeTP+qmdn8JPy7Rf+3X+zBgQAAAAAAkP9rI6iqqirNme2qpDAMhhtIWvxVKP2w7/1f6DapVmdnzsDCCucFx7QmaXx0ouB/kOfGfprM52Rkf4xZtb9E5BERsxnM0TlhGZvK/PXImI5sEj9sf9kzu3q9ltBt2hKK7bKmP2rRFZxlkcttWI3Zu2floeqGBzhnCVqQjmGq94hyfK3dzUiOwWNTmT9rJDmCiWXYcrNdDmqXi3mHqh0RZLnMIUHPPiGzJo2zkuXmghnZPavQZAMNI5fykQ9zA/wV0LBJr00LD8yhHnyIh4ynNz6RGYlZjI9ah+0qCvOWbhWAJVJ3hMrMceYKqK4plh1kK3hgYy5xuXWELo3cw1L+KONnC/yRzxpexyxsR9LYXau3zYSCzfi449f4zPHcF+wWtgRYHWsVBk/Xjs1Gx7apl7+7Wdjz8lq2YL/zYRH5zKeh8L7qOwxGFRG7cyrknU8QkX2xelVAiH4tmi8+dt022BVYNSy3DjSdel4bosupuTufWz/hiuAu5QSA8t98VKyn5Et456OiH/hIAdDORWX+vxL6ZFOSu/NZbnoUSLt7XKztt6X8wqcy8+rPW34JiLVgu/hc/UfUf9jxjU8honbxeVXmDeBjUT9Zlz4zC+obH3PT1C2huKcV7fSRiBLoQ/8RBn146o24eufDq5nklL70H4/0sQi6NZYqyWwPYvS5QkVctV1kgw6e1HmamPrYn4OWtl41umjhZWw6LfGNj4v41p+TLujZLbG3i/TSePukmEDIcybaKwHvy82zOezuWd24/PT8EiQ15GyniQqaNmqUst5/Eg3tRz//xqcDSLc3hgwEArqjsR+arMlul2ak50ywsLrcGgolBPddz/OxIV98YgDQsvoXIJ33j0mmv3zj43oCCuer+9h4PRTO51fJxpJPPrkCIFlusun4V375878k4T+G/QFTIGsvrRmuEwAAAABJRU5ErkJggg=='><div style='font-size:0.9em;line-height:0.9em;font-weight:bold;margin:5px auto 0px auto;text-align:center;font-variant:all-small-caps'>" UNI_VERSION " (" UNI_VERSION_EXTRA ")<br>Powered by A10001986 <a href='https://" WEBHOME ".out-a-ti.me' style='text-decoration:underline' target=_blank>[Home/Updates]</a></div>";

const char menu_myDiv[]  = "<hr><div style='margin-left:auto;margin-right:auto;text-align:center;'>";
const char menu_myNoSP[] = "Please <a href='/update'>install</a> sound pack (";
const char menu_myUAI[]  = "Update available (";
const char menu_end[]    = ")</div><hr>";

static char newversion[8];

static int  shouldSaveConfig = 0;
static bool shouldSaveIPConfig = false;
static bool shouldDeleteIPConfig = false;

// Did user configure a WiFi network to connect to?
bool wifiHaveSTAConf = false;
static bool connectedToTCDAP = false;

// WiFi power management in AP mode
bool          wifiInAPMode = false;
bool          wifiAPIsOff = false;
unsigned long wifiAPModeNow;
unsigned long wifiAPOffDelay = 0;     // default: never
static bool   wifiReactAPOnFP = true;

// WiFi power management in STA mode
bool          wifiIsOff = false;
unsigned long wifiOnNow = 0;
unsigned long wifiOffDelay     = 0;   // default: never
unsigned long origWiFiOffDelay = 0;
static bool   wifiReconOnFP = true;

static File acFile;
static bool haveACFile = false;
static bool haveAC = false;
static int  numUploads = 0;
static int  *ACULerr = NULL;
static int  *opType = NULL;

#ifdef REMOTE_HAVEMQTT
#define MQTT_SHORT_INT (30*1000)
#define MQTT_LONG_INT  (5*60*1000)
static const char    emptyStr[1] = { 0 };
bool                 useMQTT = false;
static char          *mqttUser = (char *)emptyStr;
static char          *mqttPass = (char *)emptyStr;
static char          *mqttServer = (char *)emptyStr;
static unsigned long mqttReconnectNow = 0;
static unsigned long mqttReconnectInt = MQTT_SHORT_INT;
static uint16_t      mqttReconnFails = 0;
static bool          mqttSubAttempted = false;
static bool          mqttOldState = true;
static bool          mqttDoPing = true;
static bool          mqttRestartPing = false;
static bool          mqttPingDone = false;
static unsigned long mqttPingNow = 0;
static unsigned long mqttPingInt = MQTT_SHORT_INT;
static uint16_t      mqttPingsExpired = 0;
#endif

static unsigned int wmLenBuf = 0;

static void wifiConnect(bool APonly = false, bool deferConfigPortal = false);
static void wifiOff(bool force);

static void checkForUpdate();

static void saveParamsCallback(int);
static void preSaveWiFiCallback();
static void saveWiFiCallback(const char *ssid, const char *pass);
static void preUpdateCallback();
static void postUpdateCallback(bool);
static int  menuOutLenCallback();
static void menuOutCallback(String& page, unsigned int ssize);
static void wifiDelayReplacement(unsigned int mydel);
static void gpCallback(int);
static bool preWiFiScanCallback();

static void updateConfigPortalValues();

static void setupStaticIP();
static IPAddress stringToIp(char *str);

static void getParam(String name, char *destBuf, size_t length, int defaultVal);
static bool myisspace(char mychar);
static char* strcpytrim(char* destination, const char* source, bool doFilter = false);
static void mystrcpy(char *sv, WiFiManagerParameter *el);
static void evalCB(char *sv, WiFiManagerParameter *el);
static void setCBVal(WiFiManagerParameter *el, char *sv);

static void setupWebServerCallback();
static void handleUploadDone();
static void handleUploading();
static void handleUploadDone();

#ifdef REMOTE_HAVEMQTT
static void strcpyutf8(char *dst, const char *src, unsigned int len);
static void mqttPing();
static bool mqttReconnect(bool force = false);
static void mqttLooper();
static void mqttCallback(char *topic, byte *payload, unsigned int length);
static void mqttSubscribe();
#endif

/*
 * wifi_setup()
 *
 */
void wifi_setup()
{
    int temp;

    WiFiManagerParameter *wifiParmArray[] = {

      &custom_hostName,

      &custom_sectstart_wifi,
      &custom_wifiConRetries, 
      &custom_wifiConTimeout,
      &custom_reconAtmp,

      &custom_sectstart_ap,
      &custom_sysID,
      &custom_appw,
      &custom_apch,
      &custom_bapch,
      &custom_wifiAPOffDelay,
      &custom_reactAP,

      NULL
    };

    WiFiManagerParameter *parmArray[] = {

      &custom_at,             // 7
      &custom_coast,
      &custom_sStrict,
      &custom_playclick,
      &custom_playALSnd,
      &custom_dGPS,
  
      &custom_sectstart_mp,   // 3
      &custom_musicFolder,
  
      &custom_sectstart_nw,   // 3
      &custom_tcdIP,
      &custom_pwrMst,
      
      &custom_haveSD,         // 2(3)
      &custom_CfgOnSD,
      //&custom_sdFrq,
      &custom_upd,

      &custom_oorst,
      &custom_oott,
  
      &custom_sectstart_hw,  // 17
      #ifdef ALLOW_DIS_UB
      &custom_dBP,
      #endif
      &custom_b0mt,
      &custom_b0mtoo,
      &custom_b1mt,
      &custom_b1mtoo,
      &custom_b2mt,
      &custom_b2mtoo,
      &custom_b3mt,
      &custom_b3mtoo,
      &custom_b4mt,
      &custom_b4mtoo,
      &custom_b5mt,
      &custom_b5mtoo,
      &custom_b6mt,
      &custom_b6mtoo,
      &custom_b7mt,
      &custom_b7mtoo,
  
      &custom_uPL,          // 4
      &custom_PLD,
      &custom_uLM,
      &custom_PMD,

      NULL
    };

    #ifdef REMOTE_HAVEMQTT
    WiFiManagerParameter *parm2Array[] = {

      &custom_useMQTT,    // 5
      &custom_state,
      &custom_mqttServer,
      &custom_mqttVers,
      &custom_mqttUser,

      &custom_mqttb1t,    // 24
      &custom_mqttb1o,
      &custom_mqttb1f,
      &custom_mqttb2t,
      &custom_mqttb2o,
      &custom_mqttb2f,
      &custom_mqttb3t,
      &custom_mqttb3o,
      &custom_mqttb3f,
      &custom_mqttb4t,
      &custom_mqttb4o,
      &custom_mqttb4f,
      &custom_mqttb5t,
      &custom_mqttb5o,
      &custom_mqttb5f,
      &custom_mqttb6t,
      &custom_mqttb6o,
      &custom_mqttb6f,
      &custom_mqttb7t,
      &custom_mqttb7o,
      &custom_mqttb7f,
      &custom_mqttb8t,
      &custom_mqttb8o,
      &custom_mqttb8f,

      NULL
    };
    #endif

    // Transition from NVS-saved data to own management:
    if(!settings.ssid[0] && settings.ssid[1] == 'X') {
        
        // Read NVS-stored WiFi data
        wm.getStoredCredentials(settings.ssid, sizeof(settings.ssid), settings.pass, sizeof(settings.pass));

        #ifdef REMOTE_DBG
        Serial.printf("WiFi Transition: ssid '%s' pass '%s'\n", settings.ssid, settings.pass);
        #endif

        write_settings();
    }

    wm.setHostname(settings.hostName);

    wm.showUploadContainer(haveSD, AA_CONTAINER, true);
    
    wm.setPreSaveWiFiCallback(preSaveWiFiCallback);
    wm.setSaveWiFiCallback(saveWiFiCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setPreOtaUpdateCallback(preUpdateCallback);
    wm.setPostOtaUpdateCallback(postUpdateCallback);
    wm.setWebServerCallback(setupWebServerCallback);
    wm.setMenuOutLenCallback(menuOutLenCallback);
    wm.setMenuOutCallback(menuOutCallback);
    wm.setDelayReplacement(wifiDelayReplacement);
    wm.setGPCallback(gpCallback);
    wm.setPreWiFiScanCallback(preWiFiScanCallback);

    // Our style-overrides, the page title
    wm.setCustomHeadElement(myHead);
    wm.setTitle(myTitle);

    // Hack version number into WiFiManager main page
    wm.setCustomMenuHTML(myCustMenu);

    temp = atoi(settings.apChnl);
    if(temp < 0) temp = 0;
    if(temp > 11) temp = 11;
    if(!temp) temp = random(1, 11);
    wm.setWiFiAPChannel(temp);

    temp = atoi(settings.wifiConTimeout);
    if(temp < 7) temp = 7;
    if(temp > 25) temp = 25;
    wm.setConnectTimeout(temp);

    temp = atoi(settings.wifiConRetries);
    if(temp < 1) temp = 1;
    if(temp > 10) temp = 10;
    wm.setConnectRetries(temp);

    wifiReconOnFP = evalBool(settings.reconOnFP);
    wifiReactAPOnFP = evalBool(settings.reactAPOnFP);

    wm.setMenu(wifiMenu, TC_MENUSIZE, false);

    temp = (sizeof(parmArray) / sizeof(WiFiManagerParameter *)) - 1;
    #ifdef HAVE_PM
    if(havePwrMon) temp += 3;
    #endif
    wm.allocParms(temp);

    temp = 0;
    while(parmArray[temp]) {
        wm.addParameter(parmArray[temp]);
        temp++;
    }
    
    #ifdef HAVE_PM
    if(havePwrMon) {
        wm.addParameter(&custom_UPM);
        wm.addParameter(&custom_bty);
        wm.addParameter(&custom_bca);
    }
    #endif

    wm.allocWiFiParms((sizeof(wifiParmArray) / sizeof(WiFiManagerParameter *)) - 1);

    temp = 0;
    while(wifiParmArray[temp]) {
        wm.addWiFiParameter(wifiParmArray[temp]);
        temp++;
    }

    #ifdef REMOTE_HAVEMQTT
    wm.allocParms2((sizeof(parm2Array) / sizeof(WiFiManagerParameter *)) - 1);

    temp = 0;
    while(parm2Array[temp]) {
        wm.addParameter2(parm2Array[temp]);
        temp++;
    }
    #endif

    updateConfigPortalValues();

    #ifdef REMOTE_HAVEMQTT
    useMQTT = evalBool(settings.useMQTT);
    #endif

    wifiHaveSTAConf = (settings.ssid[0] != 0);

    // See if we have a configured WiFi network to connect to.
    // If we detect "TCD-AP" as the SSID, we make sure that we retry
    // at least 2 times so we have a chance to catch the TCD's AP if 
    // both are powered up at the same time.
    if(wifiHaveSTAConf) {
        if(!strncmp("TCD-AP", settings.ssid, 6)) {
            if(wm.getConnectRetries() < 2) {
                wm.setConnectRetries(2);
            }
            #ifdef REMOTE_HAVEMQTT
            useMQTT = false;
            #endif
            connectedToTCDAP = true;
            // Unlike the other props, we don't
            // need a delay here, the Remote is
            // not powered up together with the
            // other props.
        }
    } else {
        // No point in retry when we have no WiFi config'd
        wm.setConnectRetries(1);
    }

    // No WiFi powersave features for STA mode here
    wifiOffDelay = origWiFiOffDelay = 0;

    // Eval AP-mode powersave delay
    wifiAPOffDelay = (unsigned long)atoi(settings.wifiAPOffDelay);
    if(wifiAPOffDelay > 0 && wifiAPOffDelay < 10) wifiAPOffDelay = 10;
    wifiAPOffDelay *= (60 * 1000);
    
    // Configure static IP
    if(loadIpSettings()) {
        setupStaticIP();
    }
    
    wifi_setup2();
}

void wifi_setup2()
{
    // Connect, but defer starting the CP
    wifiConnect(false, true);

    #ifdef REMOTE_MDNS
    if(MDNS.begin(settings.hostName)) {
        MDNS.addService("http", "tcp", 80);
    }
    #endif

    checkForUpdate();

#ifdef REMOTE_HAVEMQTT
    if((!settings.mqttServer[0]) || // No server -> no MQTT
       (wifiInAPMode))              // WiFi in AP mode -> no MQTT
        useMQTT = false;  
    
    if(useMQTT) {

        uint16_t mqttPort = 1883;
        char *t;
        int tt;

        // No WiFi power save if we're using MQTT
        origWiFiOffDelay = wifiOffDelay = 0;

        mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
        mqttClient.setVersion(atoi(settings.mqttVers) > 0 ? 5 : 3);
        mqttClient.setClientID(settings.hostName);
        
        if((t = strchr(settings.mqttServer, ':'))) {
            size_t ts = (t - settings.mqttServer) + 1;
            mqttServer = (char *)malloc(ts);
            memset(mqttServer, 0, ts);
            strncpy(mqttServer, settings.mqttServer, t - settings.mqttServer);
            tt = atoi(t + 1);
            if(tt > 0 && tt <= 65535) {
                mqttPort = tt;
            }
        } else {
            mqttServer = settings.mqttServer;
        }

        if(isIp(mqttServer)) {
            mqttClient.setServer(stringToIp(mqttServer), mqttPort);
        } else {
            IPAddress remote_addr;
            if(WiFi.hostByName(mqttServer, remote_addr)) {
                mqttClient.setServer(remote_addr, mqttPort);
            } else {
                mqttClient.setServer(mqttServer, mqttPort);
                // Disable PING if we can't resolve domain
                mqttDoPing = false;
                Serial.printf("MQTT: Failed to resolve '%s'\n", mqttServer);
            }
        }
        
        mqttClient.setCallback(mqttCallback);
        mqttClient.setLooper(mqttLooper);

        if(settings.mqttUser[0] != 0) {
            if((t = strchr(settings.mqttUser, ':'))) {
                size_t ts = strlen(settings.mqttUser) + 1;
                mqttUser = (char *)malloc(ts);
                strcpy(mqttUser, settings.mqttUser);
                mqttUser[t - settings.mqttUser] = 0;
                mqttPass = mqttUser + (t - settings.mqttUser + 1);
            } else {
                mqttUser = settings.mqttUser;
            }
        }

        #ifdef REMOTE_DBG
        Serial.printf("MQTT: server '%s' port %d user '%s' pass '%s'\n", mqttServer, mqttPort, mqttUser, mqttPass);
        #endif
            
        mqttReconnect(true);
        // Rest done in loop
            
    } else {

        #ifdef REMOTE_DBG
        Serial.println("MQTT: Disabled");
        #endif

    }
#endif

    // Start the Config Portal
    if(WiFi.status() == WL_CONNECTED) {
        wifiStartCP();
    }

    wifiSetupDone = true;
}

static void setBool(char c, bool& b)
{
    if(c == '1') {
        b = true;
    } else if(c == '0') {
        b = false;
    }
} 

/*
 * wifi_loop()
 *
 */
void wifi_loop()
{
    char oldCfgOnSD = 0;

#ifdef REMOTE_HAVEMQTT
    if(useMQTT) {
        if(mqttClient.state() != MQTT_CONNECTING) {
            if(!mqttClient.connected()) {
                if(mqttOldState || mqttRestartPing) {
                    // Disconnection first detected:
                    mqttPingDone = mqttDoPing ? false : true;
                    mqttPingNow = mqttRestartPing ? millis() : 0;
                    mqttOldState = false;
                    mqttRestartPing = false;
                    mqttSubAttempted = false;
                }
                if(mqttDoPing && !mqttPingDone) {
                    audio_loop();
                    mqttPing();
                    audio_loop();
                }
                if(mqttPingDone) {
                    audio_loop();
                    mqttReconnect();
                    audio_loop();
                }
            } else {
                // Only call Subscribe() if connected
                mqttSubscribe();
                mqttOldState = true;
            }
        }
        mqttClient.loop();
    }
#endif
    
    if(shouldSaveIPConfig) {

        #ifdef REMOTE_DBG
        Serial.println("WiFi: Saving IP config");
        #endif

        mp_stop();
        stopAudio();

        writeIpSettings();

        shouldSaveIPConfig = false;

    } else if(shouldDeleteIPConfig) {

        #ifdef REMOTE_DBG
        Serial.println("WiFi: Deleting IP config");
        #endif

        mp_stop();
        stopAudio();

        deleteIpSettings();

        shouldDeleteIPConfig = false;

    }

    if(shouldSaveConfig) {

        int temp;
        bool write_main_settings = false;

        // Save settings and restart esp32

        mp_stop();
        stopAudio();

        #ifdef REMOTE_DBG
        Serial.println("Config Portal: Saving config");
        #endif

        if(shouldSaveConfig == 1) {

            // Parameters on WiFi Config page

            // Note: Parameters that need to grabbed from the server directly
            // through getParam() must be handled in preSaveWiFiCallback().

            // ssid, pass copied to settings in saveWiFiCallback()

            strcpytrim(settings.hostName, custom_hostName.getValue(), true);
            if(!*settings.hostName) {
                strcpy(settings.hostName, DEF_HOSTNAME);
            } else {
                char *s = settings.hostName;
                for ( ; *s; ++s) *s = tolower(*s);
            }
            mystrcpy(settings.wifiConRetries, &custom_wifiConRetries);
            mystrcpy(settings.wifiConTimeout, &custom_wifiConTimeout);
            evalCB(settings.reconOnFP, &custom_reconAtmp);
            
            strcpytrim(settings.systemID, custom_sysID.getValue(), true);
            strcpytrim(settings.appw, custom_appw.getValue(), true);
            if((temp = strlen(settings.appw)) > 0) {
                if(temp < 8) {
                    settings.appw[0] = 0;
                }
            }
            mystrcpy(settings.wifiAPOffDelay, &custom_wifiAPOffDelay);
            evalCB(settings.reactAPOnFP, &custom_reactAP);

            write_main_settings = true;

        } else if(shouldSaveConfig == 2) {

            // Parameters on Settings page

            // Extract settings saved only as secSettings
            evalCB(settings.movieMode, &custom_sStrict);
            setBool(settings.movieMode[0], movieMode);
            evalCB(settings.dgps, &custom_dGPS);
            setBool(settings.dgps[0], displayGPSMode);
            evalCB(settings.upd, &custom_upd);
            setBool(settings.upd[0], showUpdAvail);
            saveAllSecCP();

            // Handle settings also saved as terSettings (except MF, only terSetting)
            if(haveSD) {
                mystrcpy(settings.musicFolder, &custom_musicFolder);
                if(*settings.musicFolder) {
                    temp = atoi(settings.musicFolder);
                    if(temp >= 0 && temp <= 9) {
                        musFolderNum = temp;
                    }
                }
            }
            evalCB(settings.autoThrottle, &custom_at);
            evalCB(settings.coast, &custom_coast);
            evalCB(settings.pwrMst, &custom_pwrMst);
            setBool(settings.autoThrottle[0], autoThrottle);
            setBool(settings.coast[0], doCoast);
            setBool(settings.pwrMst[0], powerMaster);
            updateVisMode();
            saveAllTerCP();

            evalCB(settings.playClick, &custom_playclick);
            evalCB(settings.playALsnd, &custom_playALSnd);

            strcpytrim(settings.tcdIP, custom_tcdIP.getValue());
            if(*settings.tcdIP) {
                char *s = settings.tcdIP;
                for ( ; *s; ++s) *s = tolower(*s);
            }
            // pwrMaster saved above
            
            oldCfgOnSD = settings.CfgOnSD[0];
            evalCB(settings.CfgOnSD, &custom_CfgOnSD);
            //evalCB(settings.sdFreq, &custom_sdFrq);

            getParam("oorst", settings.oorst, 1, DEF_OORST);
            getParam("oott", settings.ooTT, 1, DEF_OO_TT);

            #ifdef ALLOW_DIS_UB
            evalCB(settings.disBPack, &custom_dBP);
            #endif
            evalCB(settings.bPb0Maint, &custom_b0mt);
            evalCB(settings.bPb1Maint, &custom_b1mt);
            evalCB(settings.bPb2Maint, &custom_b2mt);
            evalCB(settings.bPb3Maint, &custom_b3mt);
            evalCB(settings.bPb4Maint, &custom_b4mt);
            evalCB(settings.bPb5Maint, &custom_b5mt);
            evalCB(settings.bPb6Maint, &custom_b6mt);
            evalCB(settings.bPb7Maint, &custom_b7mt);

            evalCB(settings.bPb0MtO, &custom_b0mtoo);
            evalCB(settings.bPb1MtO, &custom_b1mtoo);
            evalCB(settings.bPb2MtO, &custom_b2mtoo);
            evalCB(settings.bPb3MtO, &custom_b3mtoo);
            evalCB(settings.bPb4MtO, &custom_b4mtoo);
            evalCB(settings.bPb5MtO, &custom_b5mtoo);
            evalCB(settings.bPb6MtO, &custom_b6mtoo);
            evalCB(settings.bPb7MtO, &custom_b7mtoo);

            evalCB(settings.usePwrLED, &custom_uPL);
            evalCB(settings.pwrLEDonFP, &custom_PLD);
            evalCB(settings.useLvlMtr, &custom_uLM);
            evalCB(settings.LvLMtronFP, &custom_PMD);
            
            #ifdef HAVE_PM
            if(havePwrMon) {
                evalCB(settings.usePwrMon, &custom_UPM);
                getParam("bty", settings.batType, 1, DEF_BAT_TYPE);
                mystrcpy(settings.batCap, &custom_bca);
            }
            #endif

            // Copy secondary settings to other medium if
            // user changed respective option
            if(oldCfgOnSD != settings.CfgOnSD[0]) {
                moveSettings();
            }

            write_main_settings = true;

        } else if(shouldSaveConfig == 3) {

            // Parameters on HA/MQTT Settings page

            #ifdef REMOTE_HAVEMQTT
            evalCB(settings.useMQTT, &custom_useMQTT);
            strcpytrim(settings.mqttServer, custom_mqttServer.getValue());
            getParam("mprot", settings.mqttVers, 1, 0);
            strcpyutf8(settings.mqttUser, custom_mqttUser.getValue(), sizeof(settings.mqttUser));

            strcpyutf8(settings.mqttbt[0], custom_mqttb1t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[0], custom_mqttb1o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[0], custom_mqttb1f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[1], custom_mqttb2t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[1], custom_mqttb2o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[1], custom_mqttb2f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[2], custom_mqttb3t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[2], custom_mqttb3o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[2], custom_mqttb3f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[3], custom_mqttb4t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[3], custom_mqttb4o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[3], custom_mqttb4f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[4], custom_mqttb5t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[4], custom_mqttb5o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[4], custom_mqttb5f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[5], custom_mqttb6t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[5], custom_mqttb6o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[5], custom_mqttb6f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[6], custom_mqttb7t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[6], custom_mqttb7o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[6], custom_mqttb7f.getValue(), sizeof(settings.mqttbf[0]));
            strcpyutf8(settings.mqttbt[7], custom_mqttb8t.getValue(), sizeof(settings.mqttbt[0]));
            strcpyutf8(settings.mqttbo[7], custom_mqttb8o.getValue(), sizeof(settings.mqttbo[0]));
            strcpyutf8(settings.mqttbf[7], custom_mqttb8f.getValue(), sizeof(settings.mqttbf[0]));

            write_mqtt_settings();
            #endif
        }

        // Write settings if requested, or no settings file exists
        if(write_main_settings || !checkConfigExists()) {
            write_settings();
        }
        
        shouldSaveConfig = 0;

        // Reset esp32 to load new settings
        
        // Unregister from TCD
        bttfn_remote_unregister();
       
        #ifdef REMOTE_DBG
        Serial.println("Config Portal: Restarting ESP....");
        #endif
        Serial.flush();
        
        prepareReboot();
        delay(500);
        esp_restart();
    }

    // We skip web handling when we're in tcdIsInP0 mode
    // because this is time-critical.
    wm.process(!tcdIsInP0 || !FPBUnitIsOn);

    // WiFi power management
    // If a delay > 0 is configured, WiFi is powered-down after timer has
    // run out. The timer starts when the device is powered-up/boots.
    // There are separate delays for AP mode and STA mode.
    // WiFi will be re-enabled for the configured time during fake-power on.
    // Skip testing while in tcdIsInP0 mode unless fake-powered-off.
    if(!tcdIsInP0 || !FPBUnitIsOn) {
        if(wifiInAPMode) {
            // Disable WiFi in AP mode after a configurable delay (if > 0)
            if(wifiAPOffDelay > 0) {
                if(!wifiAPIsOff && (millis() - wifiAPModeNow >= wifiAPOffDelay)) {
                    wifiOff(false);
                    wifiAPIsOff = true;
                    wifiIsOff = false;
                    #ifdef REMOTE_DBG
                    Serial.println("WiFi (AP-mode) switched off (power-save)");
                    #endif
                }
            }
        } else {
            // Disable WiFi in STA mode after a configurable delay (if > 0)
            if(origWiFiOffDelay > 0) {
                if(!wifiIsOff && (millis() - wifiOnNow >= wifiOffDelay)) {
                    wifiOff(false);
                    wifiIsOff = true;
                    wifiAPIsOff = false;
                    #ifdef REMOTE_DBG
                    Serial.println("WiFi (STA-mode) switched off (power-save)");
                    #endif
                }
            }
        }
    }

}

static void wifiConnect(bool APonly, bool deferConfigPortal)
{
    bool doOnlyAP = false;
    char realAPName[16];

    strcpy(realAPName, apName);
    if(settings.systemID[0]) {
        strcat(realAPName, settings.systemID);
    }

    if(APonly || !settings.ssid[0]) {
        wm.startConfigPortal(realAPName, settings.appw, settings.ssid, settings.pass);
        doOnlyAP = true;
    }
    
    // Automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if(!doOnlyAP && wm.autoConnect(settings.ssid, settings.pass, realAPName, settings.appw)) {
        #ifdef REMOTE_DBG
        Serial.println("WiFi connected");
        #endif

        // We start the CP later
        if(!deferConfigPortal) {
            wm.startWebPortal();
        }

        // Allow modem sleep:
        // WIFI_PS_MIN_MODEM is the default, and activated when calling this
        // with "true". When this is enabled, received WiFi data can be
        // delayed for as long as the DTIM period.
        // Disable modem sleep, don't want delays accessing the CP or
        // with BTTFN/MQTT.
        WiFi.setSleep(false);

        // Set transmit power to max; we might be connecting as STA after
        // a previous period in AP mode.
        #ifdef REMOTE_DBG
        {
            wifi_power_t power = WiFi.getTxPower();
            Serial.printf("WiFi: Max TX power in STA mode %d\n", power);
        }
        #endif
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        wifiInAPMode = false;
        wifiIsOff = false;
        wifiOnNow = millis();
        wifiAPIsOff = false;  // Sic! Allows checks like if(wifiAPIsOff || wifiIsOff)

    } else {
        #ifdef REMOTE_DBG
        Serial.println("Config portal running in AP-mode");
        #endif

        {
            #ifdef REMOTE_DBG
            int8_t power;
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power %d\n", power);
            #endif

            // Try to avoid "burning" the ESP when the WiFi mode
            // is "AP" by reducing the max. transmit power.
            // The choices are:
            // WIFI_POWER_19_5dBm    = 19.5dBm
            // WIFI_POWER_19dBm      = 19dBm
            // WIFI_POWER_18_5dBm    = 18.5dBm
            // WIFI_POWER_17dBm      = 17dBm
            // WIFI_POWER_15dBm      = 15dBm
            // WIFI_POWER_13dBm      = 13dBm
            // WIFI_POWER_11dBm      = 11dBm
            // WIFI_POWER_8_5dBm     = 8.5dBm
            // WIFI_POWER_7dBm       = 7dBm     <-- proven to avoid any issues
            // WIFI_POWER_5dBm       = 5dBm
            // WIFI_POWER_2dBm       = 2dBm
            // WIFI_POWER_MINUS_1dBm = -1dBm
            WiFi.setTxPower(WIFI_POWER_7dBm);

            #ifdef REMOTE_DBG
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power set to %d\n", power);
            #endif
        }

        wifiInAPMode = true;
        wifiAPIsOff = false;
        wifiAPModeNow = millis();
        wifiIsOff = false;    // Sic!

    }
}

void wifiOff(bool force)
{
    if(!force) {
        if( (!wifiInAPMode && wifiIsOff) ||
            (wifiInAPMode && wifiAPIsOff) ) {
            return;
        }
    }

    // Parm for disableWiFi() is "waitForOFF"
    // which should be true if we stop in AP
    // mode and immediately re-connect, without
    // process()ing for a while after this call.
    // "force" is true if we want to try to
    // reconnect after disableWiFi(), false if 
    // we disconnect upon timer expiration, 
    // so it matches the purpose.
    // "false" also does not cause any delays,
    // while "true" may take up to 2 seconds.
    wm.disableWiFi(force);
}

void wifiOn(unsigned long newDelay)
{
    bool doOnlyAP = false;
    unsigned long desiredDelay;
    unsigned long Now = millis();
    
    // wifiON() is called when the user fake-powers off+on.
    //
    // Fake power down/up serves - apart from the actual fake power function -
    // additional two purposes: To re-enable WiFi if in power save mode, and 
    // to re-connect to a configured WiFi network if we failed to connect to 
    // that network at the last connection attempt. In both cases, the Config
    // Portal is started.
    //
    // "wifiInAPMode" only tells us our latest mode; if the configured WiFi
    // network was - for whatever reason - was not available when we
    // tried to (re)connect, "wifiInAPMode" is true.

    // At this point, wifiInAPMode reflects the state after
    // the last connection attempt.

    if(wifiInAPMode) {  // We are in AP mode

        if(!wifiAPIsOff) {

            // If ON but no user-config'd WiFi network or
            // disabled "Reconnect on FP" -> bail
            if(!wifiHaveSTAConf || !wifiReconOnFP) {
                // Best we can do is to restart the AP-PS timer
                // (if user selected "Re-enable AP on FP")
                if(wifiReactAPOnFP) wifiAPModeNow = Now;
                return;
            }

            // If ON and User has config'd a NW and wants reconnection attempts,
            // disable WiFi at this point in hope of successful connection below
            wifiOff(true);

        } else {

            // If OFF (PS), check if user has configured nw & wants reconnection
            // If not, see if user wants AP-reactivation
            if(!wifiHaveSTAConf || !wifiReconOnFP) {
                if(wifiReactAPOnFP) doOnlyAP = true;
                else return;
            }
            
        }

    } else {            // We are (or were) in STA mode

        // If WiFi is not off, start CP if not running
        if(!wifiIsOff && (WiFi.status() == WL_CONNECTED)) {
            if(!wm.getWebPortalActive()) {
                wm.startWebPortal();
            }
            // Restart timer
            wifiOnNow = Now;
            return;
        }

        // User does not want reconnection attempts on FP? Bail.
        if(!wifiReconOnFP) return;

    }

    // (Re)connect
    wifiConnect(doOnlyAP);

    // Restart timers
    // Note that wifiInAPMode now reflects the
    // result of our above wifiConnect() call

    if(wifiInAPMode) {

        #ifdef REMOTE_DBG
        Serial.println("wifiOn: in AP mode after connect");
        #endif
      
        wifiAPModeNow = Now;
        
        #ifdef REMOTE_DBG
        if(wifiAPOffDelay > 0) {
            Serial.printf("Restarting WiFi-off timer (AP mode); delay %d\n", wifiAPOffDelay);
        }
        #endif
        
    } else {

        #ifdef REMOTE_DBG
        Serial.println("wifiOn: in STA mode after connect");
        #endif

        if(origWiFiOffDelay) {
            desiredDelay = (newDelay > 0) ? newDelay : origWiFiOffDelay;
            if((Now - wifiOnNow >= wifiOffDelay) ||                    // If delay has run out, or
               (wifiOffDelay - (Now - wifiOnNow))  < desiredDelay) {   // new delay exceeds remaining delay:
                wifiOffDelay = desiredDelay;                           // Set new timer delay, and
                wifiOnNow = Now;                                       // restart timer
                #ifdef REMOTE_DBG
                Serial.printf("Restarting WiFi-off timer; delay %d\n", wifiOffDelay);
                #endif
            }
        }

    }
}

bool wifiNeedReConnect(bool &blocks)
{
    if(wifiInAPMode) {  // We are in AP mode

        if(!wifiAPIsOff) {

            // If ON, check if nw configured and user wants reconnection attempts
            if(!wifiHaveSTAConf || !wifiReconOnFP) {
              
                // No, but user wants to re-activate the AP: We restart timer here. NO.
                if(wifiReactAPOnFP) wifiAPModeNow = millis();
                return false;
            }

        } else {

            // If OFF, check if nw configured and user wants reconnection attempts
            if(!wifiHaveSTAConf || !wifiReconOnFP) {
                
                // No, but user wants to re-activate the AP: YES.
                if(wifiReactAPOnFP) return true;
                
                // User does not want re-activation or re-connection: NO.
                return false;
            }
          
        }

        // We have network & user wants reconnection: YES.
        blocks = true;
        return true;

    } else {            // We are (or were) in STA mode

        // User does not want reconnection attempts on FP? Bail.
        if(!wifiReconOnFP) return false;
          
        if(!wifiIsOff && (WiFi.status() == WL_CONNECTED)) {
            return !wm.getWebPortalActive();
        }

        blocks = true;
        return true;
    }
}

void wifiStartCP()
{
    if(wifiInAPMode || wifiIsOff)
        return;

    wm.startWebPortal();
}

static void checkForUpdate()
{
    *newversion = 0;
    if(!connectedToTCDAP && (WiFi.status() == WL_CONNECTED)) {
        IPAddress remote_addr;
        if(WiFi.hostByName(WEBHOME "v.out-a-ti.me", remote_addr)) {
            int curr = 0, revision = 0;
            if(sscanf(CURRVERSION, "V%d.%d", &curr, &revision) == 2) {
                if(((remote_addr[0] << 8) | remote_addr[1]) > ((curr << 8) | revision)) {
                    snprintf(newversion, sizeof(newversion), "%d.%d", remote_addr[0], remote_addr[1]);
                }
            }
        }
    }
}

bool updateAvailable()
{
    return (*newversion != 0);
}

// This is called when the WiFi config is to be saved. We set
// a flag for the loop to read out and save the new WiFi config.
// SSID and password are copied to settings here.
static void saveWiFiCallback(const char *ssid, const char *pass)
{
    // ssid is the (new?) ssid to connect to, pass the password.
    // (We don't need to compare to the old ones, the settings
    // hash will decide on save/not save.)
    // This is also used to "forget" a saved WiFi network, in
    // which case ssid and pass are empty strings.
    memset(settings.ssid, 0, sizeof(settings.ssid));
    memset(settings.pass, 0, sizeof(settings.pass));
    if(*ssid) {
        strncpy(settings.ssid, ssid, sizeof(settings.ssid) - 1);
        strncpy(settings.pass, pass, sizeof(settings.pass) - 1);
    }

    #ifdef REMOTE_DBG
    Serial.printf("saveWiFiCallback: New ssid '%s'\n", settings.ssid);
    Serial.printf("saveWiFiCallback: New pass '%s'\n", settings.pass);
    #endif
    
    shouldSaveConfig = 1;
}

// This is the callback from the actual Params page. We read out
// the WM "Settings" parameters and save them.
// paramspage is 1 or 2
static void saveParamsCallback(int paramspage)
{
    shouldSaveConfig = paramspage + 1;
}

// This is called before a firmware updated is initiated.
// Disable WiFi-off-timers, switch off audio, show "wait"
static void preUpdateCallback()
{
    wifiAPOffDelay = 0;
    origWiFiOffDelay = 0;

    remdisplay.blink(false);
    remledStop.setState(false);

    // Unregister from TCD
    bttfn_remote_unregister();

    mp_stop();
    stopAudio();

    flushDelayedSave();

    showWaitSequence();
    remdisplay.on();
}

// This is called after a firmware updated has finished.
// parm = true of ok, false if error. WM reboots only 
// if the update worked, ie when res is true.
static void postUpdateCallback(bool res)
{
    Serial.flush();
    prepareReboot();

    // WM does not reboot on OTA update errors.
    // However, don't bother for that really
    // rare case to put code here to restore
    // under all possible circumstances, like
    // fake-off, time-travel going on, ss, ....
    if(!res) {
        delay(1000);
        esp_restart();
    }
}

// Grab static IP and other parameters from WiFiManager's server.
// Since there is no public method for this, we steal the HTML
// form parameters in this callback.
static void preSaveWiFiCallback()
{
    char ipBuf[20] = "";
    char gwBuf[20] = "";
    char snBuf[20] = "";
    char dnsBuf[20] = "";
    bool invalConf = false;

    #ifdef REMOTE_DBG
    Serial.println("preSaveConfigCallback");
    #endif

    // clear as strncpy might leave us unterminated
    memset(ipBuf, 0, 20);
    memset(gwBuf, 0, 20);
    memset(snBuf, 0, 20);
    memset(dnsBuf, 0, 20);

    String temp;
    temp.reserve(16);
    if((temp = wm.server->arg(FPSTR(WMS_ip))) != "") {
        strncpy(ipBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_sn))) != "") {
        strncpy(snBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_gw))) != "") {
        strncpy(gwBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_dns))) != "") {
        strncpy(dnsBuf, temp.c_str(), 19);
    } else invalConf |= true;

    #ifdef REMOTE_DBG
    if(strlen(ipBuf) > 0) {
        Serial.printf("IP:%s / SN:%s / GW:%s / DNS:%s\n", ipBuf, snBuf, gwBuf, dnsBuf);
    } else {
        Serial.println("Static IP unset, using DHCP");
    }
    #endif

    if(!invalConf && isIp(ipBuf) && isIp(gwBuf) && isIp(snBuf) && isIp(dnsBuf)) {

        #ifdef REMOTE_DBG
        Serial.println("All IPs valid");
        #endif

        shouldSaveIPConfig = (strcmp(ipsettings.ip, ipBuf)      ||
                              strcmp(ipsettings.gateway, gwBuf) ||
                              strcmp(ipsettings.netmask, snBuf) ||
                              strcmp(ipsettings.dns, dnsBuf));
          
        if(shouldSaveIPConfig) {
            strcpy(ipsettings.ip, ipBuf);
            strcpy(ipsettings.gateway, gwBuf);
            strcpy(ipsettings.netmask, snBuf);
            strcpy(ipsettings.dns, dnsBuf);
        }

    } else {

        #ifdef REMOTE_DBG
        if(strlen(ipBuf) > 0) {
            Serial.println("Invalid IP");
        }
        #endif

        shouldDeleteIPConfig = true;

    }

    // Other parameters on WiFi Config page that
    // need grabbing directly from the server

    getParam("apchnl", settings.apChnl, 2, DEF_AP_CHANNEL);
}

bool checkIPConfig()
{
    return (strlen(ipsettings.ip) > 0 &&
            isIp(ipsettings.ip)       &&
            isIp(ipsettings.gateway)  &&
            isIp(ipsettings.netmask)  &&
            isIp(ipsettings.dns));
}

static void setupStaticIP()
{
    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress dns;

    if(checkIPConfig()) {

        ip = stringToIp(ipsettings.ip);
        gw = stringToIp(ipsettings.gateway);
        sn = stringToIp(ipsettings.netmask);
        dns = stringToIp(ipsettings.dns);

        wm.setSTAStaticIPConfig(ip, gw, sn, dns);
    }
}

static int menuOutLenCallback()
{
    int mySize = 0;

    if(!haveAudioFiles || *newversion) {
        mySize += STRLEN(menu_myDiv) + STRLEN(menu_end);
        mySize += (!haveAudioFiles) ? (STRLEN(menu_myNoSP) + 4) : (STRLEN(menu_myUAI) + strlen(newversion));
    }

    return mySize;
}

static void menuOutCallback(String& page, unsigned int ssize)
{       
    if(!haveAudioFiles || *newversion) {
        page += menu_myDiv;
        if(!haveAudioFiles) {
            page += menu_myNoSP;
            page += rspv;
        } else {
            page += menu_myUAI;
            page += newversion;
        }
        page += menu_end;
    }
}

static bool preWiFiScanCallback()
{
    // Do not allow a WiFi scan under some circumstances (as
    // it may disrupt sequences)
    
    if(blockScan || TTrunning || tcdIsInP0 || throttlePos || keepCounting || calibMode)
        return false;

    return true;
}

static void wifiDelayReplacement(unsigned int mydel)
{
    if((mydel > 30) && audioInitDone) {
        unsigned long startNow = millis();
        while(millis() - startNow < mydel) {
            audio_loop();
            delay(20);
        }
    } else {
        delay(mydel);
    }
}

void gpCallback(int reason)
{
    // Called when WM does stuff that might
    // take some time, like before and after
    // HTTPSend().
    // MUST NOT call wifi_loop() !!!
    
    if(audioInitDone) {
        switch(reason) {
        case WM_LP_PREHTTPSEND:
        case WM_LP_NONE:
        case WM_LP_POSTHTTPSEND:
            audio_loop();
            break;
        }
    }
}

static void updateConfigPortalValues()
{
    // Make sure the settings form has the correct values

    custom_hostName.setValue(settings.hostName, 31);
    custom_wifiConTimeout.setValue(settings.wifiConTimeout, 2);
    custom_wifiConRetries.setValue(settings.wifiConRetries, 2);
    setCBVal(&custom_reconAtmp, settings.reconOnFP);

    custom_sysID.setValue(settings.systemID, 7);
    custom_appw.setValue(settings.appw, 8);
    // ap channel done on-the-fly
    custom_wifiAPOffDelay.setValue(settings.wifiAPOffDelay, 2);
    setCBVal(&custom_reactAP, settings.reactAPOnFP);
    
    setCBVal(&custom_playclick, settings.playClick);
    setCBVal(&custom_playALSnd, settings.playALsnd);

    custom_tcdIP.setValue(settings.tcdIP, 31);

    setCBVal(&custom_CfgOnSD, settings.CfgOnSD);
    //setCBVal(&custom_sdFrq, settings.sdFreq);

    // oorst done on-the-fly
    // oott done on-the-fly

    #ifdef ALLOW_DIS_UB
    setCBVal(&custom_dBP, settings.disBPack);
    #endif
    
    setCBVal(&custom_b0mt, settings.bPb0Maint);
    setCBVal(&custom_b1mt, settings.bPb1Maint);
    setCBVal(&custom_b2mt, settings.bPb2Maint);
    setCBVal(&custom_b3mt, settings.bPb3Maint);
    setCBVal(&custom_b4mt, settings.bPb4Maint);
    setCBVal(&custom_b5mt, settings.bPb5Maint);
    setCBVal(&custom_b6mt, settings.bPb6Maint);
    setCBVal(&custom_b7mt, settings.bPb7Maint);

    setCBVal(&custom_b0mtoo, settings.bPb0MtO);
    setCBVal(&custom_b1mtoo, settings.bPb1MtO);
    setCBVal(&custom_b2mtoo, settings.bPb2MtO);
    setCBVal(&custom_b3mtoo, settings.bPb3MtO);
    setCBVal(&custom_b4mtoo, settings.bPb4MtO);
    setCBVal(&custom_b5mtoo, settings.bPb5MtO);
    setCBVal(&custom_b6mtoo, settings.bPb6MtO);
    setCBVal(&custom_b7mtoo, settings.bPb7MtO);

    setCBVal(&custom_uPL, settings.usePwrLED);
    setCBVal(&custom_PLD, settings.pwrLEDonFP);
    setCBVal(&custom_uLM, settings.useLvlMtr);
    setCBVal(&custom_PMD, settings.LvLMtronFP);

    #ifdef HAVE_PM
    if(havePwrMon) {
        setCBVal(&custom_UPM, settings.usePwrMon);
        // Bat type done on-the-fly
        custom_bca.setValue(settings.batCap, 5);
    }
    #endif

    #ifdef REMOTE_HAVEMQTT
    setCBVal(&custom_useMQTT, settings.useMQTT);
    custom_mqttServer.setValue(settings.mqttServer, 79);
    custom_mqttUser.setValue(settings.mqttUser, 63);

    custom_mqttb1t.setValue(settings.mqttbt[0], 127);
    custom_mqttb1o.setValue(settings.mqttbo[0], 63);
    custom_mqttb1f.setValue(settings.mqttbf[0], 63);
    custom_mqttb2t.setValue(settings.mqttbt[1], 127);
    custom_mqttb2o.setValue(settings.mqttbo[1], 63);
    custom_mqttb2f.setValue(settings.mqttbf[1], 63);
    custom_mqttb3t.setValue(settings.mqttbt[2], 127);
    custom_mqttb3o.setValue(settings.mqttbo[2], 63);
    custom_mqttb3f.setValue(settings.mqttbf[2], 63);
    custom_mqttb4t.setValue(settings.mqttbt[3], 127);
    custom_mqttb4o.setValue(settings.mqttbo[3], 63);
    custom_mqttb4f.setValue(settings.mqttbf[3], 63);
    custom_mqttb5t.setValue(settings.mqttbt[4], 127);
    custom_mqttb5o.setValue(settings.mqttbo[4], 63);
    custom_mqttb5f.setValue(settings.mqttbf[4], 63);
    custom_mqttb6t.setValue(settings.mqttbt[5], 127);
    custom_mqttb6o.setValue(settings.mqttbo[5], 63);
    custom_mqttb6f.setValue(settings.mqttbf[5], 63);
    custom_mqttb7t.setValue(settings.mqttbt[6], 127);
    custom_mqttb7o.setValue(settings.mqttbo[6], 63);
    custom_mqttb7f.setValue(settings.mqttbf[6], 63);
    custom_mqttb8t.setValue(settings.mqttbt[7], 127);
    custom_mqttb8o.setValue(settings.mqttbo[7], 63);
    custom_mqttb8f.setValue(settings.mqttbf[7], 63);
    #endif
}

void updateConfigPortalMFValues()
{
    sprintf(settings.musicFolder, "%d", musFolderNum);
    custom_musicFolder.setValue(settings.musicFolder, 2);
}

static void setBoolAndUpdCB(bool myBool, char *sett, WiFiManagerParameter *wmParm)
{
    sett[0] = myBool ? '1' : '0';
    sett[1] = 0;
    setCBVal(wmParm, sett);
}

void updateConfigPortalVisValues()
{
    setBoolAndUpdCB(autoThrottle, settings.autoThrottle, &custom_at);
    setBoolAndUpdCB(doCoast, settings.coast, &custom_coast);
    setBoolAndUpdCB(powerMaster, settings.pwrMst, &custom_pwrMst);
}

void updateConfigPortalVis2Values()
{
    setBoolAndUpdCB(movieMode, settings.movieMode, &custom_sStrict);
    setBoolAndUpdCB(displayGPSMode, settings.dgps, &custom_dGPS);
}

void updateConfigPortalUpdValues()
{
    setBoolAndUpdCB(showUpdAvail, settings.upd, &custom_upd);
}

static const char *buildBanner(const char *msg, const char *col, int op) 
{   // "%s%s%s%s</div>";
    unsigned int l = STRLEN(bannerStart) + 7 + STRLEN(bannerMid) + strlen(msg) + 6 + 4;

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);
    sprintf(str, bannerGen, bannerStart, col, bannerMid, msg);        

    return str;
}

static unsigned int calcSelectMenu(const char **theHTML, int cnt, char *setting, bool indent = false)
{
    int sr = atoi(setting);

    unsigned int l = 0;

    l += STRLEN(custHTMLHdr1);
    if(indent) l += STRLEN(custHTMLHdrI);
    l += STRLEN(custHTMLHdr2);
    l += strlen(theHTML[0]);
    l += STRLEN(custHTMLSHdr);
    l += strlen(setting);
    l += (STRLEN(custHTMLSelFmt) - (2*2));
    l += (3*strlen(theHTML[1]));
    for(int i = 0; i < cnt - 2; i++) {
        if(sr == i) l += STRLEN(custHTMLSel);
        l += (strlen(theHTML[i+2]) - 2);
        l += strlen((i == cnt - 3) ? osde : ooe);
    }

    return l + 8;
}

static void buildSelectMenu(char *target, const char **theHTML, int cnt, char *setting, bool indent = false)
{
    int sr = atoi(setting);

    strcpy(target, custHTMLHdr1);
    if(indent) strcat(target, custHTMLHdrI);
    strcat(target, custHTMLHdr2);
    strcat(target, theHTML[1]);
    strcat(target, theHTML[0]);
    strcat(target, custHTMLSHdr);
    strcat(target, setting);
    sprintf(target + strlen(target), custHTMLSelFmt, theHTML[1], theHTML[1]);
    for(int i = 0; i < cnt - 2; i++) {
        if(sr == i) strcat(target, custHTMLSel);
        sprintf(target + strlen(target), 
            theHTML[i+2], (i == cnt - 3) ? osde : ooe);
    }
}


#ifdef HAVE_PM
static const char *wmBuildBatType(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = calcSelectMenu(batTypeHTMLSrc, 7, settings.batType);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);

    buildSelectMenu(str, batTypeHTMLSrc, 7, settings.batType);
    
    return str;
}
#endif

static const char *wmBuildApChnl(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = calcSelectMenu(apChannelCustHTMLSrc, 14, settings.apChnl);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);

    buildSelectMenu(str, apChannelCustHTMLSrc, 14, settings.apChnl);
    
    return str;
}

static const char *wmBuildBestApChnl(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    int32_t mychan = 0;
    int qual = 0;

    if(wm.getBestAPChannel(mychan, qual)) {
        unsigned int l = STRLEN(bestAP) - (5*2) + STRLEN(bannerStart) + 6 + STRLEN(bannerMid) + 4 + STRLEN(badWiFi) + 1 + 8;
        if(op == WM_CP_LEN) {
            wmLenBuf = l;
            return (const char *)&wmLenBuf;
        }
        char *str = (char *)malloc(l);
        sprintf(str, bestAP, bannerStart, qual < 0 ? col_r : (qual > 0 ? col_g : col_gr), bannerMid, mychan, qual < 0 ? badWiFi : "");
        return str;
    }

    return NULL;
}

static const char *wmBuildHaveSD(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }
    
    if(haveSD)
        return NULL;

    return buildBanner(haveNoSD, col_r, op);
}

static unsigned int lengthRadioButtons(const char **theHTML, int cnt, char *setting)
{
    unsigned int mysize = STRLEN(rad0) + strlen(theHTML[0]) + strlen(theHTML[1]);
    int i, j = strlen(theHTML[2]), sr = atoi(setting);
    
    for(i = 0; i < cnt; i++) {
        mysize += STRLEN(rad1) + (3*j) + (3*2) + ((i==sr) ? STRLEN(radchk) : 0) + strlen(theHTML[3+i]);
    }
    mysize += STRLEN(rad99);

    return mysize;
}

static void buildRadioButtons(char *target, const char **theHTML, int cnt, char *setting)
{
    int i, sr = atoi(setting);
    
    sprintf(target, rad0, theHTML[0]);
    strcat(target, theHTML[1]);
    
    for(i = 0; i < cnt; i++) {
        sprintf(target+strlen(target), rad1, theHTML[2], i, theHTML[2], i, (i==sr) ? radchk : "", theHTML[2], i, theHTML[3+i]);
    }
    strcat(target, rad99);
}

static const char *wmBuildOORST(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = lengthRadioButtons(oorstCustHTMLSrc, 2, settings.oorst);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);
    buildRadioButtons(str, oorstCustHTMLSrc, 2, settings.oorst);
        
    return str;
}

static const char *wmBuildOOTT(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = lengthRadioButtons(oottCustHTMLSrc, 2,  settings.ooTT);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);
    buildRadioButtons(str, oottCustHTMLSrc, 2, settings.ooTT);
        
    return str;
}

#ifdef REMOTE_HAVEMQTT
static const char *wmBuildMQTTprot(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = calcSelectMenu(mqttpCustHTMLSrc, 4, settings.mqttVers);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);

    buildSelectMenu(str, mqttpCustHTMLSrc, 4, settings.mqttVers);
    
    return str;
}

static const char *wmBuildMQTTstate(const char *dest, int op)
{
    // Check original setting, not "useMQTT" which
    // might be overruled.
    if(!evalBool(settings.useMQTT)) {
        return NULL;
    }
    
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    int s = 0;
    const char *msg = NULL;
    const char *cls = col_r;

    if(!useMQTT) {
        msg = mqttMsgDisabled;
        cls = col_gr;
    } else {
        s = mqttClient.state();
        switch(s) {
        case MQTT_CONNECTED:
            msg = mqttMsgConnected;
            cls = col_g;
            break;
        case MQTT_CONNECTING:
            msg = mqttMsgConnecting;
            cls = col_gr;
            break;
        case MQTT_CONNECTION_TIMEOUT:
            msg = mqttMsgTimeout;
            break;
        case MQTT_CONNECTION_LOST:
        case MQTT_CONNECT_FAILED:
            msg = mqttMsgFailed;
            break;
        case MQTT_DISCONNECTED:
            msg = mqttMsgDisconnected;
            break;
        case MQTT_CONNECT_BAD_PROTOCOL:
        case MQTT_CONNECT_BAD_CLIENT_ID:
        case 129:
        case 130:
        case 132:
        case 133:
            msg = mqttMsgBadProtocol;
            break;
        case MQTT_CONNECT_UNAVAILABLE:
        case 136:
        case 137:
            msg = mqttMsgUnavailable;
            break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
        case MQTT_CONNECT_UNAUTHORIZED:
        case 134:
        case 135:
        case 138:
        case 140:
        case 156:
        case 157:
            msg = mqttMsgBadCred;
            break;
        default:
            msg = mqttMsgGenError;
            break;
        }
    }

    // "%s%s%s%s%s (%d)</div>"
    unsigned int l = STRLEN(mqttStatus) - (6*2) + STRLEN(bannerStart) + strlen(cls) + 20 + STRLEN(bannerMid) + strlen(msg) + 6;

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);

    sprintf(str, mqttStatus, bannerStart, cls, ";margin-bottom:10px", bannerMid, msg, s);

    return str;
}
#endif


/*
 * Audio data uploader
 */

static void doReboot()
{
    delay(1000);
    prepareReboot();
    delay(500);
    esp_restart();
}

static void allocUplArrays()
{
    if(opType) free((void *)opType);
    opType = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));
    if(ACULerr) free((void *)ACULerr);
    ACULerr = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));;
    memset(opType, 0, MAX_SIM_UPLOADS * sizeof(int));
    memset(ACULerr, 0, MAX_SIM_UPLOADS * sizeof(int));
}

static void setupWebServerCallback()
{
    wm.server->on(R_updateacdone, HTTP_POST, &handleUploadDone, &handleUploading);
}

static void doCloseACFile(int idx, bool doRemove)
{
    if(haveACFile) {
        closeACFile(acFile);
        haveACFile = false;
    }
    if(doRemove) removeACFile(idx);  
}

static void handleUploading()
{
    HTTPUpload& upload = wm.server->upload();

    if(upload.status == UPLOAD_FILE_START) {

          String c = upload.filename;
          const char *illChrs = "|~><:*?\" ";
          int temp;
          char tempc;

          if(numUploads >= MAX_SIM_UPLOADS) {
            
              haveACFile = false;

          } else {

              c.toLowerCase();
    
              // Remove path and some illegal characters
              tempc = '/';
              for(int i = 0; i < 2; i++) {
                  if((temp = c.lastIndexOf(tempc)) >= 0) {
                      if(c.length() - 1 > temp) {
                          c = c.substring(temp);
                      } else {
                          c = "";
                      }
                      break;
                  }
                  tempc = '\\';
              }
              for(int i = 0; i < strlen(illChrs); i++) {
                  c.replace(illChrs[i], '_');
              }
              if(!c.indexOf("..")) {
                  c.replace("..", "");
              }
    
              if(!numUploads) {
                  allocUplArrays();
                  preUpdateCallback();
              }
    
              haveACFile = openUploadFile(c, acFile, numUploads, haveAC, opType[numUploads], ACULerr[numUploads]);

              if(haveACFile && opType[numUploads] == 1) {
                  haveAC = true;
              }

          }

    } else if(upload.status == UPLOAD_FILE_WRITE) {

          if(haveACFile) {
              if(writeACFile(acFile, upload.buf, upload.currentSize) != upload.currentSize) {
                  doCloseACFile(numUploads, true);
                  ACULerr[numUploads] = UPL_WRERR;
              }
          }

    } else if(upload.status == UPLOAD_FILE_END) {

        if(numUploads < MAX_SIM_UPLOADS) {

            doCloseACFile(numUploads, false);
    
            if(opType[numUploads] >= 0) {
                renameUploadFile(numUploads);
            }
    
            numUploads++;

        }
      
    } else if(upload.status == UPLOAD_FILE_ABORTED) {

        if(numUploads < MAX_SIM_UPLOADS) {
            doCloseACFile(numUploads, true);
        }

        doReboot();

    }

    delay(0);
}

static void handleUploadDone()
{
    const char *ebuf = "ERROR";
    const char *dbuf = "DONE";
    char *buf = NULL;
    bool haveErrs = false;
    bool haveAC = false;
    int titStart = -1;
    int buflen  = strlen(wm.getHTTPSTART(titStart)) +
                  STRLEN(myTitle)    +
                  strlen(wm.getHTTPSCRIPT()) +
                  strlen(wm.getHTTPSTYLE()) +
                  STRLEN(myHead)     +
                  STRLEN(acul_part3) +
                  STRLEN(myTitle)    +
                  STRLEN(acul_part5) +
                  STRLEN(apName)     +
                  STRLEN(acul_part6) +
                  STRLEN(acul_part8) +
                  1;

    for(int i = 0; i < numUploads; i++) {
        if(opType[i] > 0) {
            haveAC = true;
            if(!ACULerr[i]) {
                if(!check_if_default_audio_present()) {
                    haveAC = false;
                    ACULerr[i] = UPL_BADERR;
                    removeACFile(i);
                }
            }
            break;
        }
    }    

    if(!haveSD && numUploads) {
      
        buflen += (STRLEN(acul_part71) + strlen(acul_errs[1]));
        
    } else {

        for(int i = 0; i < numUploads; i++) {
            if(ACULerr[i]) haveErrs = true;
        }
        if(haveErrs) {
            buflen += STRLEN(acul_part71);
            for(int i = 0; i < numUploads; i++) {
                if(ACULerr[i]) {
                    buflen += getUploadFileNameLen(i);
                    buflen += 2; // :_
                    buflen += strlen(acul_errs[ACULerr[i]-1]);
                    buflen += 4; // <br>
                }
            }
        } else {
            buflen += strlen(wm.getHTTPSTYLEOK());
            buflen += STRLEN(acul_part7);
        }
        if(haveAC) {
            buflen += STRLEN(acul_part7a);
        }
    }

    buflen += 8;

    if(!(buf = (char *)malloc(buflen))) {
        buf = (char *)(haveErrs ? ebuf : dbuf);
    } else {
        strcpy(buf, wm.getHTTPSTART(titStart));
        if(titStart >= 0) {
            strcpy(buf + titStart, myTitle);
            strcat(buf, "</title>");
        }
        strcat(buf, wm.getHTTPSCRIPT());
        strcat(buf, wm.getHTTPSTYLE());
        if(!haveErrs) {
            strcat(buf, wm.getHTTPSTYLEOK());
        }
        strcat(buf, myHead);
        strcat(buf, acul_part3);
        strcat(buf, myTitle);
        strcat(buf, acul_part5);
        strcat(buf, apName);
        strcat(buf, acul_part6);

        if(!haveSD && numUploads) {

            strcat(buf, acul_part71);
            strcat(buf, acul_errs[1]);
            
        } else {
            
            if(haveErrs) {
                strcat(buf, acul_part71);
                for(int i = 0; i < numUploads; i++) {
                    if(ACULerr[i]) {
                        char *t = getUploadFileName(i);
                        if(t) {
                            strcat(buf, t);
                        }
                        strcat(buf, ": ");
                        strcat(buf, acul_errs[ACULerr[i]-1]);
                        strcat(buf, "<br>");
                    }
                }
            } else {
                strcat(buf, acul_part7);
            }
            if(haveAC) {
                strcat(buf, acul_part7a);
            }
        }

        strcat(buf, acul_part8);
    }

    freeUploadFileNames();
    /* Pointless, we reboot
    numUploads = 0;
    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        opType[i] = 0;
        ACULerr[i] = 0;
    }
    */
    
    String str(buf);
    wm.server->send(200, "text/html", str);

    // Reboot required even for mp3 upload, because for most files, we check
    // during boot if they exist (to avoid repeatedly failing open() calls)

    doReboot();
}

bool wifi_getIP(uint8_t& a, uint8_t& b, uint8_t& c, uint8_t& d)
{
    IPAddress myip;

    switch(WiFi.getMode()) {
      case WIFI_MODE_STA:
          myip = WiFi.localIP();
          break;
      case WIFI_MODE_AP:
      case WIFI_MODE_APSTA:
          myip = WiFi.softAPIP();
          break;
      default:
          a = b = c = d = 0;
          return true;
    }

    a = myip[0];
    b = myip[1];
    c = myip[2];
    d = myip[3];

    return true;
}

// Check if String is a valid IP address
bool isIp(char *str)
{
    int segs = 0;
    int digcnt = 0;
    int num = 0;

    while(*str) {

        if(*str == '.') {

            if(!digcnt || (++segs == 4))
                return false;

            num = digcnt = 0;
            str++;
            continue;

        } else if((*str < '0') || (*str > '9')) {

            return false;

        }

        if((num = (num * 10) + (*str - '0')) > 255)
            return false;

        digcnt++;
        str++;
    }

    if(segs == 3) 
        return true;

    return false;
}

// String to IPAddress
static IPAddress stringToIp(char *str)
{
    int ip1, ip2, ip3, ip4;

    sscanf(str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);

    return IPAddress(ip1, ip2, ip3, ip4);
}

/*
 * Read parameter from server, for customhmtl input
 */

static void getParam(String name, char *destBuf, size_t length, int defaultVal)
{
    memset(destBuf, 0, length+1);
    if(wm.server->hasArg(name)) {
        strncpy(destBuf, wm.server->arg(name).c_str(), length);
    }
    if(!*destBuf) {
        sprintf(destBuf, "%d", defaultVal);
    }
}

static bool myisspace(char mychar)
{
    return (mychar == ' ' || mychar == '\n' || mychar == '\t' || mychar == '\v' || mychar == '\f' || mychar == '\r');
}

static bool myisgoodchar(char mychar)
{
    return ((mychar >= '0' && mychar <= '9') || (mychar >= 'a' && mychar <= 'z') || (mychar >= 'A' && mychar <= 'Z') || mychar == '-');
}

static char* strcpytrim(char* destination, const char* source, bool doFilter)
{
    char *ret = destination;
    
    while(*source) {
        if(!myisspace(*source) && (!doFilter || myisgoodchar(*source))) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}

static void mystrcpy(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, el->getValue());
}

static void evalCB(char *sv, WiFiManagerParameter *el)
{
    *sv++ = (*(el->getValue()) == '0') ? '0' : '1';
    *sv = 0;
}

static void setCBVal(WiFiManagerParameter *el, char *sv)
{
    el->setValue((*sv == '0') ? "0" : "1", 1);
}

#ifdef REMOTE_HAVEMQTT
static void truncateUTF8(char *src)
{
    int i, slen = strlen(src);
    unsigned char c, e;

    for(i = 0; i < slen; i++) {
        c = (unsigned char)src[i];
        e = 0;
        if     (c >= 192 && c < 224)  e = 1;
        else if(c >= 224 && c < 240)  e = 2;
        else if(c >= 240 && c < 248)  e = 3;  // Invalid UTF8 >= 245, but consider 4-byte char anyway
        if(e) {
            if((i + e) < slen) {
                i += e;
            } else {
                src[i] = 0;
                return;
            }
        }
    }
}

static void strcpyutf8(char *dst, const char *src, unsigned int len)
{
    strncpy(dst, src, len - 1);
    dst[len - 1] = 0;
    truncateUTF8(dst);
}

static void mqttLooper()
{
    audio_loop();
}

static uint16_t a2i(char *p)
{
    unsigned int t = 0;
    t += (*p++ - '0') * 1000;
    t += (*p++ - '0') * 100;
    t += (*p++ - '0') * 10;
    t += (*p - '0');

    return (uint16_t)t;
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    int i = 0, j, ml = (length <= 255) ? length : 255;
    char tempBuf[256];
    static const char *cmdList[] = {
      "PLAYKEY_",         // 0  PLAYKEY_1..PLAYKEY_9, PLAYKEY_1L..PLAYKEY9L
      "STOPKEY",          // 1
      "AUTOTHROTTLE_ON",  // 2
      "AUTOTHROTTLE_OFF", // 3
      "COASTING_ON",      // 4
      "COASTING_OFF",     // 5
      "MOVIEACCEL_ON",    // 6
      "MOVIEACCEL_OFF",   // 7
      "DISPTCDSPD_ON",    // 8
      "DISPTCDSPD_OFF",   // 9
      "MP_SHUFFLE_ON",    // 10 
      "MP_SHUFFLE_OFF",   // 11
      "MP_PLAY",          // 12
      "MP_STOP",          // 13
      "MP_NEXT",          // 14
      "MP_PREV",          // 15
      "MP_FOLDER_",       // 16  MP_FOLDER_0..MP_FOLDER_9
      "INJECT_",          // 17
      NULL
    };
    static const char *cmdList2[] = {
      "PREPARE",          // 0
      "TIMETRAVEL",       // 1
      "REENTRY",          // 2
      "ABORT_TT",         // 3
      "ALARM",            // 4
      "WAKEUP",           // 5
      NULL
    };

    // Note: This might be called while we are in a
    // wait-delay-loop. Best to just set flags here
    // that are evaluated synchronously (=later).
    // Do not stuff that messes with display, input,
    // etc.

    if(!length) return;

    memcpy(tempBuf, (const char *)payload, ml);
    tempBuf[ml] = 0;
    for(j = 0; j < ml; j++) {
        if(tempBuf[j] >= 'a' && tempBuf[j] <= 'z') tempBuf[j] &= ~0x20;
    }

    if(!strcmp(topic, "bttf/tcd/pub")) {

        // Commands from TCD

        while(cmdList2[i]) {
            j = strlen(cmdList2[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList2[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList2[i]) return;

        switch(i) {
        case 0:
            // Prepare for TT. Comes at some undefined point,
            // an undefined time before the actual tt, and may
            // not come at all.
            if(remoteAllowed) {
                doPrepareTT = true;
            }
            break;
        case 1:
            // Trigger Time Travel (if not running already)
            if(FPBUnitIsOn && remoteAllowed && !TTrunning && !remBusy) {
                networkTimeTravel = true;
                networkReentry = false;
                networkAbort = false;
                if(strlen(tempBuf) == 20) {
                    networkLead = a2i(&tempBuf[11]);
                    networkP1 = a2i(&tempBuf[16]);
                } else {
                    networkLead = ETTO_LEAD;
                    networkP1 = 6600;
                }
            }
            break;
        case 2:   // Re-entry
            // Start re-entry (if TT currently running)
            if(TTrunning) {
                networkReentry = true;
            }
            break;
        case 3:   // Abort TT (TCD fake-powered down during TT)
            if(TTrunning || tcdIsInP0) {
                networkAbort = true;
            }
            break;
        case 4:
            networkAlarm = true;
            // Eval this at our convenience
            break;
        case 5:
            if(remoteAllowed) {
                doWakeup = true;
            }
            break;
        }
       
    } else if(!remBusy && !strcmp(topic, "bttf/remote/cmd")) {

        // User commands

        while(cmdList[i]) {
            j = strlen(cmdList[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList[i]) return;

        switch(i) {
        case 0:
            if(strlen(tempBuf) > j && tempBuf[j] >= '1' && tempBuf[j] <= '9') {
                bool l = (strlen(tempBuf) > j+1 && tempBuf[j+1] == 'L');
                addCmdQueue(500 + (uint32_t)(tempBuf[j] - '0') + (l ? 10 : 0));
            }
            break;
        case 10:
        case 11:
            addCmdQueue((i == 10) ? 555 : 222);
            break;
        case 16:
            if(strlen(tempBuf) > j && tempBuf[j] >= '0' && tempBuf[j] <= '9') {
                addCmdQueue(50 + (uint32_t)(tempBuf[j] - '0'));
            }
            break;
        case 17:
            if(strlen(tempBuf) > j) {
                addCmdQueue(atoi(tempBuf+j) | 0x80000000);
            }
            break;
        default:
            addCmdQueue(1000 + i);
        }
            
    } 
}

#ifdef REMOTE_DBG
#define MQTT_FAILCOUNT 6
#else
#define MQTT_FAILCOUNT 120
#endif

static void mqttPing()
{
    switch(mqttClient.pstate()) {
    case PING_IDLE:
        if(WiFi.status() == WL_CONNECTED) {
            if(!mqttPingNow || (millis() - mqttPingNow > mqttPingInt)) {
                mqttPingNow = millis();
                if(!mqttClient.sendPing()) {
                    // Mostly fails for internal reasons;
                    // skip ping test in that case
                    mqttDoPing = false;
                    mqttPingDone = true;  // allow mqtt-connect attempt
                }
            }
        }
        break;
    case PING_PINGING:
        if(mqttClient.pollPing()) {
            mqttPingDone = true;          // allow mqtt-connect attempt
            mqttPingNow = 0;
            mqttPingsExpired = 0;
            mqttPingInt = MQTT_SHORT_INT; // Overwritten on fail in reconnect
            // Delay re-connection for 5 seconds after first ping echo
            mqttReconnectNow = millis() - (mqttReconnectInt - 5000);
        } else if(millis() - mqttPingNow > 5000) {
            mqttClient.cancelPing();
            mqttPingNow = millis();
            mqttPingsExpired++;
            mqttPingInt = MQTT_SHORT_INT * (1 << (mqttPingsExpired / MQTT_FAILCOUNT));
            mqttReconnFails = 0;
        }
        break;
    } 
}

static bool mqttReconnect(bool force)
{
    bool success = false;

    if(useMQTT && (WiFi.status() == WL_CONNECTED)) {

        if(!mqttClient.connected()) {
    
            if(force || !mqttReconnectNow || (millis() - mqttReconnectNow > mqttReconnectInt)) {

                #ifdef REMOTE_DBG
                Serial.println("MQTT: Attempting to (re)connect");
                #endif
    
                if(strlen(mqttUser)) {
                    success = mqttClient.connect(mqttUser, strlen(mqttPass) ? mqttPass : NULL);
                } else {
                    success = mqttClient.connect();
                }
    
                mqttReconnectNow = millis();
                
                if(!success) {
                    mqttRestartPing = true;  // Force PING check before reconnection attempt
                    mqttReconnFails++;
                    if(mqttDoPing) {
                        mqttPingInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    } else {
                        mqttReconnectInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    }
                    #ifdef REMOTE_DBG
                    Serial.printf("MQTT: Failed to reconnect (%d)\n", mqttReconnFails);
                    #endif
                } else {
                    mqttReconnFails = 0;
                    mqttReconnectInt = MQTT_SHORT_INT;
                    #ifdef REMOTE_DBG
                    Serial.println("MQTT: Connected to broker, waiting for CONNACK");
                    #endif
                }
    
                return success;
            } 
        }
    }
      
    return true;
}

static void mqttSubscribe()
{
    // Meant only to be called when connected!
    if(!mqttSubAttempted) {
        if(!mqttClient.subscribe("bttf/remote/cmd", "bttf/tcd/pub")) {
            #ifdef REMOTE_DBG
            Serial.println("MQTT: Failed to subscribe to command topics");
            #endif
        }
        mqttSubAttempted = true;
    }
}

bool mqttState()
{
    return (useMQTT && mqttClient.connected());
}

void mqttPublish(const char *topic, const char *pl, unsigned int len)
{
    if(useMQTT) {
        mqttClient.publish(topic, (uint8_t *)pl, len, false);
    }
}           
#endif
