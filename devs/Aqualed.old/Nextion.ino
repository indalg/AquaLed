/*
   AQUALED Nextion support functions (c) T. Formanowski 2016-2017
   https://github.com/mathompl/AquaLed
 */

#ifndef NO_NEXTION

#include <Arduino.h>
#include "Nextion.h"



// init nextion lcd
static void nexInit(void)
{
        //delay (500);
        NEXTION_BEGIN (9600);
        NEXTION_PRINT ("\r\n");
        setInt (NX_FIELD_BAUDS,  (long)NEXTION_BAUD_RATE);
        NEXTION_FLUSH ();        
        NEXTION_BEGIN ((long)NEXTION_BAUD_RATE);
        NEXTION_PRINT ("\r\n");
        setInt (NX_FIELD_BKCMD, (long)0);
        setPage (PAGE_HOME);
        //sendNextionEOL ();
        //delay (500);
        toggleButtons ();
        // init names
        for (byte i = 0; i < PWMS; i++)
        {
                setText (NX_FIELD_LD1+i, (char*)pgm_read_word(&(nx_pwm_names[i])));
                delay(10);
                setText (NX_FIELD_BLD1+i, (char*)pgm_read_word(&(nx_pwm_names[i])));
                delay(10);
        }
        forceRefresh = true;
        wdt_reset();
}

// main touch listener
static void nxTouch()
{
        byte __buffer[7];
        memset(__buffer, 0, sizeof (__buffer));
        byte i = 0;
        byte c;
        while (NEXTION_AVAIL ()> 0)
        {
                wdt_reset();
                delay(10);
                c = NEXTION_READ ();
                wdt_reset();
                if (c == NEX_RET_EVENT_TOUCH_HEAD)
                {
                        __buffer[i] = c;
                        i++;
                }
                if (i > 6) break;
        }
        if (0xFF == __buffer[4] && 0xFF == __buffer[5] && 0xFF == __buffer[6])
        {
                handlePage (__buffer[1],  __buffer[2]);
                return;
        }
        wdt_reset();
}

static void handlePage (byte pid, byte cid)
{
        lastTouch = currentMillis;
        switch (pid)
        {
        case PAGE_HOME:
                handleHomePage (cid);
                break;

        case PAGE_CONFIG:
                handleConfigPage (cid);
                break;

        case PAGE_SETTIME:
                handleSetTimePage (cid);
                break;

        case PAGE_SETTINGS:
                handleSettingsPage (cid);
                break;

        case PAGE_PWM:
                handlePWMPage (cid);
                break;

        case PAGE_TEST:
                handleTestPage (cid);
                break;

        case PAGE_SCREENSAVER:
                handleScreenSaver (cid);
                break;

        case PAGE_THERMO:
                handleThermoPage (cid);
                break;

        case PAGE_SCHEDULE:
                handleSchedulePage (cid);
                break;

        case PAGE_PWM_LIST:
                handlePWMListPage (cid);
                break;

        case PAGE_ERROR:
                handleScreenSaver (cid);
                break;

        default:
                break;
        }
        lastTouch = currentMillis;
}

static void handleScreenSaver (byte cid)
{
        forceRefresh = true;
        setPage (PAGE_HOME);
        lastTouch = currentMillis;
}

static boolean readSensor (byte field, byte sensor[])
{
  if (!getNumber(NX_PAGE_THERMO, field, &t)) return false;
  for (byte k = 0; k < 8; k++)
          sensor[k] = sensorsList[t].address[k];
  return true;
}

static void handleThermoPage (byte cid)
{
        boolean status = false;
        switch (cid)
        {
        // save
        case THERMOPAGE_BUTTON_SAVE:
                setPage (PAGE_SAVING);
                if (!readSensor (NX_FIELD_N0, settings.ledSensorAddress)) break;
                if (!readSensor (NX_FIELD_N1, settings.sumpSensorAddress)) break;
                if (!readSensor (NX_FIELD_N2, settings.waterSensorAddress)) break;
                writeEEPROMSensors ();
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                status = true;
                break;

        //cancel
        case THERMOPAGE_BUTTON_CANCEL:
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                status = true;
                break;

        default:
                break;
        }
        if (!status) setPage (PAGE_THERMO);
}

static void handleSchedulePage (byte cid)
{
        switch (cid)
        {
        //cancel
        case SCHEDULEPAGE_BUTTON_CLOSE:
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                break;

        default:
                break;
        }
}

