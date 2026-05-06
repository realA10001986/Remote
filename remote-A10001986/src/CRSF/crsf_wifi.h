
#ifdef HAVE_CRSF

static const char *wmBuildCRSFOM(const char *dest, int op);
static const char *wmBuildCRSFPR(const char *dest, int op);
static const char *wmBuildCRSFSU(const char *dest, int op);
static const char *wmBuildCRSFTR(const char *dest, int op);
static const char *wmBuildCRSFMP(const char *dest, int op);
static const char *wmBuildCRSFDP(const char *dest, int op);
static const char *wmBuildCRSFRC(const char *dest, int op);
static const char *wmBuildCRSFPC(const char *dest, int op);
static const char *wmBuildCRSFTC(const char *dest, int op);
static const char *wmBuildCRSFYC(const char *dest, int op);
static const char *wmBuildCRSFCAL(const char *dest, int op);

static void syncCRSFPortalBuffers();
static bool saveCRSFPortalInputSettings();
static void getServerParamOneBased(const char *name, char *destBuf, size_t length, int minval, int maxval, int defaultVal);

static const char *cOpModeCustHTMLSrc[4] = {
    "'>Operation mode",
    "copm",
    ">Legacy%s1'",
    ">ELRS/CRSF%s"
};
static const char *cPktRateCustHTMLSrc[7] = {
    "'>ELRS Packet rate",
    "cpktr",
    ">50 Hz%s1'",
    ">100 Hz%s2'",
    ">150 Hz%s3'",
    ">250 Hz%s4'",
    ">500 Hz%s"
};
static const char *cSpdUnitCustHTMLSrc[4] = {
    "'>Speed units",
    "cspdu",
    ">km/h%s1'",
    ">mph%s"
};
static const char *cTlmRatioCustHTMLSrc[9] = {
    "'>Telemetry Ratio",
    "ctlmr",
    ">Std%s1'",
    ">1:2%s2'",
    ">1:4%s3'",
    ">1:8%s4'",
    ">1:16%s5'",
    ">1:32%s6'",
    ">Off%s"
};
static const char *cMaxPowerCustHTMLSrc[8] = {
    "'>Max Power",
    "cmpwr",
    ">10 mW%s1'",
    ">25 mW%s2'",
    ">100 mW%s3'",
    ">250 mW%s4'",
    ">500 mW%s5'",
    ">1000 mW%s"
};
static const char *cDynPowerCustHTMLSrc[4] = {
    "'>Dynamic Power",
    "cdynp",
    ">Off%s1'",
    ">Dyn%s"
};
static const char *cRollChCustHTMLSrc[18] = {
    "'>Aileron target channel",
    "crlch",
    ">CH1%s1'",
    ">CH2%s2'",
    ">CH3%s3'",
    ">CH4%s4'",
    ">CH5%s5'",
    ">CH6%s6'",
    ">CH7%s7'",
    ">CH8%s8'",
    ">CH9%s9'",
    ">CH10%s10'",
    ">CH11%s11'",
    ">CH12%s12'",
    ">CH13%s13'",
    ">CH14%s14'",
    ">CH15%s15'",
    ">CH16%s"
};
static const char *cPitchChCustHTMLSrc[18] = {
    "'>Elevator target channel",
    "cptch",
    ">CH1%s1'",
    ">CH2%s2'",
    ">CH3%s3'",
    ">CH4%s4'",
    ">CH5%s5'",
    ">CH6%s6'",
    ">CH7%s7'",
    ">CH8%s8'",
    ">CH9%s9'",
    ">CH10%s10'",
    ">CH11%s11'",
    ">CH12%s12'",
    ">CH13%s13'",
    ">CH14%s14'",
    ">CH15%s15'",
    ">CH16%s"
};
static const char *cThrChCustHTMLSrc[18] = {
    "'>Throttle target channel",
    "cthch",
    ">CH1%s1'",
    ">CH2%s2'",
    ">CH3%s3'",
    ">CH4%s4'",
    ">CH5%s5'",
    ">CH6%s6'",
    ">CH7%s7'",
    ">CH8%s8'",
    ">CH9%s9'",
    ">CH10%s10'",
    ">CH11%s11'",
    ">CH12%s12'",
    ">CH13%s13'",
    ">CH14%s14'",
    ">CH15%s15'",
    ">CH16%s"
};
static const char *cYawChCustHTMLSrc[18] = {
    "'>Rudder target channel",
    "cywch",
    ">CH1%s1'",
    ">CH2%s2'",
    ">CH3%s3'",
    ">CH4%s4'",
    ">CH5%s5'",
    ">CH6%s6'",
    ">CH7%s7'",
    ">CH8%s8'",
    ">CH9%s9'",
    ">CH10%s10'",
    ">CH11%s11'",
    ">CH12%s12'",
    ">CH13%s13'",
    ">CH14%s14'",
    ">CH15%s15'",
    ">CH16%s"
};

