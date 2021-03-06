/*
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root.
 */
#include <stdint.h>
#include <stdbool.h>

#include "RiotDerEnc.h"
#include "RiotX509Bldr.h"

#define ASRT(_X) if(!(_X))      {goto Error;}
#define CHK(_X)  if(((_X)) < 0) {goto Error;}

// OIDs.  Note that the encoder expects a -1 sentinel.
static int riotOID[] = { 2,23,133,5,4,1,-1 };
static int ecdsaWithSHA256OID[] = { 1,2,840,10045,4,3,2,-1 };
static int ecPublicKeyOID[] = { 1,2,840,10045, 2,1,-1 };
static int prime256v1OID[] = { 1,2,840,10045, 3,1,7,-1 };
static int keyUsageOID[] = { 2,5,29,15,-1 };
static int extKeyUsageOID[] = { 2,5,29,37,-1 };
static int extAuthKeyIdentifierOID[] = { 2,5,29,35,-1 };
static int subjectAltNameOID[] = { 2,5,29,17,-1 };
static int clientAuthOID[] = { 1,3,6,1,5,5,7,3,2,-1 };
static int sha256OID[] = { 2,16,840,1,101,3,4,2,1,-1 };
static int commonNameOID[] = { 2,5,4,3,-1 };
static int countryNameOID[] = { 2,5,4,6,-1 };
static int orgNameOID[] = { 2,5,4,10,-1 };
static int basicConstraintsOID[] = { 2,5,29,19,-1 };

static int GenerateGuidFromSeed(char* nameBuf, uint32_t *nameBufLen, const uint8_t* seed, size_t seedLen)
{
    uint8_t digest[RIOT_DIGEST_LENGTH];
    int     result;

    RIOT_SHA256_Block(seed, seedLen, digest);
    result = Base64Encode(digest, 16, nameBuf, nameBufLen);
    return result;
}

static int
X509AddExtensions(
    DERBuilderContext   *Tbs,
    uint8_t             *DevIdPub,
    uint32_t             DevIdPubLen,
    uint8_t             *Fwid,
    uint32_t             FwidLen
)
// Create the RIoT extensions.  
{
    uint8_t     authKeyIdentifier[SHA1_DIGEST_LENGTH];
    RIOT_SHA1_Block(DevIdPub, DevIdPubLen, authKeyIdentifier);
    uint8_t     keyUsage = RIOT_X509_KEY_USAGE;
    uint8_t        extLen = 1;

    CHK(DERStartExplicit(Tbs, 3));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    
    // keyUsage
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, keyUsageOID));
    CHK(            DERStartEnvelopingOctetString(Tbs));
    CHK(                DERAddBitString(Tbs, &keyUsage, extLen)); 
    CHK(            DERPopNesting(Tbs));
    CHK(        DERPopNesting(Tbs));

    // extendedKeyUsage
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, extKeyUsageOID));
    CHK(            DERStartEnvelopingOctetString(Tbs));
    CHK(                DERStartSequenceOrSet(Tbs, true));
    CHK(                    DERAddOID(Tbs, clientAuthOID));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(        DERPopNesting(Tbs));

    // authKeyIdentifier
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, extAuthKeyIdentifierOID));
    CHK(            DERStartEnvelopingOctetString(Tbs));
    CHK(                DERStartSequenceOrSet(Tbs, true));
    CHK(                    DERStartExplicit(Tbs, 0));
    CHK(                        DERAddOctetString(Tbs, authKeyIdentifier, 20));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(        DERPopNesting(Tbs));
    
    // RIoT extension
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, riotOID));
    CHK(            DERStartEnvelopingOctetString(Tbs));
    CHK(                DERStartSequenceOrSet(Tbs, true));
    CHK(                    DERAddInteger(Tbs, 1));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERStartSequenceOrSet(Tbs, true));
    CHK(                            DERAddOID(Tbs, ecPublicKeyOID));
    CHK(                            DERAddOID(Tbs, prime256v1OID));
    CHK(                        DERPopNesting(Tbs));
    CHK(                        DERAddBitString(Tbs, DevIdPub, DevIdPubLen));
    CHK(                    DERPopNesting(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddOID(Tbs, sha256OID));
    CHK(                        DERAddOctetString(Tbs, Fwid, FwidLen));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(        DERPopNesting(Tbs));

    CHK(    DERPopNesting(Tbs));
    CHK(DERPopNesting(Tbs));

    return 0;

Error:
    return -1;
}