static boolean handleTestSlider (int field, byte i)
{
        if (!getNumber(field, &t)) return false;
        pwmRuntime[i].valueTest = mapRound ((byte)t, 0, 100, 0, PWM_I2C_MAX);
        pwmRuntime[i].testMode = true;
        return true;

}

static void handleTestPage (byte cid)
{
        switch (cid)
        {
        // pwms
        case TESTPAGE_SLIDER_PWM_1:
                handleTestSlider (NX_FIELD_N0, 0);
                break;
        case TESTPAGE_SLIDER_PWM_2:
                handleTestSlider (NX_FIELD_N1, 1);
                break;
        case TESTPAGE_SLIDER_PWM_3:
                handleTestSlider (NX_FIELD_N2, 2);
                break;
        case TESTPAGE_SLIDER_PWM_4:
                handleTestSlider (NX_FIELD_N3, 3);
                break;
        case TESTPAGE_SLIDER_PWM_5:
                handleTestSlider (NX_FIELD_N4, 4);
                break;
        case TESTPAGE_SLIDER_PWM_6:
                handleTestSlider (NX_FIELD_N5, 5);
                break;
        case TESTPAGE_SLIDER_PWM_7:
                handleTestSlider (NX_FIELD_N6, 6);
                break;
        case TESTPAGE_SLIDER_PWM_8:
                handleTestSlider (NX_FIELD_N7, 7);
                break;

        // close
        case TESTPAGE_BUTTON_CLOSE:
                lastTouch = currentMillis;


                for (byte i = 0; i < PWMS; i++)
                {
                        pwmRuntime[i].valueTest = 0;
                        pwmRuntime[i].testMode = false;
                }
                forcePWMRecovery ();
                forceDimmingRestart ();
                setPage (PAGE_CONFIG);
                break;

        default:
                break;
        }
}


static void handlePWMPage (byte cid)
{
        uint16_t tmax, tamb;
        int i;
        byte lastPin;
        byte lastI2C;
        boolean status = false;
        switch (cid)
        {
        // save
        case PWMPAGE_BUTTON_SAVE:
                setPage (PAGE_SAVING);
                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N9, &i)) break;

                if (i < 1 || i > PWMS) break;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C0, &t)) break;
                pwmSettings[i - 1].enabled = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C1, &t)) break;
                pwmSettings[i - 1].isNightLight = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C2, &t)) break;
                pwmSettings[i - 1].isProg = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N0, &t)) break;
                pwmSettings[i - 1].onHour = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N1, &t)) break;
                pwmSettings[i - 1].onMinute = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N2, &t)) break;
                pwmSettings[i - 1].offHour = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N3, &t)) break;
                pwmSettings[i - 1].offMinute = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N4, &t)) break;
                pwmSettings[i - 1].sunriseLenght = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N5, &t)) break;
                pwmSettings[i - 1].sunsetLenght = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N6, &t)) break;
                //  tmin = mapRound ((byte)t, 0, 100, 0, 255);
                //pwmSettings[i - 1].valueNight = mapRound ((byte)t, 0, 255, 0, PWM_I2C_MAX);
                pwmSettings[i - 1].valueNight = (byte)t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N7, &t)) break;
                tmax = mapRound ((byte)t, 0, 100, 0, PWM_I2C_MAX);
                pwmSettings[i - 1].valueDay = tmax;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N8, &t)) return;
                tamb = mapRound ((byte)t, 0, 100, 0, PWM_I2C_MAX);
                pwmSettings[i - 1].valueProg = tamb;

                lastPin = pwmSettings[i - 1].pin;
                lastI2C = pwmSettings[i - 1].isI2C;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N10, &t)) break;
                pwmSettings[i - 1].pin = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_N11, &t)) break;
                pwmSettings[i - 1].watts = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C3, &t)) break;
                pwmSettings[i - 1].isI2C = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C4, &t)) break;
                pwmSettings[i - 1].invertPwm = t;

                if (!getNumber(NX_PAGE_PWM, NX_FIELD_C5, &t)) break;
                pwmSettings[i - 1].useLunarPhase = t;

                if (lastPin != pwmSettings[i - 1].pin || lastI2C != pwmSettings[i - 1].isI2C)
                        initPWM ( i-1 );

                lastTouch = currentMillis;
                writeEEPROMPWMConfig (i - 1);
                updateChannelTimes (i-1);
                setPage (PAGE_PWM_LIST);
                status = true;
                break;

        //  cancel
        case PWMPAGE_BUTTON_CANCEL:
                lastTouch = currentMillis;
                setPage (PAGE_PWM_LIST);
                status = true;
                break;

        default:
                status = true;
                break;
        }
        if (!status) setPage (PAGE_PWM_LIST);
}

