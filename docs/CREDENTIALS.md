This file contains a list of all sensitive credentials, access keys, authentication tokens, and security certificates used by CI/CD (Github Actions).

- [PRIVATE\_H](#private_h)
- [RELEASE\_KEYSTORE](#release_keystore)
- [SECURE\_PROPERTIES](#secure_properties)
- [FIREBASE\_APP\_DISTRIBUTION\_JSON](#firebase_app_distribution_json)
- [FIREBASE\_TEST\_LAB\_JSON](#firebase_test_lab_json)
- [GOOGLE\_SERVICES\_JSON](#google_services_json)
- [GOOGLE\_PLAY\_JSON](#google_play_json)
- [HUAWEI\_APPGALLERY\_JSON](#huawei_appgallery_json)
- [AGCONNECT\_SERVICES\_JSON](#agconnect_services_json)
- [APPSTORE\_JSON](#appstore_json)
- [CERTIFICATES\_DEV\_P12](#certificates_dev_p12)
- [CERTIFICATES\_DISTR\_P12](#certificates_distr_p12)
- [APPSTORE\_CERTIFICATE\_PASSWORD](#appstore_certificate_password)

## PRIVATE_H

Shared compile-time secrets for all platforms.

```bash
gh secret set PRIVATE_H --env beta --body "$(base64 < private.h)"
gh secret set PRIVATE_H --env production --body "$(base64 < private.h)"
```

## RELEASE_KEYSTORE

Android Java-compatible keystore with certificates used for signing APKs.

```bash
gh secret set RELEASE_KEYSTORE --env beta --body "$(base64 < android/app/release.keystore)"
gh secret set RELEASE_KEYSTORE --env production --body "$(base64 < android/app/release.keystore)"
```

## SECURE_PROPERTIES

Android Gradle configuration file containing the passwords for the `release.keystore`.

```bash
gh secret set SECURE_PROPERTIES --env beta --body "$(base64 < android/app/secure.properties)"
gh secret set SECURE_PROPERTIES --env production --body "$(base64 < android/app/secure.properties)"
```

## FIREBASE_APP_DISTRIBUTION_JSON

Credentials for uploading betas to Google Firebase App Distribution.

```bash
gh secret set FIREBASE_APP_DISTRIBUTION_JSON --env beta --body "$(base64 < android/app/firebase-app-distribution.json)"
```

## FIREBASE_TEST_LAB_JSON

Credentials for using Firebase Test Lab ("Monkey").

```bash
gh secret set FIREBASE_TEST_LAB_JSON --env beta --body "$(base64 < android/app/firebase-test-lab.json)"
```

## GOOGLE_SERVICES_JSON

Credentials for using Firebase Crashlytics.

```bash
gh secret set GOOGLE_SERVICES_JSON --env beta --body "$(base64 < android/app/google-services.json)"
```

## GOOGLE_PLAY_JSON

Credentials for uploading Android releases to Google Play.

```bash
gh secret set GOOGLE_PLAY_JSON --env production --body "$(base64 < android/app/google-play.json)"
```

## HUAWEI_APPGALLERY_JSON

Credentials for uploading Android releases to Huawei AppGallery.

```bash
gh secret set HUAWEI_APPGALLERY_JSON --env production --body "$(base64 < android/app/huawei-appgallery.json)"
```

## AGCONNECT_SERVICES_JSON

Credentials for Huawei Mobile Services (HMS) to use Location Kit (not yet finished).

```bash
gh secret set AGCONNECT_SERVICES_JSON --env beta --body "$(base64 < android/app/agconnect-services.json)"
gh secret set AGCONNECT_SERVICES_JSON --env production --body "$(base64 < android/app/agconnect-services.json)"
```

## APPSTORE_JSON

Credentials for uploading iOS releases to Apple AppStore Connect.

```bash
gh secret set APPSTORE_JSON --env beta --body "$(base64 < xcode/keys/appstore.json)"
gh secret set APPSTORE_JSON --env production --body "$(base64 < xcode/keys/appstore.json)"
```

## CERTIFICATES_DEV_P12

Credentials for signing iOS releases - dev keys.

```bash
gh secret set CERTIFICATES_DEV_P12 --env beta --body "$(base64 < xcode/keys/CertificatesDev.p12)"
gh secret set CERTIFICATES_DEV_P12 --env production --body "$(base64 < xcode/keys/CertificatesDev.p12)"
```

## CERTIFICATES_DISTR_P12

Credentials for signing iOS releases - AppStore keys.

```bash
gh secret set CERTIFICATES_DISTR_P12 --env beta --body "$(base64 < xcode/keys/CertificatesDistr.p12)"
gh secret set CERTIFICATES_DISTR_P12 --env production --body "$(base64 < xcode/keys/CertificatesDistr.p12)"
```

## APPSTORE_CERTIFICATE_PASSWORD

Password for `CertificatesDistr.p12`.

```bash
gh secret set APPSTORE_CERTIFICATE_PASSWORD --env beta
gh secret set APPSTORE_CERTIFICATE_PASSWORD --env production
```
