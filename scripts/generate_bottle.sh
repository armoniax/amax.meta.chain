#!/usr/bin/env bash
set -eo pipefail

VERS_10_13=`sw_vers -productVersion | awk '/10\.13\..*/{print $0}'`
VERS_10_14=`sw_vers -productVersion | awk '/10\.14\..*/{print $0}'`
VERS_12=`sw_vers -productVersion | awk '/12\.14\..*/{print $0}'`
if [[ -n "$VERS_10_13" ]];
then
   MAC_VERSION="high_sierra"
elif [[ -n "$VERS_10_14" ]];
then
   MAC_VERSION="mojave"
elif [[ -n "$VERS_12" ]];
then
   MAC_VERSION="monterey"
else
   echo "Error, unsupported OS X version"
   exit -1
fi

NAME="${PROJECT}-${VERSION}.${MAC_VERSION}.bottle"

mkdir -p ${PROJECT}/${VERSION}/opt/amax/lib/cmake

PREFIX="${PROJECT}/${VERSION}"
SPREFIX="\/usr\/local"
SUBPREFIX="opt/${PROJECT}"
SSUBPREFIX="opt\/${PROJECT}"

export PREFIX
export SPREFIX
export SUBPREFIX
export SSUBPREFIX

. ./generate_tarball.sh ${NAME}

hash=`openssl dgst -sha256 ${NAME}.tar.gz | awk 'NF>1{print $NF}'`

echo "class Eosio < Formula
   # typed: false
   # frozen_string_literal: true
   
   homepage \"${URL}\"
   revision 0
   url \"https://github.com/armoniax/amachain/archive/v${VERSION}.tar.gz\"
   version \"${VERSION}\"

   option :universal

   depends_on \"gmp\"
   depends_on \"gettext\"
   depends_on \"openssl@1.1\"
   depends_on \"libusb\"
   depends_on macos: :mojave
   depends_on arch: :intel

   bottle do
      root_url \"https://github.com/armoniax/amachain/releases/download/v${VERSION}\"
      sha256 ${MAC_VERSION}: \"${hash}\"
   end
   def install
      raise \"Error, only supporting binary packages at this time\"
   end
end
__END__" &> amax.rb

rm -r ${PROJECT} || exit 1