static void handleSettingsPage (byte cid)
{
        boolean status = false;
        switch (cid)
        {
        // save
        case SETTINGSPAGE_BUTTON_SAVE:
                setPage (PAGE_SAVING);
                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_N0, &t)) break;
                settings.max_led_temp = t;

                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_N1, &t)) break;
                settings.max_sump_temp = t;

                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_N2, &t)) break;
                settings.max_water_temp = t;

                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_N3, &t)) break;
                settings.pwmDimmingTime = t;

                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_N4, &t)) break;
                settings.screenSaverTime = t;

                if (!getNumber(NX_PAGE_SETTINGS,NX_FIELD_C0, &t)) break;
                settings.softDimming  = t;

                writeEEPROMSettings ();
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                status = true;
                break;

        // cancel
        case SETTINGSPAGE_BUTTON_CANCEL:
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                status = true;
                break;

        default:
                break;
        }
        if (!status) setPage (PAGE_SETTINGS);
}

static void handleSetTimePage (byte cid)
{

        int setHour, setMinute, setYear, setMonth, setDay;
        boolean status = false;
        switch (cid)
        {
        // save
        case TIMEPAGE_BUTTON_SAVE:
                setPage (PAGE_SAVING);
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_N0, &setHour)) break;
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_N1, &setMinute)) break;
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_N2, &setDay)) break;
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_N3, &setMonth)) break;
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_N4, &setYear)) break;

                setTime( setHour, setMinute, 0, setDay, setMonth, setYear );
                RTC.set( now( ) );
                if (!getNumber(NX_PAGE_TIME, NX_FIELD_C0, &t)) break;
                settings.dst =t;
                readTime ();
                adjustDST ();
                writeEEPROMSettings ();
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                lastTouch = currentMillis;
                status = true;
                break;

        // cancel
        case TIMEPAGE_BUTTON_CANCEL:
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                lastTouch = currentMillis;
                status = true;
                break;

        default:
                status = true;
                break;
        }
        if (!status) setPage (PAGE_SETTIME);
}


static void drawSchedule ()
{
        for (byte i = 0; i < PWMS; i++)
        {
                int min_start = (int)pwmSettings[i].onHour * (int)60 + (int)pwmSettings[i].onMinute;
                int min_stop = (int)pwmSettings[i].offHour * (int)60 + (int)pwmSettings[i].offMinute;

                // background
                if (pwmSettings[i].isNightLight == 1)
                        fillRect (offset * i + startx, starty, width, height, COLOR_DARKBLUE);
                else
                        fillRect (offset * i + startx, starty, width, height, COLOR_RED);

                // off
                if (pwmSettings[i].enabled == 0) continue;

                // light
                boolean midnight = false;
                if (min_stop < min_start) midnight = true;

                if (!midnight)
                {
                        int startL = map (min_start, 0, min_hour, starty, starty + height);
                        int stopL = map (min_stop, 0, min_hour, starty, starty + height ) - startL;
                        fillRect (offset * i + startx, startL, width, stopL, COLOR_GREEN);
                }
                else
                {
                        int startL = map (min_start, 0, min_hour, starty, starty + height);
                        int stopL = map (min_stop, 0, min_hour, starty, starty + height ) - 30;
                        int stopM =  map (min_hour, 0, min_hour, starty, starty + height ) - startL;
                        // start to midnight
                        fillRect (offset * i + startx, startL, width, stopM, COLOR_GREEN);
                        // midnight to end
                        fillRect (offset * i + startx, starty, width, stopL, COLOR_GREEN);
                }
                // sunrise
                int min_sunrise  = min_start + (int)pwmSettings[i].sunriseLenght;
                if (min_sunrise > min_hour) midnight = true; else midnight = false;

                if (!midnight)
                {
                        int startL = map (min_start, 0, min_hour, starty, starty + height);
                        int stopL = map (min_sunrise, 0, min_hour, starty, starty + height ) - startL;
                        fillRect (offset * i + startx, startL, width, stopL, COLOR_BLUE);
                }
                else
                {
                        int startL = map (min_start, 0, min_hour, starty, starty + height);
                        int stopM =  map (min_hour, 0, min_hour, starty, starty + height ) - startL;
                        int min_sunrise_left = min_sunrise - min_hour;
                        int stopL = map (min_sunrise_left, 0, min_hour, starty, starty + height ) - starty;
                        // start to midnight
                        fillRect (offset * i + startx, startL, width, stopM, COLOR_BLUE);
                        // midnight to end
                        fillRect (offset * i + startx, starty, width, stopL, COLOR_BLUE);
                }
                // sunset
                int min_sunset  = min_stop - (int)pwmSettings[i].sunsetLenght;
                if (min_sunset < 0) midnight = true; else midnight = false;
                if (!midnight)
                {
                        int startL = map (min_sunset, 0, min_hour, starty, starty + height);
                        int stopL = map (min_stop, 0, min_hour, starty, starty + height ) - startL;
                        fillRect (offset * i + startx, startL, width, stopL, COLOR_BLUE);
                }
                else
                {
                        int min_sunset_left = min_hour + min_sunset;
                        int stopL = map (min_stop, 0, min_hour, starty, starty + height ) - starty;
                        int startL = map (min_sunset_left, 0, min_hour, starty, starty + height);
                        int stopM =  map (min_hour, 0, min_hour, starty, starty + height ) - startL;
                        // zero to end
                        fillRect (offset * i + startx, starty, width, stopL, COLOR_BLUE);
                        // rest till midnight
                        fillRect (offset * i + startx, startL, width, stopM, COLOR_BLUE);
                }
        }
        int min_now = map (hour () * 60 + minute (), 0, min_hour, starty, starty + height);
        fillRect (hour_startx, min_now, hour_stopx, 1, COLOR_YELLOW);
        // siatka dodatkowa
        /*
           for (int i = 0 ; i < 24; i++)
           {
           fillRect (40, 10 * i + starty+1, 180, 1, COLOR_DARKGRAY);

           }*/
}

