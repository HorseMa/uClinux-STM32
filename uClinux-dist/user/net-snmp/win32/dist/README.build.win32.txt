***************************************************************************
*
* README.build.win32
*
* Authors: Alex Burger <alex_b@users.sourceforge.net>
*          
*
***************************************************************************

Introduction
============

This README outlines the steps required to create a binary release for
Windows using Microsoft Visual Studio and the NullSoft installer.

There are four sections:

  Compiling binaries

  Compiling HTMLHelp file

  Combining the binaries and HTMLHelp files

  Bulding a NullSoft installer package


Compiling binaries
==================

Requirements
------------

 -Windows NT/2000/XP
 -MSVC++ 6.0 SP5
 -ActivePerl 5.8.2 build 808
 -gnu_regex.exe (0.12) - http://people.delphiforums.com/gjc/gnu_regex.html
 -Platform SDK
 -MSYS / MinGW -or- tar.exe and gzip.exe


Building the main binaries
--------------------------

Note:  All shell steps are using the Window CMD prompt unless otherwise stated.

1.  Extract source.  The location will be references as (source dir)

2.  Delete c:\usr if it exists and rename Net-SNMP.dll in your %windir% if you 
    have it already (to ensure Perl tests are using the compiled DLL)

3.  Apply any required patches

4.  Remove the example MIB files:  

    Edit win32\net-snmp\agent\mib_module_config.h and change the following lines:

      #define USING_EXAMPLES_UCDDEMOPUBLIC_MODULE 1
      #define USING_EXAMPLES_EXAMPLE_MODULE 1

    to:

      #undef USING_EXAMPLES_UCDDEMOPUBLIC_MODULE
      #undef USING_EXAMPLES_EXAMPLE_MODULE    

5.  cd (source dir)\win32

6.  Run build.bat

7.  Set to the following:

    Net-SNMP build and install options
    ==================================
    
    1. OpenSSL support:      disabled
    2. Platform SDK support: enabled
    
    3. Install path:         c:/usr
    4. Install after build:  enabled
    
    5. Perl modules:         enabled
    6. Install perl modules: disabled
    
    7. Quiet build (logged): enabled
    8. Debug mode:           disabled
    9. IPv6 transports:      disabled

8.  F to start the build and verify everything was built ok

9.  Delete any generated config files from c:\usr\snmp\persist


Creating the Perl package
-------------------------

1.  cd (source dir)
    cd perl

2.  nmake ppd

3.  Open an MSYS shell (unless you have tar.exe and gzip.exe)

4.  cd (source dir)
    cd perl

5.  tar cvf Net-SNMP.tar blib; gzip --best Net-SNMP.tar

6.  Open a Windows command prompt (CMD) and cd (source dir)\perl

7.  ren Bundle-NetSNMP.ppd Net-SNMP.ppd

8.  Modify Net-SNMP.ppd to look like the following.  Change the 
    VERSION="x,x,x,x" line to the correct values.  Do NOT change 
    * lines in the original file.

<SOFTPKG NAME="Net-SNMP" VERSION="5,2,0,0">
    <TITLE>Net-SNMP</TITLE>
    <ABSTRACT>Object Oriented Interface to Net-SNMP</ABSTRACT>
    <AUTHOR></AUTHOR>
    <IMPLEMENTATION>
*        <OS NAME="MSWin32" />
*        <ARCHITECTURE NAME="MSWin32-x86-multi-thread-5.8" />
        <CODEBASE HREF="x86/Net-SNMP.tar.gz" />
    </IMPLEMENTATION>
</SOFTPKG>

9.  Create base directories:

    md "c:\usr\docs"
    md "c:\usr\perl"
    md "c:\usr\perl\x86"
    md "c:\usr\temp"

10. Copy files:

    cd (source dir)
    copy COPYING "c:\usr\docs"
    copy win32\dist\README.txt "c:\usr"
    copy win32\dist\scripts\net-snmp-perl-test.pl "c:\usr\bin"

    copy perl\Net-SNMP.ppd "c:\usr\Perl"
    copy perl\Net-SNMP.tar.gz "c:\usr\Perl\x86"

11. Update the BUILD INFORMATION section of c:\usr\README.txt