WiFiManagerParameter custom_crsfom(wmBuildCRSFOM, WFM_SECTS_HEAD);
WiFiManagerParameter custom_ss_crsf("ELRS/CRSF Settings", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_crsfap("cAP", "Connect to WiFi in ELRS/CRSF mode<br><span>If unchecked, device will remain in AP mode.</span>", settings.crsfap, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_crsfpr(wmBuildCRSFPR);
WiFiManagerParameter custom_crsfsu(wmBuildCRSFSU);
WiFiManagerParameter custom_crsftr(wmBuildCRSFTR);
WiFiManagerParameter custom_crsfmp(wmBuildCRSFMP);
WiFiManagerParameter custom_crsfdp(wmBuildCRSFDP);
WiFiManagerParameter custom_crsfrc(wmBuildCRSFRC);
WiFiManagerParameter custom_crsfrr("crrv", "Reverse Aileron", settings.elrsRollRev, "class='mt5 ml20'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_crsfpc(wmBuildCRSFPC);
WiFiManagerParameter custom_crsfprv("cprv", "Reverse Elevator", settings.elrsPitchRev, "class='mt5 ml20'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_crsftc(wmBuildCRSFTC);
WiFiManagerParameter custom_crsftrv("ctrv", "Reverse Throttle", settings.elrsThrRev, "class='mt5 ml20'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_crsfyc(wmBuildCRSFYC);
WiFiManagerParameter custom_crsfyrv("cyrv", "Reverse Rudder", settings.elrsYawRev, "class='mt5 ml20'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_ss_crsfcal("ELRS/CRSF Gimbal Calibration", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_crsfcal(wmBuildCRSFCAL, WFM_FOOT);

WiFiManagerParameter *crsfParmArray[] = {
      &custom_crsfom,
      &custom_ss_crsf,
      &custom_crsfap,
      &custom_crsfpr,
      &custom_crsfsu,
      &custom_crsftr,
      &custom_crsfmp,
      &custom_crsfdp,
      &custom_crsfrc,
      &custom_crsfrr,
      &custom_crsfpc,
      &custom_crsfprv,
      &custom_crsftc,
      &custom_crsftrv,
      &custom_crsfyc,
      &custom_crsfyrv,
      &custom_ss_crsfcal,
      &custom_crsfcal,
      NULL
};



/*
 * Callback from wifi_loop() for saving settings
 *
 */
static bool crsf_wifi_loop_settings()
{
    evalCB(settings.crsfap, &custom_crsfap);
    if(saveCRSFPortalInputSettings()) {
        return true;
    }
    Serial.println("Config Portal: Failed to save CRSF input settings");
    return false;
}

/*
 * Callback from saveParamsCallback()
 *
 */
static void crsf_wifi_saveParamsCallback()
{
    getServerParam("copm", settings.opMode, 1, 0, 1, 0);
    getServerParam("cpktr", settings.elrsPktRate, 1, 0, 4, DEF_ELRSPKTRATE);
    getServerParam("cspdu", settings.elrsSpdUnit, 1, 0, 1, DEF_ELRSSPDUNIT);
    getServerParam("ctlmr", settings.elrsTlmRatio, 1, 0, 6, DEF_ELRSTLMRATIO);
    getServerParam("cmpwr", settings.elrsMaxPower, 1, 0, 5, DEF_ELRSMAXPOWER);
    getServerParam("cdynp", settings.elrsDynPower, 1, 0, 1, DEF_ELRSDYNPWR);
    getServerParamOneBased("crlch", settings.elrsRollCh, 2, 1, 16, DEF_ELRSROLLCH);
    getServerParamOneBased("cptch", settings.elrsPitchCh, 2, 1, 16, DEF_ELRSPITCHCH);
    getServerParamOneBased("cthch", settings.elrsThrCh, 2, 1, 16, DEF_ELRSTHRCH);
    getServerParamOneBased("cywch", settings.elrsYawCh, 2, 1, 16, DEF_ELRSYAWCH);
    getServerParam("crrv", settings.elrsRollRev, 1, 0, 1, 0);
    getServerParam("cprv", settings.elrsPitchRev, 1, 0, 1, 0);
    getServerParam("ctrv", settings.elrsThrRev, 1, 0, 1, 0);
    getServerParam("cyrv", settings.elrsYawRev, 1, 0, 1, 0);
    getServerParam("crrlo", settings.elrsRollLow, 5, 0, 2047, 0);
    getServerParam("crrct", settings.elrsRollCtr, 5, 0, 2047, 1024);
    getServerParam("crrhi", settings.elrsRollHigh, 5, 0, 2047, 2047);
    getServerParam("cptlo", settings.elrsPitchLow, 5, 0, 2047, 0);
    getServerParam("cptct", settings.elrsPitchCtr, 5, 0, 2047, 1024);
    getServerParam("cpthi", settings.elrsPitchHigh, 5, 0, 2047, 2047);
    getServerParam("cthlo", settings.elrsThrLow, 5, 0, 2047, 0);
    getServerParam("cthct", settings.elrsThrCtr, 5, 0, 2047, 1024);
    getServerParam("cthhi", settings.elrsThrHigh, 5, 0, 2047, 2047);
    getServerParam("cywlo", settings.elrsYawLow, 5, 0, 2047, 0);
    getServerParam("cywct", settings.elrsYawCtr, 5, 0, 2047, 1024);
    getServerParam("cywhi", settings.elrsYawHigh, 5, 0, 2047, 2047);
}


static void syncCRSFPortalBuffers()
{
    ELRSInputAxisProfile profiles[ELRS_GIMBAL_AXIS_COUNT];
    ELRSGimbalRouting routing;

    if(!haveNewBoard) {
        return;
    }

    loadELRSInputConfig(profiles, ELRS_GIMBAL_AXIS_COUNT, &routing);

    snprintf(settings.elrsRollCh, sizeof(settings.elrsRollCh), "%u", (unsigned)routing.aileronChannel);
    snprintf(settings.elrsPitchCh, sizeof(settings.elrsPitchCh), "%u", (unsigned)routing.elevatorChannel);
    snprintf(settings.elrsThrCh, sizeof(settings.elrsThrCh), "%u", (unsigned)routing.throttleChannel);
    snprintf(settings.elrsYawCh, sizeof(settings.elrsYawCh), "%u", (unsigned)routing.rudderChannel);

    settings.elrsRollRev[0] = profiles[ELRS_GIMBAL_INPUT_AILERON].reverse ? '1' : '0';
    settings.elrsRollRev[1] = 0;
    settings.elrsPitchRev[0] = profiles[ELRS_GIMBAL_INPUT_ELEVATOR].reverse ? '1' : '0';
    settings.elrsPitchRev[1] = 0;
    settings.elrsThrRev[0] = profiles[ELRS_GIMBAL_INPUT_THROTTLE].reverse ? '1' : '0';
    settings.elrsThrRev[1] = 0;
    settings.elrsYawRev[0] = profiles[ELRS_GIMBAL_INPUT_RUDDER].reverse ? '1' : '0';
    settings.elrsYawRev[1] = 0;

    snprintf(settings.elrsRollLow, sizeof(settings.elrsRollLow), "%d", profiles[ELRS_GIMBAL_INPUT_AILERON].minimum);
    snprintf(settings.elrsRollCtr, sizeof(settings.elrsRollCtr), "%d", profiles[ELRS_GIMBAL_INPUT_AILERON].center);
    snprintf(settings.elrsRollHigh, sizeof(settings.elrsRollHigh), "%d", profiles[ELRS_GIMBAL_INPUT_AILERON].maximum);

    snprintf(settings.elrsPitchLow, sizeof(settings.elrsPitchLow), "%d", profiles[ELRS_GIMBAL_INPUT_ELEVATOR].minimum);
    snprintf(settings.elrsPitchCtr, sizeof(settings.elrsPitchCtr), "%d", profiles[ELRS_GIMBAL_INPUT_ELEVATOR].center);
    snprintf(settings.elrsPitchHigh, sizeof(settings.elrsPitchHigh), "%d", profiles[ELRS_GIMBAL_INPUT_ELEVATOR].maximum);

    snprintf(settings.elrsThrLow, sizeof(settings.elrsThrLow), "%d", profiles[ELRS_GIMBAL_INPUT_THROTTLE].minimum);
    snprintf(settings.elrsThrCtr, sizeof(settings.elrsThrCtr), "%d", profiles[ELRS_GIMBAL_INPUT_THROTTLE].center);
    snprintf(settings.elrsThrHigh, sizeof(settings.elrsThrHigh), "%d", profiles[ELRS_GIMBAL_INPUT_THROTTLE].maximum);

    snprintf(settings.elrsYawLow, sizeof(settings.elrsYawLow), "%d", profiles[ELRS_GIMBAL_INPUT_RUDDER].minimum);
    snprintf(settings.elrsYawCtr, sizeof(settings.elrsYawCtr), "%d", profiles[ELRS_GIMBAL_INPUT_RUDDER].center);
    snprintf(settings.elrsYawHigh, sizeof(settings.elrsYawHigh), "%d", profiles[ELRS_GIMBAL_INPUT_RUDDER].maximum);
}

static bool saveCRSFPortalInputSettings()
{
    ELRSInputAxisProfile profiles[ELRS_GIMBAL_AXIS_COUNT];
    ELRSGimbalRouting routing;

    if(!haveNewBoard) {
        return false;
    }

    loadELRSInputConfig(profiles, ELRS_GIMBAL_AXIS_COUNT, &routing);

    routing.aileronChannel = (uint8_t)atoi(settings.elrsRollCh);
    routing.elevatorChannel = (uint8_t)atoi(settings.elrsPitchCh);
    routing.throttleChannel = (uint8_t)atoi(settings.elrsThrCh);
    routing.rudderChannel = (uint8_t)atoi(settings.elrsYawCh);

    profiles[ELRS_GIMBAL_INPUT_AILERON].reverse = (settings.elrsRollRev[0] != '0');
    profiles[ELRS_GIMBAL_INPUT_ELEVATOR].reverse = (settings.elrsPitchRev[0] != '0');
    profiles[ELRS_GIMBAL_INPUT_THROTTLE].reverse = (settings.elrsThrRev[0] != '0');
    profiles[ELRS_GIMBAL_INPUT_RUDDER].reverse = (settings.elrsYawRev[0] != '0');

    profiles[ELRS_GIMBAL_INPUT_AILERON].minimum = (int16_t)atoi(settings.elrsRollLow);
    profiles[ELRS_GIMBAL_INPUT_AILERON].center = (int16_t)atoi(settings.elrsRollCtr);
    profiles[ELRS_GIMBAL_INPUT_AILERON].maximum = (int16_t)atoi(settings.elrsRollHigh);

    profiles[ELRS_GIMBAL_INPUT_ELEVATOR].minimum = (int16_t)atoi(settings.elrsPitchLow);
    profiles[ELRS_GIMBAL_INPUT_ELEVATOR].center = (int16_t)atoi(settings.elrsPitchCtr);
    profiles[ELRS_GIMBAL_INPUT_ELEVATOR].maximum = (int16_t)atoi(settings.elrsPitchHigh);

    profiles[ELRS_GIMBAL_INPUT_THROTTLE].minimum = (int16_t)atoi(settings.elrsThrLow);
    profiles[ELRS_GIMBAL_INPUT_THROTTLE].center = (int16_t)atoi(settings.elrsThrCtr);
    profiles[ELRS_GIMBAL_INPUT_THROTTLE].maximum = (int16_t)atoi(settings.elrsThrHigh);

    profiles[ELRS_GIMBAL_INPUT_RUDDER].minimum = (int16_t)atoi(settings.elrsYawLow);
    profiles[ELRS_GIMBAL_INPUT_RUDDER].center = (int16_t)atoi(settings.elrsYawCtr);
    profiles[ELRS_GIMBAL_INPUT_RUDDER].maximum = (int16_t)atoi(settings.elrsYawHigh);

    return saveELRSInputConfig(profiles, ELRS_GIMBAL_AXIS_COUNT, &routing);
}

/*
 * Callback from saveParamsCallback()
 *
 */
static void crsf_wifi_updateConfigPortalValues()
{
    syncCRSFPortalBuffers();
    setCBVal(&custom_crsfap, settings.crsfap);
    setCBVal(&custom_crsfrr, settings.elrsRollRev);
    setCBVal(&custom_crsfprv, settings.elrsPitchRev);
    setCBVal(&custom_crsftrv, settings.elrsThrRev);
    setCBVal(&custom_crsfyrv, settings.elrsYawRev);
    // all others done on-the-fly
}

static const char *wmBuildSelectOneBased(const char *dest, int op, const char **src, int count, char *setting, bool indent = false)
{
    char tempSetting[3];
    int selectValue = atoi(setting);

    if(selectValue < 1) {
        selectValue = 1;
    } else if(selectValue > (count - 2)) {
        selectValue = count - 2;
    }

    snprintf(tempSetting, sizeof(tempSetting), "%d", selectValue - 1);

    return wmBuildSelect(dest, op, src, count, tempSetting, indent);
}

static void wmAppendCRSFCALPoint(String &html,
                                 const char *label,
                                 const char *inputId,
                                 const char *liveId,
                                 const char *value)
{
    html += "<div class='elrscal-row'><label for='";
    html += inputId;
    html += "'>";
    html += label;
    html += "</label><input id='";
    html += inputId;
    html += "' name='";
    html += inputId;
    html += "' maxlength='4' value='";
    html += value;
    html += "' type='number' min='0' max='2047'><button type='button' onclick=\"elrsCapture('";
    html += liveId;
    html += "','";
    html += inputId;
    html += "')\">Capture</button></div>";
}

static void wmAppendCRSFCALAxis(String &html,
                                const char *name,
                                const char *liveId,
                                const char *hint,
                                const char *lowId,
                                const char *lowValue,
                                const char *centerId,
                                const char *centerValue,
                                const char *highId,
                                const char *highValue)
{
    html += "<div class='elrscal-axis'><div class='elrscal-head'><span class='elrscal-name'>";
    html += name;
    html += "</span><span class='elrscal-live'>Live <span id='";
    html += liveId;
    html += "'>--</span></span></div><small class='elrscal-help'>";
    html += hint;
    html += "</small>";
    wmAppendCRSFCALPoint(html, "Low", lowId, liveId, lowValue);
    wmAppendCRSFCALPoint(html, "Center", centerId, liveId, centerValue);
    wmAppendCRSFCALPoint(html, "High", highId, liveId, highValue);
    html += "</div>";
}

static const char *wmBuildCRSFCAL(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    if(!opModeCRSF)
        return NULL;

    String html;

    html.reserve(7000);
    html += "<div class='cmp0 elrscal-wrap'><p style='font-size:0.85em;line-height:1.35em;margin:0 0 10px 0'>"
            "Capture each gimbal's raw ADC low, center, and high points here, then save this page. "
            "Those saved points become the real CRSF gimbal output mapping: low=1000, center=1500, high=2000."
            "</p>";
    html += "<div id='elrscalstat' style='font-size:0.8em;color:#444;margin:0 0 10px 0'>Live ADC: waiting for samples...</div>";
    html += "<style>"
            ".elrscal-wrap{box-sizing:border-box;width:100%;max-width:100%;padding:0;margin:0;white-space:normal;overflow-wrap:anywhere;overflow:hidden}"
            ".elrscal-wrap p,.elrscal-wrap #elrscalstat{white-space:normal;overflow-wrap:anywhere;max-width:100%}"
            ".elrscal-axis{box-sizing:border-box;width:100%;max-width:100%;margin:12px 0 0 0;padding:10px 0 0 0;border-top:1px solid #ddd;overflow:hidden;white-space:normal}"
            ".elrscal-head{display:block;line-height:1.3em;max-width:100%;overflow-wrap:anywhere;white-space:normal}"
            ".elrscal-name{font-weight:bold}"
            ".elrscal-live{display:block;font-size:.85em;color:#333}"
            ".elrscal-help{display:block;font-size:.72em;color:#555;margin:2px 0 8px 0;line-height:1.25em;white-space:normal;overflow-wrap:anywhere}"
            ".elrscal-row{box-sizing:border-box;width:100%;max-width:100%;margin:0 0 8px 0;overflow:hidden}"
            ".elrscal-row label{display:block;font-size:.82em;margin:0 0 2px 0}"
            ".elrscal-row input{box-sizing:border-box;width:100%;max-width:100%;min-width:0}"
            ".elrscal-row button{box-sizing:border-box;width:100%;max-width:100%;min-width:0;margin:4px 0 0 0;font-size:.95em;line-height:2rem}"
            "</style>";

    wmAppendCRSFCALAxis(html, "Aileron", "elrs_roll_live",
                        "right gimbal, left (low) - right (high)",
                        "crrlo", settings.elrsRollLow,
                        "crrct", settings.elrsRollCtr,
                        "crrhi", settings.elrsRollHigh);
    wmAppendCRSFCALAxis(html, "Elevator", "elrs_pitch_live",
                        "right gimbal, down (low) - up (high)",
                        "cptlo", settings.elrsPitchLow,
                        "cptct", settings.elrsPitchCtr,
                        "cpthi", settings.elrsPitchHigh);
    wmAppendCRSFCALAxis(html, "Throttle", "elrs_throttle_live",
                        "left gimbal, down (low) - up (high)",
                        "cthlo", settings.elrsThrLow,
                        "cthct", settings.elrsThrCtr,
                        "cthhi", settings.elrsThrHigh);
    wmAppendCRSFCALAxis(html, "Rudder", "elrs_yaw_live",
                        "left gimbal, left (low) - right (high)",
                        "cywlo", settings.elrsYawLow,
                        "cywct", settings.elrsYawCtr,
                        "cywhi", settings.elrsYawHigh);

    html += "<script>(function(){if(window.__elrsCalInit)return;window.__elrsCalInit=true;"
            "function ge(id){return document.getElementById(id);}function setLive(id,val){var el=ge(id);if(el)el.textContent=val;}"
            "function setStatus(msg){var el=ge('elrscalstat');if(el)el.textContent=msg;}"
            "window.elrsCapture=function(liveId,targetId){var live=ge(liveId),target=ge(targetId);if(!live||!target)return;"
            "if(live.textContent==='--')return;target.value=live.textContent;};"
            "function fail(){setStatus('Live ADC unavailable. Make sure the board is powered and the ADS1015 is reachable.');"
            "setLive('elrs_roll_live','--');setLive('elrs_pitch_live','--');setLive('elrs_throttle_live','--');setLive('elrs_yaw_live','--');}"
            "function poll(){fetch('/elrsraw',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){"
            "if(!d.ok){fail();return;}setLive('elrs_roll_live',d.roll);setLive('elrs_pitch_live',d.pitch);"
            "setLive('elrs_throttle_live',d.throttle);setLive('elrs_yaw_live',d.yaw);"
            "setStatus('Live ADC connected. Capture low, center, and high for each gimbal, then save the page.');"
            "}).catch(fail);}poll();setInterval(poll,500);})();</script></div>";

    if(op == WM_CP_LEN) {
        wmLenBuf = html.length() + 1;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(html.length() + 1);
    if(!str) {
        return NULL;
    }
    strcpy(str, html.c_str());
    return str;
}

static const char *wmBuildCRSFOM(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cOpModeCustHTMLSrc, 4, settings.opMode, false);
}
static const char *wmBuildCRSFPR(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cPktRateCustHTMLSrc, 7, settings.elrsPktRate, false);
}
static const char *wmBuildCRSFSU(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cSpdUnitCustHTMLSrc, 4, settings.elrsSpdUnit, false);
}
static const char *wmBuildCRSFTR(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cTlmRatioCustHTMLSrc, 9, settings.elrsTlmRatio, false);
}
static const char *wmBuildCRSFMP(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cMaxPowerCustHTMLSrc, 8, settings.elrsMaxPower, false);
}
static const char *wmBuildCRSFDP(const char *dest, int op)
{
    return wmBuildSelect(dest, op, cDynPowerCustHTMLSrc, 4, settings.elrsDynPower, false);
}
static const char *wmBuildCRSFRC(const char *dest, int op)
{
    return wmBuildSelectOneBased(dest, op, cRollChCustHTMLSrc, 18, settings.elrsRollCh, false);
}
static const char *wmBuildCRSFPC(const char *dest, int op)
{
    return wmBuildSelectOneBased(dest, op, cPitchChCustHTMLSrc, 18, settings.elrsPitchCh, false);
}
static const char *wmBuildCRSFTC(const char *dest, int op)
{
    return wmBuildSelectOneBased(dest, op, cThrChCustHTMLSrc, 18, settings.elrsThrCh, false);
}
static const char *wmBuildCRSFYC(const char *dest, int op)
{
    return wmBuildSelectOneBased(dest, op, cYawChCustHTMLSrc, 18, settings.elrsYawCh, false);
}

static void handleELRSRawRead()
{
    int16_t axes[ELRS_GIMBAL_AXIS_COUNT];
    char buf[128];

    if(!readELRSCurrentRawAxes(axes)) {
        wm.server->send(503, "application/json", "{\"ok\":false}");
        return;
    }

    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"roll\":%d,\"pitch\":%d,\"throttle\":%d,\"yaw\":%d}",
             axes[ELRS_GIMBAL_INPUT_AILERON],
             axes[ELRS_GIMBAL_INPUT_ELEVATOR],
             axes[ELRS_GIMBAL_INPUT_THROTTLE],
             axes[ELRS_GIMBAL_INPUT_RUDDER]);
    wm.server->send(200, "application/json", buf);
}

static void getServerParamOneBased(const char *name, char *destBuf, size_t length, int minval, int maxval, int defaultVal)
{
    char tempBuf[4];

    getServerParam(name, tempBuf, sizeof(tempBuf) - 1, minval - 1, maxval - 1, defaultVal - 1);
    snprintf(destBuf, length + 1, "%d", atoi(tempBuf) + 1);
}

#endif