static void handleConfigPage (byte cid)
{
        byte s;
        byte idxLed, idxSump, idxWater;

        char tempbuff[150] = {0};
        switch (cid)
        {
        // schedule
        case CONFIGPAGE_BUTTON_SCHEDULE:
                setPage (PAGE_SCHEDULE);
                drawSchedule ();
                break;

        // close
        case CONFIGPAGE_BUTTON_CLOSE:
                forceRefresh = true;
                setPage (PAGE_HOME);
                toggleButtons();
                break;

        // thermo setup
        case CONFIGPAGE_BUTTON_THERMO:
              #ifndef NO_TEMPERATURE
                setPage (PAGE_THERMO);
                s = discoverOneWireDevices ();
                memset (tempbuff, 0, sizeof (tempbuff));
                setValue (NX_FIELD_VA0, (s-1));
                for (byte i = 0; i < s; i++)
                {
                        sprintf(tempbuff + strlen (tempbuff), "%d. ", i);
                        if (i == 0)
                        {
                                strcpy (tempbuff + strlen (tempbuff), "BRAK ");
                        }
                        else
                        {
                                if (sensorsList[i].detected)
                                        strcpy (tempbuff + strlen (tempbuff), "* ");
                                for (byte k = 0; k < 8; k++)
                                        sprintf (tempbuff + strlen (tempbuff), "%02x ", sensorsList[i].address[k]);
                        }
                        strcpy  (tempbuff + strlen (tempbuff), "\r\n");
                }
                idxLed = listContains (settings.ledSensorAddress);
                idxSump = listContains (settings.sumpSensorAddress);
                idxWater = listContains (settings.waterSensorAddress);
                if (idxLed == 255) idxLed = 0;
                if (idxSump == 255) idxSump = 0;
                if (idxWater == 255) idxWater = 0;
                setValue (NX_FIELD_N0, idxLed);
                setValue (NX_FIELD_N1, idxSump);
                setValue (NX_FIELD_N2, idxWater);
                setText (NX_FIELD_T4, (String) tempbuff);
              #endif
                break;

        // test
        case CONFIGPAGE_BUTTON_TEST:
                setPage (PAGE_TEST);
                for (byte i = 0; i < PWMS; i++)
                {
                        setValue (NX_FIELD_N0+i, mapRound (pwmRuntime[i].valueCurrent, 0, PWM_I2C_MAX, 0, 100));
                        setValue (NX_FIELD_C1+i, mapRound (pwmRuntime[i].valueCurrent, 0, PWM_I2C_MAX, 0, 100));
                        pwmRuntime[i].valueTest = pwmRuntime[i].valueCurrent;
                }

                break;

        // hour and date setup
        case CONFIGPAGE_BUTTON_TIME:
                setPage (PAGE_SETTIME);
                setValue (NX_FIELD_N0, hour ());
                setValue (NX_FIELD_N1, minute ());
                setValue (NX_FIELD_N2, day ());
                setValue (NX_FIELD_N3, month ());
                setValue (NX_FIELD_N4, year());
                setValue (NX_FIELD_C0, settings.dst);
                break;

        // other settings
        case CONFIGPAGE_BUTTON_SETTINGS:
                setPage (PAGE_SETTINGS);
                setValue (NX_FIELD_N0, settings.max_led_temp);
                setValue (NX_FIELD_N1, settings.max_sump_temp);
                setValue (NX_FIELD_N2, settings.max_water_temp);
                setValue (NX_FIELD_N3, settings.pwmDimmingTime);
                setValue (NX_FIELD_N4, settings.screenSaverTime);
                setValue (NX_FIELD_C0, settings.softDimming);
                break;

        // pwm config
        case CONFIGPAGE_BUTTON_PWM:
                setPage (PAGE_PWM_LIST);
                break;

        default:
                break;
        }
}