Compiling HTMLHelp file
=======================

This section outlines the steps required to build a Windows HTML Help file
based on the Net-SNMP man pages, README files, Perldoc documentation etc.

Requirements
------------

 -Linux (or similar) with:
   -man (man page viewer)
   -tidy (HTML tidy - http://tidy.sourceforge.net)
   -Perl
   -perldoc
   -man2html3.0.1.tar.gz (http://search.cpan.org/~ehood/man2html3.0.1/)
    Note:  Most Linux distributions come with man2html as part of the man rpm
           package which is not the same as man2html from man2html3.0.1.tar.gz.
           All that is needed from man2html3.0.1.tar.gz is the man2html script.  
           You do not need to do a complete install (make/make install) but you
           do need to make sure the script is configured correctly by setting the
           man command line switches etc for the OS from inside of the script.

 -Windows with:
   -HTML Help Workshop (Search msdn.microsoft.com for 'html help workshop download')


Convert documents to html
-------------------------

Note:  The following steps are completed using Linux.

Note:  A temporary location of /tmp/net-snmp is used.

1.  Extract Net-SNMP source and cd (source dir)

2.  cd (source dir)

3.  cp perl/SNMP/README README.perl

4.  Remove older copies of converted files:

    rm -R -f /tmp/net-snmp

5.  Build Net-SNMP man pages: 

    ./configure; make sedscript; cd man;make; cd ..

6.  Install only the man files to /temp/net-snmp:

    cd man; make install prefix=/tmp/net-snmp; cd ..

7.  Go to the scripts folder and make sure all the scripts are executable:

    cd (source dir)
    cd win32/dist/scripts; chmod +x *

8.  Edit these files and make sure the paths are correct in the OPTIONS 
    section.  Also ensure the list of README files and Perl modules 
    are correct in readme2html and poddir2html.

    mandir2html
    readme2html
    poddir2html

    Note:  mandir2html will process ALL man pages in c:\temp\net-snmp while
           readme2html and poddir2html will only process files listed in the
           script.  If new man pages are added or removed, the Table of 
           Contents (toc.hhc) and project file (Net-SNMP.hhp) need to be 
           updated by hand.

9.  Run each script to generate the .html files:

    ./mandir2html; ./readme2html; ./poddir2html

    Note:  There will be many warnings from tidy which can be ignored.

10. Verify each converted file to ensure all the links are correct.  The files
    are located in /tmp/net-snmp/html by default.  In some instances, URLs may be 
    split across two lines such as the Variables link at the bottom of 
    man8-snmptrapd.8.html.

11. If new man pages are added or removed, the Table of Contents (Net-SNMP.hhc) and
    project file (Net-SNMP.hhp) need to be updated by hand.

12. Convert EXAMPLE.conf.win32 to html:

    ./txt2html ../../EXAMPLE.conf.win32 | tidy > /tmp/net-snmp/html/EXAMPLE.conf.win32.html
    

Build Net-SNMP.chm
------------------

Note:  The following steps are completed using Windows.

Note:  A temporary location of c:\temp\net-snmp is used.

1.  Transfer /tmp/net-snmp/html from Linux to c:\temp\net-snmp\html

2.  Copy the following files to c:\temp\net-snmp\html:

    cd (source dir)
    copy "win32\dist\htmlhelp\Configuration_Overview.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\Developer_FAQ.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\FAQ.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\Help_Caveats.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\Introduction.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\Net-SNMP.hhc" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\Net-SNMP.hhp" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\net-snmp-4.2-800.jpg" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\snmp.conf.win32.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\snmpd.conf.win32.html" c:\temp\net-snmp\html\
    copy "win32\dist\htmlhelp\snmptrapd.conf.win32.html" c:\temp\net-snmp\html\

3.  New configuration options may be available in the new release of
    Net-SNMP, so the *.conf.win32.html files should be updated.

    Create a text file with all the configuration options for snmpd and 
    snmptrapd using:

    cd win32\bin\release (folder of *Windows* compiled Net-SNMP)
    snmptrapd -H 2> c:\temp\net-snmp\html\snmptrapd.options
    snmpd -H 2> c:\temp\net-snmp\html\snmpd.options

    Update these files using an HTML editor (Mozilla etc):

    c:\temp\net-snmp\html\snmp.conf.win32.html
    c:\temp\net-snmp\html\snmpd.conf.win32.html
    c:\temp\net-snmp\html\snmptrapd.conf.win32.html
 
    Only add the relevent section to each file from the .options files
    created above, ensure the font is set to fixed width.

    Tidy each file using tidy under Windows (or transfer to Linux and tidy
    using Linux):

    tidy -asxhtml -m c:\temp\net-snmp\html\snmptrapd.conf.win32.html
    tidy -asxhtml -m c:\temp\net-snmp\html\snmpd.conf.win32.html
    tidy -asxhtml -m c:\temp\net-snmp\html\snmp.conf.win32.html

4.  Edit c:\temp\net-snmp\html\Net-SNMP.hhp and update the version for the 
    'Title' variable.

5.  Run HTML Workshop

6.  Open c:\temp\net-snmp\html\Net-SNMP.hhp

7.  Click File - Compile

8.  Select 'C:\temp\net-snmp\html\Net-SNMP.hhp' as the filename

9.  Click Compile

10. You should now have a c:\temp\net-snmp\html\Net-SNMP.chm file.

11. Launch the file and ensure every content item displays the correct page.


Combining the binaries and HTMLHelp files
=========================================

1.  Copy the HTML Help file to c:\usr\docs:

    copy c:\temp\Net-SNMP\html\Net-SNMP.chm c:\usr\docs\

2.  Create a .zip file of c:\usr for archive purposes.


Bulding a NullSoft installer package
====================================

Requirements
------------

 -Windows
 -Nullsoft Scriptable Install System 2.0 - http://nsis.sourceforge.net/home/

1.  Complete the three sections above:  'Compiling binaries', 'Compiling 
    HTMLHelp file' and 'Combining the binaries and HTMLHelp files'.  Net-SNMP
    should be located in c:\usr.

2.  Copy the following files to c:\usr:

    cd (source dir)
    copy win32\dist\installer\SetEnVar.nsi c:\usr\
    copy win32\dist\installer\net-snmp.nsi c:\usr\
    copy win32\dist\installer\Add2Path.nsi c:\usr\
    copy win32\dist\installer\net-snmp-header1.bmp c:\usr\

3.  Create the following empty files:

    echo . > c:\usr\registeragent.bat
    echo . > c:\usr\unregisteragent.bat
    echo . > c:\usr\registertrapd.bat
    echo . > c:\usr\unregistertrapd.bat
    echo . > c:\usr\etc\snmp\snmp.conf

4.  Edit the following variables in c:\usr\net-snmp.nsi:

    PRODUCT_MAJ_VERSION
    PRODUCT_MIN_VERSION
    PRODUCT_REVISION
    PRODUCT_EXE_VERSION

    For example, for 5.1.2:

    PRODUCT_MAJ_VERSION "5"
    PRODUCT_MIN_VERSION "1"
    PRODUCT_REVISION "2"
    PRODUCT_EXE_VERSION "1"

    The generated filename would be: net-snmp-5.1.2-1.win32.exe

    PRODUCT_EXE_VERSION is usually 1 unless the binary package is re-released
    due to packaging issues.  For pre releases, include the pre-release version 
    in PRODUCT_REVISION.   For example, for 5.1.2 pre2 use:

    PRODUCT_MAJ_VERSION "5"
    PRODUCT_MIN_VERSION "1"
    PRODUCT_REVISION "2.pre2"
    PRODUCT_EXE_VERSION "1"

    This will ensure the version number is visible during installation.

    The generated filename would be: net-snmp-5.1.2.pre2-1.win32.exe

5.  Launch the 'Nullsoft Install System (NSIS 2.0)'

6.  Select 'MakeNSISW (compiler interface)'

7.  Click File - Load Script

8.  Select c:\usr\net-snmp.nsi

9.  You should now have a c:\usr\Net-SNMP-x.x.x-x.exe binary installer 
    package

10. Test the package

11. Compare the directory contents of the compiled folder with the installed
    folder to ensure there are no missing MIB files etc.  Modify net-snmp.nsi
    and rebuild if required.
