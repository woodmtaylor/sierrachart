/*
 * Strike Zone Hotkey System for Sierra Chart
 * 
 * This study creates temporary Strike Zone drawings based on specific price levels.
 * Different hotkeys trigger different Strike Zone configurations.
 * Strike Zones can be cleared at any time.
 */
#include "sierrachart.h"

SCDLLName("Strike Zone Hotkey System")

// Struct to store Strike Zone configuration info
struct StrikeZoneConfig {
    int TicksFromLevel;
    int HeightInTicks;
    COLORREF Color;
    int LineWidth;
    int LineStyle;
};

// Forward declarations
void CreateStrikeZoneAroundLevel(SCStudyInterfaceRef& sc, float Level, 
                              int BeginIndex, int EndIndex, 
                              StrikeZoneConfig& Config, int& ZoneCount);

/****************************************************************************/
// Main study function
SCSFExport scsf_StrikeZoneHotkeys(SCStudyInterfaceRef sc)
{
    // Declare Inputs
    SCInputRef TightZoneTicks = sc.Input[0];
    SCInputRef StandardZoneTicks = sc.Input[1];
    SCInputRef WideZoneTicks = sc.Input[2];
    
    SCInputRef TightZoneHeight = sc.Input[3];
    SCInputRef StandardZoneHeight = sc.Input[4];
    SCInputRef WideZoneHeight = sc.Input[5];
    
    SCInputRef TightZoneColor = sc.Input[6];
    SCInputRef StandardZoneColor = sc.Input[7];
    SCInputRef WideZoneColor = sc.Input[8];
    
    SCInputRef EnableHotkeys = sc.Input[9];
    SCInputRef ClearOnSessionEnd = sc.Input[10];
    
    SCInputRef HotKeyTightZone = sc.Input[11];
    SCInputRef HotKeyStandardZone = sc.Input[12];
    SCInputRef HotKeyWideZone = sc.Input[13];
    SCInputRef HotKeyClearZones = sc.Input[14];
    SCInputRef KeyRepeatDelay = sc.Input[15];
    
    // For tracking whether we've created Strike Zones for current session
    int& s_PreviousTradingDate = sc.GetPersistentInt(1);
    
    // Get the count of zones we've created for tracking
    int& ZoneCount = sc.GetPersistentInt(3);
    
    // State tracking for zone creation
    int& hasZones = sc.GetPersistentInt(4);
    
    // Previous key states for detecting key presses
    int& prevButton1State = sc.GetPersistentInt(5);
    int& prevButton2State = sc.GetPersistentInt(6);
    int& prevButton3State = sc.GetPersistentInt(7);
    int& prevButton4State = sc.GetPersistentInt(8);
    
    // Key state tracking for more reliable detection
    int& lastKeyProcessedTime1 = sc.GetPersistentInt(9);
    int& lastKeyProcessedTime2 = sc.GetPersistentInt(10);
    int& lastKeyProcessedTime3 = sc.GetPersistentInt(11);
    int& lastKeyProcessedTime4 = sc.GetPersistentInt(12);
    
    // Set Configuration and Defaults
    if (sc.SetDefaults)
    {
        sc.GraphName = "Strike Zone Hotkey System";
        sc.StudyDescription = "Creates Strike Zones around Horizontal Rays and Extended Rectangles using hotkeys";
        
        // General settings
        sc.AutoLoop = 0;  // Disable auto looping
        
        // Update frequently to catch keypresses
        sc.UpdateAlways = 1;
        
        // Strike Zone size configurations
        TightZoneTicks.Name = "Tight Zone: Distance from Level (Ticks)";
        TightZoneTicks.SetInt(5);
        TightZoneTicks.SetIntLimits(1, 100);
        
        StandardZoneTicks.Name = "Standard Zone: Distance from Level (Ticks)";
        StandardZoneTicks.SetInt(10);
        StandardZoneTicks.SetIntLimits(1, 100);
        
        WideZoneTicks.Name = "Wide Zone: Distance from Level (Ticks)";
        WideZoneTicks.SetInt(15);
        WideZoneTicks.SetIntLimits(1, 100);
        
        // Strike Zone height configurations
        TightZoneHeight.Name = "Tight Zone: Height (Ticks)";
        TightZoneHeight.SetInt(10);
        TightZoneHeight.SetIntLimits(1, 100);
        
        StandardZoneHeight.Name = "Standard Zone: Height (Ticks)";
        StandardZoneHeight.SetInt(20);
        StandardZoneHeight.SetIntLimits(1, 100);
        
        WideZoneHeight.Name = "Wide Zone: Height (Ticks)";
        WideZoneHeight.SetInt(30);
        WideZoneHeight.SetIntLimits(1, 100);
        
        // Strike Zone color configurations
        TightZoneColor.Name = "Tight Zone: Color";
        TightZoneColor.SetColor(RGB(0, 255, 255));  // Cyan
        
        StandardZoneColor.Name = "Standard Zone: Color";
        StandardZoneColor.SetColor(RGB(255, 255, 0));  // Yellow
        
        WideZoneColor.Name = "Wide Zone: Color";
        WideZoneColor.SetColor(RGB(255, 128, 0));  // Orange
        
        // General options
        EnableHotkeys.Name = "Enable Hotkeys";
        EnableHotkeys.SetYesNo(1);
        
        ClearOnSessionEnd.Name = "Clear Strike Zones on Session End";
        ClearOnSessionEnd.SetYesNo(1);
        
        // Hotkey configurations
        HotKeyTightZone.Name = "Hotkey: Tight Strike Zone";
        HotKeyTightZone.SetInt(49);  // '1' key
        
        HotKeyStandardZone.Name = "Hotkey: Standard Strike Zone";
        HotKeyStandardZone.SetInt(50);  // '2' key
        
        HotKeyWideZone.Name = "Hotkey: Wide Strike Zone";
        HotKeyWideZone.SetInt(51);  // '3' key
        
        HotKeyClearZones.Name = "Hotkey: Clear All Strike Zones";
        HotKeyClearZones.SetInt(52);  // '4' key
        
        // Add key repeat delay setting
        KeyRepeatDelay.Name = "Key Detection Sensitivity (ms)";
        KeyRepeatDelay.SetInt(200); // 200ms default
        KeyRepeatDelay.SetIntLimits(10, 1000);
        
        // Initialize persistent variables
        s_PreviousTradingDate = 0;
        ZoneCount = 0;
        hasZones = 0;
        prevButton1State = 0;
        prevButton2State = 0;
        prevButton3State = 0;
        prevButton4State = 0;
        lastKeyProcessedTime1 = 0;
        lastKeyProcessedTime2 = 0;
        lastKeyProcessedTime3 = 0;
        lastKeyProcessedTime4 = 0;
        
        return;
    }
    
    // Study was reset or is being removed
    if (sc.LastCallToFunction)
    {
        // Clear any strike zone drawings when the study is removed
        if (hasZones)
        {
            sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, 0);
            ZoneCount = 0;
            hasZones = 0;
        }
        
        return;
    }
    
    // Check if we need to clear Strike Zones at session end
    if (ClearOnSessionEnd.GetYesNo())
    {
        int CurrentTradingDate = sc.GetTradingDayDate(sc.BaseDateTimeIn[sc.ArraySize - 1]);
        
        // If we've moved to a new trading day and we have Strike Zones
        if (s_PreviousTradingDate != 0 && 
            CurrentTradingDate != s_PreviousTradingDate && 
            hasZones)
        {
            // Clear all Strike Zone drawings
            sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, 0);
            ZoneCount = 0;
            hasZones = 0;
            
            sc.AddMessageToLog("Cleared strike zones at session end", 1);
        }
        
        // Update the trading date
        s_PreviousTradingDate = CurrentTradingDate;
    }
    
    // Only process hotkeys if enabled
    if (EnableHotkeys.GetYesNo())
    {
        // Get current time in milliseconds for key repeat protection
        int currentTimeMS = (int)(sc.CurrentSystemDateTime.GetTimeInSeconds() * 1000);
        int keyRepeatDelayMS = KeyRepeatDelay.GetInt();
        
        // Get key states
        int button1State = GetAsyncKeyState(HotKeyTightZone.GetInt());
        int button2State = GetAsyncKeyState(HotKeyStandardZone.GetInt());
        int button3State = GetAsyncKeyState(HotKeyWideZone.GetInt());
        int button4State = GetAsyncKeyState(HotKeyClearZones.GetInt());
        
        // Create flags that check if a key is currently down
        bool button1Down = (button1State & 0x8000) != 0;
        bool button2Down = (button2State & 0x8000) != 0;
        bool button3Down = (button3State & 0x8000) != 0;
        bool button4Down = (button4State & 0x8000) != 0;
        
        // Process clear zones (key 4)
        if (button4Down && 
            (currentTimeMS - lastKeyProcessedTime4 > keyRepeatDelayMS) && 
            hasZones)
        {
            sc.AddMessageToLog("Clear button (4) pressed - clearing strike zones", 1);
            
            // Use TOOL_DELETE_ALL to clear all study-created drawings at once
            sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, 0);
            
            // Reset states
            ZoneCount = 0;
            hasZones = 0;
            
            // Update the last time we processed this key
            lastKeyProcessedTime4 = currentTimeMS;
        }
        
        // Process zone creation hotkeys
        StrikeZoneConfig Config;
        bool CreateStrikeZone = false;
        
        // Check button 1 (Tight Zones)
        if (button1Down && (currentTimeMS - lastKeyProcessedTime1 > keyRepeatDelayMS))
        {
            Config.TicksFromLevel = TightZoneTicks.GetInt();
            Config.HeightInTicks = TightZoneHeight.GetInt();
            Config.Color = TightZoneColor.GetColor();
            Config.LineWidth = 2;
            Config.LineStyle = DRAWSTYLE_DASH;
            CreateStrikeZone = true;
            
            sc.AddMessageToLog("Button 1 pressed - creating tight zones", 0);
            lastKeyProcessedTime1 = currentTimeMS;
        }
        // Check button 2 (Standard Zones)
        else if (button2Down && (currentTimeMS - lastKeyProcessedTime2 > keyRepeatDelayMS))
        {
            Config.TicksFromLevel = StandardZoneTicks.GetInt();
            Config.HeightInTicks = StandardZoneHeight.GetInt();
            Config.Color = StandardZoneColor.GetColor();
            Config.LineWidth = 2;
            Config.LineStyle = DRAWSTYLE_DASH;
            CreateStrikeZone = true;
            
            sc.AddMessageToLog("Button 2 pressed - creating standard zones", 0);
            lastKeyProcessedTime2 = currentTimeMS;
        }
        // Check button 3 (Wide Zones)
        else if (button3Down && (currentTimeMS - lastKeyProcessedTime3 > keyRepeatDelayMS))
        {
            Config.TicksFromLevel = WideZoneTicks.GetInt();
            Config.HeightInTicks = WideZoneHeight.GetInt();
            Config.Color = WideZoneColor.GetColor();
            Config.LineWidth = 2;
            Config.LineStyle = DRAWSTYLE_DASH;
            CreateStrikeZone = true;
            
            sc.AddMessageToLog("Button 3 pressed - creating wide zones", 0);
            lastKeyProcessedTime3 = currentTimeMS;
        }
        
        // If we need to create strike zones
        if (CreateStrikeZone)
        {
            // First, clear any existing zones
            if (hasZones)
            {
                sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, 0);
                ZoneCount = 0;
            }
            
            // These are the exact price levels for Horizontal Rays in the chart
            // ONLY these will be used to create zones
            float HorizontalRayLevels[] = {
                5440.25f,
                5417.50f,
                5383.75f,
                5353.50f,
                5345.00f,
                5328.50f,
                5313.25f,
                5301.75f,
                5273.00f
            };
            
            int numHorizontalRayLevels = sizeof(HorizontalRayLevels) / sizeof(HorizontalRayLevels[0]);
            
            // Create zones for each horizontal ray level
            for (int i = 0; i < numHorizontalRayLevels; i++)
            {
                CreateStrikeZoneAroundLevel(sc, HorizontalRayLevels[i], 
                                         sc.ArraySize - 100, // Start a bit back from the end
                                         sc.ArraySize - 1,  // End at the last bar
                                         Config, ZoneCount);
            }
            
            // Flag that we now have zones
            hasZones = 1;
            
            char logMsg[100];
            sprintf(logMsg, "Created %d strike zones", ZoneCount);
            sc.AddMessageToLog(logMsg, 1);
        }
        
        // Update previous key states for next cycle
        prevButton1State = button1Down ? 1 : 0;
        prevButton2State = button2Down ? 1 : 0;
        prevButton3State = button3Down ? 1 : 0;
        prevButton4State = button4Down ? 1 : 0;
    }
}