static void handlePWMListPage (byte cid)
{
        byte tmin, tmax, tamb;

        switch (cid)
        {
        // enter pwm settings
        case PWMCONFIGPAGE_BUTTON_PWM_1:
        case PWMCONFIGPAGE_BUTTON_PWM_2:
        case PWMCONFIGPAGE_BUTTON_PWM_3:
        case PWMCONFIGPAGE_BUTTON_PWM_4:
        case PWMCONFIGPAGE_BUTTON_PWM_5:
        case PWMCONFIGPAGE_BUTTON_PWM_6:
        case PWMCONFIGPAGE_BUTTON_PWM_7:
        case PWMCONFIGPAGE_BUTTON_PWM_8:
                tmin = pwmSettings[cid - 1].valueNight;
                tmax = (byte) mapRound (pwmSettings[cid - 1].valueDay, 0, PWM_I2C_MAX, 0, 100);
                tamb = (byte) mapRound (pwmSettings[cid - 1].valueProg, 0, PWM_I2C_MAX, 0, 100);
                setPage (PAGE_PWM);

                setValue (NX_FIELD_C0, pwmSettings[cid - 1].enabled);
                setValue (NX_FIELD_C1, pwmSettings[cid - 1].isNightLight);
                setValue (NX_FIELD_C2, pwmSettings[cid - 1].isProg);
                setValue (NX_FIELD_N0, pwmSettings[cid - 1].onHour);
                setValue (NX_FIELD_N1, pwmSettings[cid - 1].onMinute);
                setValue (NX_FIELD_N2, pwmSettings[cid - 1].offHour);
                setValue (NX_FIELD_N3, pwmSettings[cid - 1].offMinute);
                setValue (NX_FIELD_N4, pwmSettings[cid - 1].sunriseLenght);
                setValue (NX_FIELD_N5, pwmSettings[cid - 1].sunsetLenght);
                setValue (NX_FIELD_N6, tmin);
                setValue (NX_FIELD_N7, tmax);
                setValue (NX_FIELD_N8, tamb);
                setValue (NX_FIELD_N9, cid);
                setValue (NX_FIELD_N10, pwmSettings[cid - 1].pin);
                setValue (NX_FIELD_N11, pwmSettings[cid - 1].watts);
                setValue (NX_FIELD_C3, pwmSettings[cid - 1].isI2C);
                setValue (NX_FIELD_C4, pwmSettings[cid - 1].invertPwm);
                setValue (NX_FIELD_C5, pwmSettings[cid - 1].useLunarPhase);
                setText (NX_FIELD_T4,  (char*)pgm_read_word(&(nx_pwm_names[cid -1])));
                break;

        // cancel
        case PWMCONFIGPAGE_BUTTON_CLOSE:
                lastTouch = currentMillis;
                setPage (PAGE_CONFIG);
                lastTouch = currentMillis;
                break;

        default:
                break;
        }
}

