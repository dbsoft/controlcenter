#!/bin/sh
PLATFORM=`uname -s`
APPNAME=$1
BINNAME=$2
IDENTITY=$3

if [ $PLATFORM = "Darwin" ]
then
    rm -rf install/package
    mkdir install/package
    cp -prf "$APPNAME.app" install/package
    cp -f readme.txt install/package
    cp -f /usr/local/lib/libdwindows.dylib "install/package/$APPNAME.app/Contents/MacOS"
    cp -f /usr/local/lib/libdwcompat.dylib "install/package/$APPNAME.app/Contents/MacOS"
    cp -f $BINNAME "install/package/$APPNAME.app/Contents/MacOS"
    install_name_tool -change `otool -L $BINNAME | grep libdwindows | cut -d ' ' -f 1 | tr -d '\t'` @executable_path/libdwindows.dylib "install/package/$APPNAME.app/Contents/MacOS/$BINNAME"
    install_name_tool -change `otool -L $BINNAME | grep libdwcompat | cut -d ' ' -f 1 | tr -d '\t'` @executable_path/libdwcompat.dylib "install/package/$APPNAME.app/Contents/MacOS/$BINNAME"
    if [ -z "$IDENTITY" ]
    then
        IDENTITY="-"
    else
        echo "Signing code with identity: $IDENTITY"
    fi
    codesign --deep -s "$IDENTITY" "install/package/$APPNAME.app"
    #/Developer/Tools/SetFile -a V "install/package/$BINNAME.png"
    ln -s /Applications install/package/.
    rm -f "install/$APPNAME.dmg"
    hdiutil create -fs HFS+ -volname "$APPNAME" -srcfolder install/package/ "install/$APPNAME.dmg"
fi
