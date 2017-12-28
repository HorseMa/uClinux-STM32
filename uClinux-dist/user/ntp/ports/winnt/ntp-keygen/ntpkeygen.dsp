# Microsoft Developer Studio Project File - Name="ntpkeygen" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ntpkeygen - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ntpkeygen.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ntpkeygen.mak" CFG="ntpkeygen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ntpkeygen - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ntpkeygen - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/ntp/dev/ports/winnt/ntpkeygen", OWBAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ntpkeygen - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O2 /I "." /I "..\include" /I "..\..\..\include" /I "$(OPENSSL)\inc32" /I "..\..\..\libopts" /D "NDEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D "__STDC__" /D "SYS_WINNT" /D "HAVE_CONFIG_H" /D _WIN32_WINNT=0x400 /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib advapi32.lib ws2_32.lib $(OPENSSL)\out32dll\libeay32.lib   ..\libntp\Release\libntp.lib /nologo /subsystem:console /machine:I386 /out:"../bin/Release/ntp-keygen.exe"

!ELSEIF  "$(CFG)" == "ntpkeygen - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W4 /Gm /GX /ZI /Od /I "." /I "..\include" /I "..\..\..\include" /I "$(OPENSSL)\inc32" /I "..\..\..\libopts" /D "_DEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D "__STDC__" /D "SYS_WINNT" /D "HAVE_CONFIG_H" /D _WIN32_WINNT=0x400 /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib advapi32.lib ws2_32.lib $(OPENSSL)\out32dll\libeay32.lib  ..\libntp\Debug\libntp.lib /nologo /subsystem:console /debug /machine:I386 /out:"../bin/Debug/ntp-keygen.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "ntpkeygen - Win32 Release"
# Name "ntpkeygen - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\libntp\getopt.c
# End Source File
# Begin Source File

SOURCE="..\..\..\util\ntp-keygen-opts.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\util\ntp-keygen.c"
# End Source File
# Begin Source File

SOURCE=..\..\..\libntp\ntp_random.c
# End Source File
# Begin Source File

SOURCE=..\libntp\randfile.c
# End Source File
# Begin Source File

SOURCE=..\libntp\util_clockstuff.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\..\..\configure

!IF  "$(CFG)" == "ntpkeygen - Win32 Release"

# Begin Custom Build
ProjDir=.
InputPath=..\..\..\configure

"$(ProjDir)\version.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo Using NT Shell Script to generate version.c 
	..\scripts\mkver.bat -P ntpkeygen 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ntpkeygen - Win32 Debug"

# Begin Custom Build
ProjDir=.
InputPath=..\..\..\configure

"$(ProjDir)\version.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo Using NT Shell Script to generate version.c 
	..\scripts\mkver.bat -P ntpkeygen 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