static void handleHomePage (byte cid)
{
        switch (cid)
        {

        // config show
        case HOMEPAGE_BUTTON_CONFIG:
                setPage (PAGE_CONFIG);
                break;

        //toggle night mode
        case HOMEPAGE_BUTTON_NIGHT:
                if (settings.forceAmbient  == 1 || settings.forceOFF  == 1) return;
                if (settings.forceNight == 1)
                {
                        settings.forceNight = 0;
                        forcePWMRecovery ();
                }
                else
                {
                        settings.forceNight = 1;
                        forceDimmingRestart ();
                }

                toggleButtons();
                writeEEPROMForceNight ();
                break;

        // ambient toggle
        case HOMEPAGE_BUTTON_AMBIENT:
                if (settings.forceOFF == 1 || settings.forceNight == 1) return;
                if (settings.forceAmbient == 1)
                {
                        settings.forceAmbient = 0;
                        forcePWMRecovery ();
                }
                else
                {
                        settings.forceAmbient = 1;
                        forceDimmingRestart ();
                }

                toggleButtons();
                writeEEPROMForceAmbient ();
                break;

        // off/on toggle
        case HOMEPAGE_BUTTON_OFF:
                if (settings.forceOFF == 1)
                {
                        settings.forceOFF = 0;
                        forcePWMRecovery ();
                }
                else
                {
                        settings.forceOFF = 1;
                        forceDimmingRestart ();
                }
                toggleButtons();
                writeEEPROMForceOff ();
                break;

        default:
                break;
        }
}

static void toggleButton (byte value, byte field, byte pic_on, byte pic_off)
{
        if (value == 1) setPic(field,pic_on); else setPic(field, pic_off);
}

// toggle home page buttons images
static void toggleButtons()
{
        toggleButton (settings.forceOFF, NX_FIELD_BO, NX_PIC_BO_ON, NX_PIC_BO_OFF);
        toggleButton (settings.forceAmbient, NX_FIELD_BA, NX_PIC_BA_ON, NX_PIC_BA_OFF);
        toggleButton (settings.forceNight, NX_FIELD_BN, NX_PIC_BN_ON, NX_PIC_BN_OFF);
}

#ifndef NO_TEMPERATURE
void updateTempField (byte field, byte sensor, byte max, byte min)
{
        if (temperaturesFans[sensor].nxTemperature != temperaturesFans[sensor].temperature  || forceRefresh)
        {
                if (temperaturesFans[sensor].temperature != TEMP_ERROR)
                {
                        setTemperature(field, temperaturesFans[sensor].temperature);
                        temperaturesFans[sensor].nxTemperature = temperaturesFans[sensor].temperature;
                        if (temperaturesFans[sensor].temperature < min || temperaturesFans[sensor].temperature > max)
                                setColor (field, COLOR_LIGHTRED);
                        else
                                setColor (field, COLOR_LIGHTGREEN);
                }
                else
                        setText(field, NX_STR_DASH);
        }
}

void updateFanField (byte field, byte sensor)
{
        if (!temperaturesFans[sensor].fanStatus) setText (field, NX_STR_EMPTY);
        else setText (field, NX_STR_FAN);
        temperaturesFans[sensor].nxFanStatus = temperaturesFans[sensor].fanStatus;
}
#endif
static void updateWaterTemp() {
    #ifndef NO_TEMPERATURE
        updateTempField (NX_FIELD_WT, WATER_TEMPERATURE_FAN,settings.max_water_temp, WATER_TEMPERATURE_MIN);
    #endif
}