static int
X509AddX501Name(
    DERBuilderContext   *Context,
    const char          *CommonName,
    const char          *OrgName,
    const char          *CountryName
)
{
    CHK(    DERStartSequenceOrSet(Context, true));
    CHK(        DERStartSequenceOrSet(Context, false));
    CHK(            DERStartSequenceOrSet(Context, true));
    CHK(                DERAddOID(Context, commonNameOID));
    CHK(                DERAddUTF8String(Context, CommonName));
    CHK(            DERPopNesting(Context));
    CHK(        DERPopNesting(Context));
    CHK(        DERStartSequenceOrSet(Context, false));
    CHK(            DERStartSequenceOrSet(Context, true));
    CHK(                DERAddOID(Context, countryNameOID));
    CHK(                DERAddUTF8String(Context, CountryName));
    CHK(            DERPopNesting(Context));
    CHK(        DERPopNesting(Context));
    CHK(        DERStartSequenceOrSet(Context, false));
    CHK(            DERStartSequenceOrSet(Context, true));
    CHK(                DERAddOID(Context, orgNameOID));
    CHK(                DERAddUTF8String(Context, OrgName));
    CHK(            DERPopNesting(Context));
    CHK(        DERPopNesting(Context));
    CHK(    DERPopNesting(Context));

    return 0;

Error:
    return -1;
}

int
X509GetDeviceCertTBS(
    DERBuilderContext   *Tbs,
    RIOT_X509_TBS_DATA  *TbsData,
    RIOT_ECC_PUBLIC     *DevIdKeyPub,
    uint8_t             *RootKeyPub,
    uint32_t             RootKeyPubLen)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;
    uint8_t     keyUsage = RIOT_X509_KEY_USAGE;
    uint8_t     authKeyIdentifier[SHA1_DIGEST_LENGTH];

    RiotCrypt_ExportEccPub(DevIdKeyPub, encBuffer, &encBufferLen);

    if (RootKeyPub != NULL)
    {
        RIOT_SHA1_Block(RootKeyPub, RootKeyPubLen, authKeyIdentifier);
    }

    CHK(DERStartSequenceOrSet(Tbs, true));
    CHK(    DERAddShortExplicitInteger(Tbs, 2));
    CHK(    DERAddIntegerFromArray(Tbs, TbsData->SerialNum, RIOT_X509_SNUM_LEN));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddOID(Tbs, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->IssuerCommon, TbsData->IssuerOrg, TbsData->IssuerCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidFrom));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidTo));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->SubjectCommon, TbsData->SubjectOrg, TbsData->SubjectCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, ecPublicKeyOID));
    CHK(            DERAddOID(Tbs, prime256v1OID));
    CHK(        DERPopNesting(Tbs));
    CHK(        DERAddBitString(Tbs, encBuffer, encBufferLen));
    CHK(    DERPopNesting(Tbs));
    CHK(    DERStartExplicit(Tbs, 3));
    CHK(        DERStartSequenceOrSet(Tbs, true));

    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, keyUsageOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
                            encBufferLen = 1;
    CHK(                    DERAddBitString(Tbs, &keyUsage, encBufferLen)); // Actually 6bits
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, basicConstraintsOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddBoolean(Tbs, true));
    CHK(                        DERAddInteger(Tbs, 1));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));

    if(RootKeyPub!=NULL)
    {
        CHK(        DERStartSequenceOrSet(Tbs, true));
        CHK(            DERAddOID(Tbs, extAuthKeyIdentifierOID));
        CHK(            DERStartEnvelopingOctetString(Tbs));
        CHK(                DERStartSequenceOrSet(Tbs, true));
        CHK(                    DERStartExplicit(Tbs, 0));
        CHK(                        DERAddOctetString(Tbs, authKeyIdentifier, 20));
        CHK(                    DERPopNesting(Tbs));
        CHK(                DERPopNesting(Tbs));
        CHK(            DERPopNesting(Tbs));
        CHK(        DERPopNesting(Tbs));
    }

    CHK(        DERPopNesting(Tbs));
    CHK(    DERPopNesting(Tbs));
    CHK(DERPopNesting(Tbs));

    ASRT(DERGetNestingDepth(Tbs) == 0);
    return 0;

