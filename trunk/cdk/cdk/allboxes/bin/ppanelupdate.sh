#!/bin/sh
#
# ppanelupdate.sh
# Downloads ppanel(s) from the server and installs them if the versions differ

VERSION=0.12
RELEASE=openpli

#
# Check for new version
#
CURR_VERSION=`echo $VERSION | tr -d '.'`

if [ -x /var/bin/ppanelupdate.sh ]; then
   OTHER_VERSION=`grep ^VERSION= /var/bin/ppanelupdate.sh | cut -d= -f2 | tr -d '.'`
   [ "$OTHER_VERSION" -gt "$CURR_VERSION" ] && exec /var/bin/ppanelupdate.sh "$@"
fi

if [ -x /bin/ppanelupdate.sh ]; then
   OTHER_VERSION=`grep ^VERSION= /bin/ppanelupdate.sh | cut -d= -f2 | tr -d '.'`
   [ "$OTHER_VERSION" -gt "$CURR_VERSION" ] && exec /bin/ppanelupdate.sh "$@"
fi

TEMPPANEL=/tmp/ppanel.tar.gz.tmp
TEMPVER=/tmp/ppanel.ver
TEMPPANELDIR=/tmp/ppaneltemp
TEMPFILELIST=/tmp/ppanelfiles
TEMPREFRESH=/tmp/ppanelrefresh
TEMPWGETOUT=/tmp/ppanelwgetout

URL=`echo $1 | sed s/#RELEASE#/$RELEASE/`
DESTDIR=$2
VERURL=`echo $URL | sed s/.tar.gz/.ver/`

echo "PPanelupdate version $VERSION"

rm -f $TEMPREFRESH

# Retrieve current .ver file
CURRENTVERFILE=`echo $VERURL | sed "s/.*\///"`
if [ -f /var/etc/$CURRENTVERFILE ] ; then
   CURRENTVERSION=`cat /var/etc/$CURRENTVERFILE | grep VERSION | cut -f2 -d=`
else
	 # No version file, take version 0 to force download
	 CURRENTVERSION=0
fi

# Download .ver file first
if wget -O $TEMPVER "$VERURL" > $TEMPWGETOUT ; then
    cat $TEMPWGETOUT
    SERVERVERSION=`cat $TEMPVER | grep VERSION | cut -f2 -d=`
    SERVERMD5=`cat $TEMPVER | grep -v VERSION | cut -f1 -d' '`
    echo "Version on Dreambox: $CURRENTVERSION"
    echo "Version on server: $SERVERVERSION"
    VERFOUND=1
else
    # No version file, this looks like a non-PLi server
    cat $TEMPWGETOUT
    echo "Downloading from a non-PLi server"
    VERFOUND=0
fi

if [ $VERFOUND -eq 1 ] ; then
   # Compare versions
   if [ 0$SERVERVERSION -gt 0$CURRENTVERSION ] ; then
      # Download tarball
      if wget -O $TEMPPANEL "$URL" ; then
         DOWNLOADEDMD5=`md5sum $TEMPPANEL | cut -f1 -d' '`
         if [ x$DOWNLOADEDMD5 = x$SERVERMD5 ] ; then
            echo "Check 1: tarball checksum valid"
         else
            # MD5 do not match
            # In most cases the servers are not in sync now
            echo
            echo "Menu was already up-to-date"
            echo "But it looks like a new menu will"
            echo "be available in a short time"
            exit 0
         fi
      else
         echo
         echo "**************************************"
         echo "Server is down or you have no internet"
         echo "connection. Please try later again."
         echo "**************************************"
         exit 1
      fi
   else
	    echo
      echo "Menu was already up-to-date"
      exit 0
   fi
else
   # No version found, download tarball anyway
   if ! wget -O $TEMPPANEL "$URL" ; then
      echo
      echo "**************************************"
      echo "Server is down or you have no internet"
      echo "connection. Please try later again."
      echo "**************************************"
      exit 1
   fi
   # We could add here the code to check the diffs between files
fi

# Install tarball
mkdir -p $TEMPPANELDIR
tar xzvf $TEMPPANEL -C $TEMPPANELDIR > $TEMPFILELIST
for XMLFILES in `grep "\.xml"  $TEMPFILELIST` ; do
   if ! grep -q "</directory>" $TEMPPANELDIR/$XMLFILES  ; then
      echo "Check 2: $XMLFILES seems corrupt. Quitting!"
      exit 1
   else
      echo "Check 2: xml files in tarball valid"
   fi
done

# Trick with tar in order to copy over symlink when used in OE images
tar -C $DESTDIR -zxf $TEMPPANEL $TEMPFILELIST
for XMLFILES in `grep "\.xml"  $TEMPFILELIST` ; do
	translation_to_chinese $DESTDIR/$XMLFILES
done
if [ $VERFOUND -eq 1 ] ; then
	touch $TEMPREFRESH
	cp $TEMPVER /var/etc/$CURRENTVERFILE
fi
echo
echo "Menu updated!"

rm -Rf $TEMPPANEL $TEMPPANELDIR $TEMPFILELIST $TEMPVER $TEMPWGETOUT

#
# The End
#