static void updateHomePage() {
#ifndef NO_TEMPERATURE
        updateWaterTemp();
        updateTempField (NX_FIELD_LT, LED_TEMPERATURE_FAN,settings.max_led_temp, 0);
        updateTempField (NX_FIELD_ST, SUMP_TEMPERATURE_FAN,settings.max_sump_temp, 0);

#endif

        for (byte i = 0; i < PWMS; i++)
        {
                byte valueField = 66+i;
                byte iconField =  3+i;
                if (!pwmRuntime[i].valueCurrent && pwmSettings[i].enabled == 0 && (pwmRuntime[i].nxPwmLast != 0 || forceRefresh))
                {
                        setText (valueField, NX_STR_DASH);
                        setText (iconField, NX_STR_SPACE);
                        pwmRuntime[i].nxPwmLast = 0;
                        continue;
                }

                if (pwmRuntime[i].nxPwmLast != pwmRuntime[i].valueCurrent || forceRefresh)
                {
                        uint16_t color = COLOR_WHITE;
                        double percent =  mapDouble((double)pwmRuntime[i].valueCurrent, 0.0, (double)PWM_I2C_MAX, 0.0, 100.0);
                        byte icon = NX_STR_SPACE;
                        if (pwmRuntime[i].isSunrise )
                        {
                                icon = NX_STR_SUNRISE;
                                color = COLOR_LIGHTYELLOW;
                        }
                        else if (pwmRuntime[i].isSunset )
                        {
                                icon = NX_STR_SUNSET;
                                color = COLOR_ORANGE;
                        }
                        else if (pwmRuntime[i].recoverLastState)
                        {
                                icon = NX_STR_RECOVER;
                                color = COLOR_LIGHTGREEN;
                        }
                        else if (pwmRuntime[i].valueCurrent < pwmRuntime[i].valueGoal)
                        {
                                icon = NX_STR_UP;

                        }
                        else if (pwmRuntime[i].valueCurrent > pwmRuntime[i].valueGoal) icon = NX_STR_DOWN;
                        else if (pwmRuntime[i].valueCurrent == 0 || pwmSettings[i].enabled == 0)
                        {
                                icon = NX_STR_OFF;
                                color = COLOR_LIGHTRED;
                        }
                        else if (pwmRuntime[i].isNight)
                        {
                                if (pwmSettings[i].useLunarPhase)
                                {
                                        color = rgb565 (map (moonPhases[moonPhase], 0,100,150,255));
                                }
                                icon = NX_STR_NIGHT;
                        }
                        else if (pwmRuntime[i].valueCurrent == pwmSettings[i].valueDay)
                        {
                                icon = NX_STR_ON;
                                color = COLOR_YELLOW;
                        }
                        pwmRuntime[i].nxPwmLast = pwmRuntime[i].valueCurrent;
                        setPercent (valueField,  percent);
                        setText (iconField, icon);
                        setColor (iconField, color);
                }
        }
        #ifndef NO_TEMPERATURE
        updateFanField (NX_FIELD_T0, WATER_TEMPERATURE_FAN);
        updateFanField (NX_FIELD_T1, LED_TEMPERATURE_FAN);
        updateFanField (NX_FIELD_T2, SUMP_TEMPERATURE_FAN);
        #endif
}

uint16_t rgb565( byte rgb)
{
        uint16_t ret  = (rgb & 0xF8) << 8;// 5 bits
        ret |= (rgb & 0xFC) << 3;       // 6 bits
        ret |= (rgb & 0xF8) >> 3;       // 5 bits
        return( ret);
}

static void displayWats ()
{
        watts = 0;
        for (byte i = 0; i < PWMS; i++)
        {
                if (pwmSettings[i].enabled && pwmSettings[i].watts && pwmRuntime[i].valueCurrent)
                {
                        watts+=(float)(((float)pwmRuntime[i].valueCurrent / (float)PWM_I2C_MAX) * (float)pwmSettings[i].watts);
                }
        }
        if (watts > MAX_WATTS) max_watts_exceeded = true;
        else max_watts_exceeded = false;

        if(watts!=lastWatts) setWatts (NX_FIELD_WA, watts);
}

static void timeDisplay() {

        char buff[7] = {0};
        memset(buff, 0, sizeof (buff));
        if (time_separator % 2 == 0) sprintf(buff + strlen(buff), "%02u:%02u", hour (), minute ());
        else sprintf(buff + strlen(buff), "%02u %02u", hour (), minute ());
        setText (NX_FIELD_H, (String)buff);
        time_separator++;
}

static void nxDisplay ()
{
        if (currentMillis - previousNxInfo > NX_INFO_RESOLUTION)
        {
                previousNxInfo = currentMillis;

                if (lampOverheating)
                {
                        nxScreen = PAGE_ERROR;
                        setPage (PAGE_ERROR);
                        setText (NX_FIELD_T1, (char*)pgm_read_word(&(nx_errors[0])) );
                }

                if (max_watts_exceeded)
                {
                        nxScreen = PAGE_ERROR;
                        setPage (PAGE_ERROR);
                        setText (NX_FIELD_T1, (char*)pgm_read_word(&(nx_errors[1])) );
                }


                if (nxScreen == PAGE_SCREENSAVER )
                {
                        timeDisplay();
                        updateWaterTemp();
                        displayWats ();
                        forceRefresh = false;
                }

                if (nxScreen == PAGE_HOME )
                {
                        timeDisplay();
                        updateHomePage();
                        displayWats ();
                        forceRefresh = false;
                }
                // screensaver
                if (currentMillis - lastTouch > settings.screenSaverTime*1000 && nxScreen != PAGE_SCREENSAVER && settings.screenSaverTime > 0
                    && nxScreen != PAGE_TEST
                    && nxScreen != PAGE_SETTINGS
                    && nxScreen != PAGE_PWM
                    && nxScreen != PAGE_SETTIME
                    && nxScreen != PAGE_THERMO
                    && nxScreen != PAGE_SCHEDULE
                    && nxScreen != PAGE_PWM_LIST
                    && nxScreen != PAGE_ERROR
                    && nxScreen != PAGE_SAVING
                    )
                {
                        setPage (PAGE_SCREENSAVER);
                        setText (NX_FIELD_T1,NX_STR_EMPTY);
                        forceRefresh = true;
                }
        }
        if (lastTouch == 0) lastTouch = currentMillis;
}