Error:
    return -1;
}

int
X509MakeDeviceCert(
    DERBuilderContext   *DeviceIDCert,
    RIOT_ECC_SIGNATURE  *TbsSig
)
// Create a Device Certificate given a ready-to-sign TBS region in the context
{
    uint8_t     encBuffer[((BIGLEN - 1) * 4)];
    uint32_t    encBufferLen = ((BIGLEN - 1) * 4);

    // Elevate the "TBS" block into a real certificate,
    // i.e., copy it into an enclosing sequence.
    CHK(DERTbsToCert(DeviceIDCert));
    CHK(    DERStartSequenceOrSet(DeviceIDCert, true));
    CHK(        DERAddOID(DeviceIDCert, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(DeviceIDCert));
    CHK(    DERStartEnvelopingBitString(DeviceIDCert));
    CHK(        DERStartSequenceOrSet(DeviceIDCert, true));
                    BigValToBigInt(encBuffer, &TbsSig->r);
    CHK(            DERAddIntegerFromArray(DeviceIDCert, encBuffer, encBufferLen));
                    BigValToBigInt(encBuffer, &TbsSig->s);
    CHK(            DERAddIntegerFromArray(DeviceIDCert, encBuffer, encBufferLen));
    CHK(        DERPopNesting(DeviceIDCert));
    CHK(    DERPopNesting(DeviceIDCert));
    CHK(DERPopNesting(DeviceIDCert));

    ASRT(DERGetNestingDepth(DeviceIDCert) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetAliasCertTBS(
    DERBuilderContext   *Tbs,
    RIOT_X509_TBS_DATA  *TbsData,
    RIOT_ECC_PUBLIC     *AliasKeyPub,
    RIOT_ECC_PUBLIC     *DevIdKeyPub,
    uint8_t             *Fwid,
    uint32_t             FwidLen
)
{
    int result;
    char guidBuffer[64];
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;

    if (strcmp(TbsData->SubjectCommon, "*") == 0)
    {
        RiotCrypt_ExportEccPub(DevIdKeyPub, encBuffer, &encBufferLen);
        uint32_t bufLen = sizeof(guidBuffer);

        // Replace the common-name with a per-device GUID derived from the DeviceID public key
        result = GenerateGuidFromSeed(guidBuffer, &bufLen, encBuffer, encBufferLen);

        if (result < 0) {
            return result;
        }

        guidBuffer[bufLen-1] = 0;
        TbsData->SubjectCommon = guidBuffer;
    }

    CHK(DERStartSequenceOrSet(Tbs, true));
    CHK(    DERAddShortExplicitInteger(Tbs, 2));
    CHK(    DERAddIntegerFromArray(Tbs, TbsData->SerialNum, RIOT_X509_SNUM_LEN));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddOID(Tbs, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->IssuerCommon, TbsData->IssuerOrg, TbsData->IssuerCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidFrom));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidTo));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->SubjectCommon, TbsData->SubjectOrg, TbsData->SubjectCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, ecPublicKeyOID));
    CHK(            DERAddOID(Tbs, prime256v1OID));
    CHK(        DERPopNesting(Tbs));
                RiotCrypt_ExportEccPub(AliasKeyPub, encBuffer, &encBufferLen);
    CHK(        DERAddBitString(Tbs, encBuffer, encBufferLen));
    CHK(    DERPopNesting(Tbs));
            RiotCrypt_ExportEccPub(DevIdKeyPub, encBuffer, &encBufferLen);
    CHK(    X509AddExtensions(Tbs, encBuffer, encBufferLen, Fwid, FwidLen));
    CHK(DERPopNesting(Tbs));
    
    ASRT(DERGetNestingDepth(Tbs) == 0);
    return 0;

Error:
    return -1;
}

