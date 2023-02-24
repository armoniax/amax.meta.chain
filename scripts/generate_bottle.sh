#!/usr/bin/env bash
set -eo pipefail

OS_VER=$(sw_vers -productVersion)
OS_MAJ=$(echo "${OS_VER}" | cut -d'.' -f1)
OS_MIN=$(echo "${OS_VER}" | cut -d'.' -f2)
OS_PATCH=$(echo "${OS_VER}" | cut -d'.' -f3)

if [[ ${OS_MAJ} -eq 10 ]] && [[ ${OS_MIN} -eq 13 ]]; then
   MAC_VERSION="high_sierra"
elif [[ ${OS_MAJ} -eq 10 ]] && [[ ${OS_MIN} -eq 14 ]]; then
   MAC_VERSION="mojave"
elif [[ ${OS_MAJ} -eq 12 ]]; then
   MAC_VERSION="monterey"
elif [[ ${OS_MAJ} -eq 13 ]]; then
   MAC_VERSION="ventura"
else
   echo "Error, unsupported OS X version:${OS_VER}"
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

echo "class Amax < Formula
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
   depends_on macos: :monterey
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
