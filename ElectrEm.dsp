# Microsoft Developer Studio Project File - Name="New ElectrEm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=New ElectrEm - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ElectrEm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ElectrEm.mak" CFG="New ElectrEm - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "New ElectrEm - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "New ElectrEm - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "New ElectrEm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "New_ElectrEm___Win32_Release"
# PROP BASE Intermediate_Dir "New_ElectrEm___Win32_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MD /W3 /GX /O2 /I ".\Resources\Windows" /I "C:\Libs\SDL-1.2.11\include" /I "C:\Libs\zlib-1.2.3" /D "NDEBUG" /D "USE_NATIVE_GUI" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MD /W3 /GX /O2 /I ".\Resources\Windows" /I "C:\Libs\SDL-1.2.11\include" /I "C:\Libs\zlib-1.2.3" /D "NDEBUG" /D "USE_NATIVE_GUI" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 sdl.lib sdlmain.lib zlib.lib gdi32.lib user32.lib shell32.lib comdlg32.lib advapi32.lib /nologo /subsystem:windows /machine:I386 /libpath:"C:\Libs\zlib-1.2.3" /libpath:"C:\Libs\SDL-1.2.11\lib"
# ADD LINK32 sdl.lib sdlmain.lib zlib.lib gdi32.lib user32.lib shell32.lib comdlg32.lib advapi32.lib /nologo /subsystem:windows /machine:I386 /libpath:"C:\Libs\zlib-1.2.3" /libpath:"C:\Libs\SDL-1.2.11\lib"

!ELSEIF  "$(CFG)" == "New ElectrEm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "New_ElectrEm___Win32_Debug"
# PROP BASE Intermediate_Dir "New_ElectrEm___Win32_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MD /W3 /GX /O2 /I ".\Resources\Windows" /I "C:\Libs\SDL-1.2.11\include" /I "C:\Libs\zlib-1.2.3" /D "NDEBUG" /D "USE_NATIVE_GUI" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MD /W3 /GX /Zi /Od /I ".\Resources\Windows" /I "C:\Libs\SDL-1.2.11\include" /I "C:\Libs\zlib-1.2.3" /D "NDEBUG" /D "USE_NATIVE_GUI" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /FAs /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 sdl.lib sdlmain.lib zlib.lib gdi32.lib user32.lib shell32.lib comdlg32.lib advapi32.lib /nologo /subsystem:windows /machine:I386 /libpath:"C:\Libs\zlib-1.2.3" /libpath:"C:\Libs\SDL-1.2.11\lib"
# ADD LINK32 sdl.lib sdlmain.lib zlib.lib gdi32.lib user32.lib shell32.lib comdlg32.lib advapi32.lib /nologo /subsystem:windows /debug /machine:I386 /libpath:"C:\Libs\zlib-1.2.3" /libpath:"C:\Libs\SDL-1.2.11\lib"

!ENDIF 

# Begin Target

# Name "New ElectrEm - Win32 Release"
# Name "New ElectrEm - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Plus 3"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Plus3\Drive\DriveADF.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Plus3\Drive\DriveBase.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\PLUS3\DRIVE\DriveFDI.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Plus3\Drive\fdi2raw\fdi2raw.c
# End Source File
# Begin Source File

SOURCE=.\src\Plus3\Helper\Helper.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Plus3\Drive\Sector.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Plus3\wd1770.cpp
# End Source File
# End Group
# Begin Group "Tape"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Tape\FastTape.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tape\FeederCSW.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tape\Feeders.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tape\FeederUEF.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Tape\Tape.cpp
# End Source File
# End Group
# Begin Group "Display"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Display.cpp
# End Source File
# Begin Source File

SOURCE=.\src\DisplayTables.cpp
# End Source File
# Begin Source File

SOURCE=.\src\DisplayUpdate.cpp
# End Source File
# End Group
# Begin Group "GUI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\GUI\Windows\PreferencesDialog.cpp
# End Source File
# End Group
# Begin Group "HostMachine"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\HostMachine\HostMachine.cpp
# End Source File
# Begin Source File

SOURCE=.\src\HostMachine\WindowsHostMachine.cpp
# End Source File
# End Group
# Begin Group "Configuration"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Configuration\Config.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Configuration\ConfigurationStore.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Configuration\ElectronConfiguration.cpp
# End Source File
# End Group
# Begin Group "Plus 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\PLUS1\Plus1.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\PLUS1\Plus1.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\6502core.cpp
# End Source File
# Begin Source File

SOURCE=.\src\6502misc.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\BASIC.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ComponentBase.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Keyboard.cpp
# End Source File
# Begin Source File

SOURCE=.\src\Main.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ProcessPool.cpp
# End Source File
# Begin Source File

SOURCE=.\src\UEFChunk.cpp
# End Source File
# Begin Source File

SOURCE=.\src\UEFMain.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ULA.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Tape.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Tape\Internal.h
# End Source File
# Begin Source File

SOURCE=.\src\Tape\Tape.h
# End Source File
# End Group
# Begin Group "Display.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\Display.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\SRC\BASIC.h
# End Source File
# Begin Source File

SOURCE=.\src\HostMachine\HostMachine.h
# End Source File
# Begin Source File

SOURCE=.\src\GUI\Windows\PreferencesDialog.h
# End Source File
# Begin Source File

SOURCE=.\src\HostMachine\WindowsHostMachine.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Resources\Windows\ADF.ICO
# End Source File
# Begin Source File

SOURCE=.\RESOURCES\WINDOWS\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\Resources\Windows\ElectrEm.rc
# End Source File
# Begin Source File

SOURCE=.\Resources\Windows\Icon.ico
# End Source File
# Begin Source File

SOURCE=.\Resources\Windows\SSD.ICO
# End Source File
# Begin Source File

SOURCE=.\Resources\Windows\UEF.ICO
# End Source File
# Begin Source File

SOURCE=.\RESOURCES\WINDOWS\ueficon1.ico
# End Source File
# End Group
# End Target
# End Project