int 
X509MakeAliasCert(
    DERBuilderContext   *AliasCert,
    RIOT_ECC_SIGNATURE  *TbsSig
)
// Create an Alias Certificate given a ready-to-sign TBS region in the context
{
    uint8_t     encBuffer[((BIGLEN - 1) * 4)];
    uint32_t    encBufferLen = ((BIGLEN - 1) * 4);

    // Elevate the "TBS" block into a real certificate,
    // i.e., copy it into an enclosing sequence.
    CHK(DERTbsToCert(AliasCert));   
    CHK(    DERStartSequenceOrSet(AliasCert, true));
    CHK(        DERAddOID(AliasCert, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(AliasCert));
    CHK(    DERStartEnvelopingBitString(AliasCert));
    CHK(        DERStartSequenceOrSet(AliasCert, true));
                    BigValToBigInt(encBuffer, &TbsSig->r);
    CHK(            DERAddIntegerFromArray(AliasCert, encBuffer, encBufferLen));
                    BigValToBigInt(encBuffer, &TbsSig->s);
    CHK(            DERAddIntegerFromArray(AliasCert, encBuffer, encBufferLen));
    CHK(        DERPopNesting(AliasCert));
    CHK(    DERPopNesting(AliasCert));
    CHK(DERPopNesting(AliasCert));

    ASRT(DERGetNestingDepth(AliasCert) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetDEREccPub(
    DERBuilderContext   *Context,
    RIOT_ECC_PUBLIC      Pub
)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;

    CHK(DERStartSequenceOrSet(Context, true));
    CHK(    DERStartSequenceOrSet(Context, true));
    CHK(        DERAddOID(Context, ecPublicKeyOID));
    CHK(        DERAddOID(Context, prime256v1OID));
    CHK(    DERPopNesting(Context));
            RiotCrypt_ExportEccPub(&Pub, encBuffer, &encBufferLen);
    CHK(    DERAddBitString(Context, encBuffer, encBufferLen));
    CHK(DERPopNesting(Context));

    ASRT(DERGetNestingDepth(Context) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetDEREcc(
    DERBuilderContext   *Context,
    RIOT_ECC_PUBLIC      Pub,
    RIOT_ECC_PRIVATE     Priv
)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;

    CHK(DERStartSequenceOrSet(Context, true));
    CHK(    DERAddInteger(Context, 1));
            BigValToBigInt(encBuffer, &Priv);
    CHK(    DERAddOctetString(Context, encBuffer, 32));
    CHK(    DERStartExplicit(Context, 0));
    CHK(        DERAddOID(Context, prime256v1OID));
    CHK(    DERPopNesting(Context));
    CHK(    DERStartExplicit(Context, 1));
                RiotCrypt_ExportEccPub(&Pub, encBuffer, &encBufferLen);
    CHK(        DERAddBitString(Context, encBuffer, encBufferLen));
    CHK(    DERPopNesting(Context));
    CHK(DERPopNesting(Context));

    ASRT(DERGetNestingDepth(Context) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetDERCsrTbs(
    DERBuilderContext   *Context,
    RIOT_X509_TBS_DATA  *TbsData,
    RIOT_ECC_PUBLIC*      DeviceIDPub
)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;

    CHK(DERStartSequenceOrSet(Context, true));
    CHK(    DERAddInteger(Context, 0));
    CHK(    X509AddX501Name(Context, TbsData->IssuerCommon, TbsData->IssuerOrg, TbsData->IssuerCountry));
    CHK(    DERStartSequenceOrSet(Context, true));
    CHK(        DERStartSequenceOrSet(Context, true));
    CHK(            DERAddOID(Context, ecPublicKeyOID));
    CHK(            DERAddOID(Context, prime256v1OID));
    CHK(        DERPopNesting(Context));
                RiotCrypt_ExportEccPub(DeviceIDPub, encBuffer, &encBufferLen);
    CHK(        DERAddBitString(Context, encBuffer, encBufferLen));
    CHK(    DERPopNesting(Context));
    CHK(DERStartExplicit(Context,0));
    CHK(DERPopNesting(Context));
    CHK(DERPopNesting(Context));

    ASRT(DERGetNestingDepth(Context) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetDERCsr(
    DERBuilderContext   *Context,
    RIOT_ECC_SIGNATURE  *Signature
)
{
    uint8_t     encBuffer[((BIGLEN - 1) * 4)];
    uint32_t    encBufferLen = ((BIGLEN - 1) * 4);

    // Elevate the "TBS" block into a real certificate, i.e., copy it
    // into an enclosing sequence and then add the signature block.
    CHK(DERTbsToCert(Context));
    CHK(    DERStartSequenceOrSet(Context, true));
    CHK(        DERAddOID(Context, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Context));
    CHK(    DERStartEnvelopingBitString(Context));
    CHK(        DERStartSequenceOrSet(Context, true));
                    BigValToBigInt(encBuffer, &Signature->r);
    CHK(            DERAddIntegerFromArray(Context, encBuffer, encBufferLen));
                    BigValToBigInt(encBuffer, &Signature->s);
    CHK(            DERAddIntegerFromArray(Context, encBuffer, encBufferLen));
    CHK(        DERPopNesting(Context));
    CHK(    DERPopNesting(Context));
    CHK(DERPopNesting(Context));

    ASRT(DERGetNestingDepth(Context) == 0);
    return 0;

Error:
    return -1;
}

int
X509GetRootCertTBS(
    DERBuilderContext   *Tbs,
    RIOT_X509_TBS_DATA  *TbsData,
    RIOT_ECC_PUBLIC     *RootKeyPub
)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;
    uint8_t     keyUsage = RIOT_X509_KEY_USAGE;

    CHK(DERStartSequenceOrSet(Tbs, true));
    CHK(    DERAddShortExplicitInteger(Tbs, 2));
    CHK(    DERAddIntegerFromArray(Tbs, TbsData->SerialNum, RIOT_X509_SNUM_LEN));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddOID(Tbs, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->IssuerCommon, TbsData->IssuerOrg, TbsData->IssuerCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidFrom));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidTo));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->SubjectCommon, TbsData->SubjectOrg, TbsData->SubjectCountry));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERAddOID(Tbs, ecPublicKeyOID));
    CHK(            DERAddOID(Tbs, prime256v1OID));
    CHK(        DERPopNesting(Tbs));
                RiotCrypt_ExportEccPub(RootKeyPub, encBuffer, &encBufferLen);
    CHK(        DERAddBitString(Tbs, encBuffer, encBufferLen));
    CHK(    DERPopNesting(Tbs));
    CHK(    DERStartExplicit(Tbs, 3));
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, keyUsageOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
                            encBufferLen = 1;
    CHK(                    DERAddBitString(Tbs, &keyUsage, encBufferLen)); // Actually 6bits
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, basicConstraintsOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddBoolean(Tbs, true));
    CHK(                        DERAddInteger(Tbs, 2));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(        DERPopNesting(Tbs));
    CHK(    DERPopNesting(Tbs));
    CHK(DERPopNesting(Tbs));

    ASRT(DERGetNestingDepth(Tbs) == 0);
    return 0;