/* NEXTION COMMUNICATION */

static void sendNextionEOL ()
{
        NEXTION_WRITE(0xFF);
        NEXTION_WRITE(0xFF);
        NEXTION_WRITE(0xFF);
        wdt_reset ();
}

static void setColor (byte field, unsigned int color)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PCO])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_EQ])));
        NEXTION_PRINT (color);
        sendNextionEOL ();
}

static void setTemperature (byte field, float temp)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        NEXTION_PRINTF (temp, 1);
        printPGM( (char*)pgm_read_word(&(nx_strings[NX_STR_DEGREE])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setText (byte field, String text)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        NEXTION_PRINT(text);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setText (byte field, const char * txt)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        printPGM (txt);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setText (byte field, int nx_string_id)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        printPGM( (char*)pgm_read_word(&(nx_strings[nx_string_id])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setValue (byte field, unsigned int val)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_VAL])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_EQ])));
        NEXTION_PRINT(val);
        sendNextionEOL ();
}

static void setInt (byte field, long val)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_EQ])));
        NEXTION_PRINT(val);
        sendNextionEOL ();
}

static void setPic (byte field, byte val)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PIC])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_EQ])));
        NEXTION_PRINT(val);
        sendNextionEOL ();
}

static void setPercent (byte field, double percent)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        NEXTION_PRINTF( percent, 1);
        printPGM( (char*)pgm_read_word(&(nx_strings[NX_STR_PERCENT])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setWatts (byte field, float watts)
{
        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        _writeTxt ();
        NEXTION_PRINTF( watts, 1 );
        printPGM( (char*)pgm_read_word(&(nx_strings[NX_STR_WATTS])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));
        sendNextionEOL ();
}

static void setPage (byte number)
{
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PAGE])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_SPACE])));
        NEXTION_PRINT(number);
        sendNextionEOL ();
        nxScreen = number;
}

static void _writeTxt ()
{
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_TXT])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_EQ])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_PARENTH])));

}

static bool getNumber (byte field, int *result)
{
        return getNumber (255,  field, result);
}

static bool getNumber (byte page, byte field, int *result)
{
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_GET])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_SPACE])));
        if (page!=255)
        {
                printPGM( (char*)pgm_read_word(&(nx_pages[page])));
                printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_DOT])));
        }

        printPGM( (char*)pgm_read_word(&(nx_fields[field])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_VAL])));
        sendNextionEOL ();
        uint8_t temp[8] = {0};
        uint32_t r;
        NEXTION_SETTIMEOUT(500);
        if (sizeof(temp) != NEXTION_READBYTES((char *)temp, sizeof(temp)))
        {
                return false;
        }
        if (temp[0] == NEX_RET_NUMBER_HEAD && temp[5] == 0xFF && temp[6] == 0xFF && temp[7] == 0xFF)
        {
                r = ((uint32_t)temp[4] << 24) | ((uint32_t)temp[3] << 16) | ((uint32_t)temp[2] << 8) | ((uint32_t)temp[1]);
                *result = (int) r;
                return true;
        }
        return false;
}

// fills nextion rectangle
static void fillRect (int x, int y, int w, int h, int color)
{
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_FILL])));
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_SPACE])));
        NEXTION_PRINT (x);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_COMMA])));
        NEXTION_PRINT (y);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_COMMA])));
        NEXTION_PRINT (w);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_COMMA])));
        NEXTION_PRINT (h);
        printPGM( (char*)pgm_read_word(&(nx_commands[NX_CMD_COMMA])));
        NEXTION_PRINT (color);
        sendNextionEOL ();
}

static void printPGM (const char * str)
{
        char c;
        if (!str) return;
        while ((c = pgm_read_byte(str++)))
                NEXTION_PRINT (c);
}

#endif