translation_to_chinese(){
	sed -i "s/\"Download latest menu\"/\"\&#x4e0b;\&#x8f7d;\&#x6700;\&#x65b0;\&#x83dc;\&#x5355;\"/g" $1 >/dev/null
	sed -i "s/\"Enable automatic menu downloads\"/\"\&#x5141;\&#x8bb8;\&#x83dc;\&#x5355;\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\"/g" $1 >/dev/null
	sed -i "s/\"Disable automatic menu downloads\"/\"\&#x7981;\&#x6b62;\&#x83dc;\&#x5355;\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\"/g" $1 >/dev/null
	sed -i "s/\"Manual install\"/\"\&#x624b;\&#x52a8;\&#x5b89;\&#x88c5;\"/g" $1 >/dev/null
	sed -i "s/\"Downloads...\"/\"\&#x4e0b;\&#x8f7d;...\"/g" $1 >/dev/null
	sed -i "s/\"Addons\/Plugins...\"/\"\&#x6269;\&#x5c55;\/\&#x63d2;\&#x4ef6;\"/g" $1 >/dev/null
	sed -i "s/\"Channel Settings...\"/\"\&#x9891;\&#x9053;\&#x8bbe;\&#x7f6e;\"/g" $1 >/dev/null
	sed -i "s/\"Extra Services...\"/\"\&#x6269;\&#x5c55;\&#x670d;\&#x52a1;...\"/g" $1 >/dev/null
	sed -i "s/\"Games...\"/\"\&#x6e38;\&#x620f;...\"/g" $1 >/dev/null
	sed -i "s/\"Languages...\"/\"\&#x8bed;\&#x8a00;...\"/g" $1 >/dev/null
	sed -i "s/\"Remove Software...\"/\"\&#x5378;\&#x8f7d;\&#x8f6f;\&#x4ef6;...\"/g" $1 >/dev/null
	sed -i "s/\"For manual install, store\"/\"\&#x8981;\&#x624b;\&#x52a8;\&#x5b89;\&#x88c5;\&#xff0c;\&#x8bf7;\&#x5c06;\"/g" $1 >/dev/null
	sed -i "s/\"your .tar.gz file in \/tmp\"/\"\&#x4f60;\&#x7684;.tar.gz\&#x6587;\&#x4ef6;\&#x653e;\&#x5728;\/tmp \&#x4e0b;\"/g" $1 >/dev/null
	sed -i "s/\"Skins\/Skin Previews...\"/\"\&#x76ae;\&#x80a4;\/\&#x76ae;\&#x80a4;\&#x9884;\&#x89c8;...\"/g" $1 >/dev/null
	sed -i "s/\"Download Skins...\"/\"\&#x4e0b;\&#x8f7d;\&#x76ae;\&#x80a4;...\"/g" $1 >/dev/null
	sed -i "s/\"Preview Skins...\"/\"\&#x9884;\&#x89c8;\&#x76ae;\&#x80a4;...\"/g" $1 >/dev/null
	sed -i "s/\"Older Softcams...\"/\"\&#x65e7;\&#x7248;Softcams...\"/g" $1 >/dev/null
	sed -i "s/\"Automatically download menus if a new version is available\"/\"\&#x5982;\&#x679c;\&#x5b58;\&#x5728;\&#x65b0;\&#x7248;\&#x672c;\&#x5219;\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\&#x83dc;\&#x5355;\"/g" $1 >/dev/null
	sed -i "s/\"Download the latest Software Management menu\"/\"\&#x4e0b;\&#x8f7d;\&#x6700;\&#x65b0;\&#x7684;\&#x8f6f;\&#x4ef6;\&#x7ba1;\&#x7406;\&#x83dc;\&#x5355;\"/g" $1 >/dev/null
	sed -i "s/\"Disable the automatic download of menus\"/\"\&#x7981;\&#x6b62;\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\&#x83dc;\&#x5355;\"/g" $1 >/dev/null
	sed -i "s/\"Remove installed software on your Dreambox\"/\"\&#x5378;\&#x8f7d;\&#x63a5;\&#x6536;\&#x673a;\&#x4e0a;\&#x5df2;\&#x5b89;\&#x88c5;\&#x7684;\&#x8f6f;\&#x4ef6;\"/g" $1 >/dev/null
	sed -i "s/\"Are you sure to download the latest menu?\"/\"\&#x5378;\&#x8f7d;\&#x63a5;\&#x6536;\&#x673a;\&#x4e0a;\&#x5df2;\&#x5b89;\&#x88c5;\&#x7684;\&#x8f6f;\&#x4ef6;\&#x3f;\"/g" $1 >/dev/null
	sed -i "s/Automatic downloads enabled/\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\&#x5df2;\&#x542f;\&#x7528;/g" $1 >/dev/null
	sed -i "s/Automatic downloads disabled/\&#x81ea;\&#x52a8;\&#x66f4;\&#x65b0;\&#x5df2;\&#x7981;\&#x7528;/g" $1 >/dev/null
	sed -i "s/\"Download software on your Dreambox\"/\"\&#x4e0b;\&#x8f7d;\&#x8f6f;\&#x4ef6;\&#x5230;\&#x63a5;\&#x6536;\&#x673a;\&#x91cc;\"/g" $1 >/dev/null

}