// Function to create a strike zone around a price level
void CreateStrikeZoneAroundLevel(SCStudyInterfaceRef& sc, float Level, 
                              int BeginIndex, int EndIndex, 
                              StrikeZoneConfig& Config, int& ZoneCount)
{
    if (ZoneCount >= 99)
        return;  // Safety check to prevent overflow
    
    // Create Strike Zone centered on the given price level
    s_UseTool StrikeZone;
    
    StrikeZone.Clear();
    StrikeZone.DrawingType = DRAWING_RECTANGLEHIGHLIGHT;
    StrikeZone.ChartNumber = sc.ChartNumber;
    StrikeZone.Color = Config.Color;
    StrikeZone.LineWidth = Config.LineWidth;
    StrikeZone.LineStyle = (SubgraphLineStyles)Config.LineStyle;
    
    // Ensure we have a valid index range
    if (BeginIndex < 0) BeginIndex = 0;
    if (EndIndex >= sc.ArraySize) EndIndex = sc.ArraySize - 1;
    if (BeginIndex > EndIndex) BeginIndex = EndIndex - 10; // Fallback
    
    StrikeZone.BeginIndex = BeginIndex;
    StrikeZone.EndIndex = EndIndex;
    
    // Position perfectly centered on the level
    StrikeZone.BeginValue = Level - ((Config.HeightInTicks / 2.0f) * sc.TickSize);
    StrikeZone.EndValue = Level + ((Config.HeightInTicks / 2.0f) * sc.TickSize);
    
    // Add as new with transparency
    StrikeZone.AddMethod = UTAM_ADD_ALWAYS;
    StrikeZone.TransparencyLevel = 65;
    
    // Associate this drawing with the current study instance
    StrikeZone.AssociatedStudyID = sc.StudyGraphInstanceID;
    
    // Add the drawing
    sc.UseTool(StrikeZone);
    ZoneCount++;
}