Error:
    return -1;
}

int
X509MakeRootCert(
    DERBuilderContext   *RootCert,
    RIOT_ECC_SIGNATURE  *TbsSig
)
// Create an Alias Certificate given a ready-to-sign TBS region in the context
{
    uint8_t     encBuffer[((BIGLEN - 1) * 4)];
    uint32_t    encBufferLen = ((BIGLEN - 1) * 4);

    // Elevate the "TBS" block into a real certificate,
    // i.e., copy it into an enclosing sequence.
    CHK(DERTbsToCert(RootCert));
    CHK(    DERStartSequenceOrSet(RootCert, true));
    CHK(        DERAddOID(RootCert, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(RootCert));
    CHK(    DERStartEnvelopingBitString(RootCert));
    CHK(        DERStartSequenceOrSet(RootCert, true));
                    BigValToBigInt(encBuffer, &TbsSig->r);
    CHK(            DERAddIntegerFromArray(RootCert, encBuffer, encBufferLen));
                    BigValToBigInt(encBuffer, &TbsSig->s);
    CHK(            DERAddIntegerFromArray(RootCert, encBuffer, encBufferLen));
    CHK(        DERPopNesting(RootCert));
    CHK(    DERPopNesting(RootCert));
    CHK(DERPopNesting(RootCert));

    ASRT(DERGetNestingDepth(RootCert) == 0);
    return 0;

Error:
    return -1;
}

