#!/bin/bash -efux

# Download cpanm and make it executable as a standalone script
curl -L https://cpanmin.us -o cpanm
chmod 0755 cpanm

sudo ./cpanm --notest Test2::V0 > build.log 2>&1 \
    || (cat build.log && exit 1)

if [ "${APT_INSTALL-}" ]; then
    sudo apt-get install -y $APT_INSTALL
fi

git clone --depth 1 -b $OPENSSL_BRANCH https://github.com/openssl/openssl.git
if [ "${PATCH_OPENSSL}" == "1" ]; then
    git apply patches/openssl-tls1.3.patch
    git apply patches/openssl-asn1_item_verify_ctx.patch
    git apply patches/openssl-x509_sig_info_init.patch
    # Apply PKCS#12 provider-PBE patch for OpenSSL 3.6 provider builds
    git apply patches/pkcs12/openssl-pkcs12-provider-pbe-3.6.patch
fi

# Apply PKCS#12 provider-PBE patch for OpenSSL 4.x and master provider builds.
# The patch is strictly additive (fallback only fires when OBJ_NAME_get returns
# NULL) so it is safe to apply unconditionally; non-provider builds are
# unaffected.  Tolerate failures on master where line numbers may have drifted.
case "${OPENSSL_BRANCH}" in
    openssl-4.0.0)
        git apply patches/pkcs12/openssl-pkcs12-provider-pbe-4.0.patch
        ;;
    master)
        git apply patches/pkcs12/openssl-pkcs12-provider-pbe-4.0.patch || \
            echo "NOTE: pkcs12 provider-pbe patch did not apply cleanly to master; PKCS12 provider tests will skip"
        ;;
esac
cd openssl
git describe --always --long

PREFIX=$HOME/opt

${SETARCH-} ./config shared -d --prefix=$PREFIX --libdir=lib --openssldir=$PREFIX ${USE_RPATH:+-Wl,-rpath=$PREFIX/lib}
${SETARCH-} make -s -j$(nproc) build_libs
${SETARCH-} make -s -j$(nproc) build_programs
make -s install_sw
