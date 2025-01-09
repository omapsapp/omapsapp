# Release Management

## Guidelines

The release management follows the **Fixed-Time, Variable-Scope** principle. This means that releases occur on a **predictable** schedule, while the scope of each release **may vary**.

### Schedule

Everyone should plan with the expectation that releases will happen predictably **every month**.

The current objective is to perform 1 (one) **feature release** (containing new features) per month, with the target of uploading the App Store and Google Play versions for review by the first Monday of each month.

In addition to the monthly feature release, a **data-only release** (containing only updated OSM data, with no new features) shall occur in the middle of the month, typically around the 15th.

### Scope

Everyone should expect that each release will include only what was finished on time and **won't be delayed** to include any particular "almost ready" change (there could be exceptions for important hot fixes only).

Hence the scope of each release **may vary** based on the readiness of the features. Volunteer Contributors work at their own pace while the funded work adheres to the scope and schedule outlined in the agreed project plan.

### Schedule vs Scope

Each release aims to include all features that have been merged into the master branch before the release cut-off date and have not caused regressions during the QA validation process. If certain changes are not included in the current release, they will be postponed to the next one.

The general recommendation is to merge large changes immediately after the release to allow the entire month for fixing potential regressions and stabilizing the system.

### Release Manager

Each release has one person appointed as the Release Manager to oversee the upcoming release.

It is the duty of the Release Manager to drive the entire process to the successful completion, utilizing all available resources and means. Specific tasks may be delegated to individual team members as required.

The person holds the authority to make final decisions on cut-off and release dates, and can exclude or revert changes to ensure the release schedule and quality targets are met.

### Release Tracking

Each release should have its own Release Tracking Issue on GitHub, which contains a schedule and a checklist of items to be executed in the correct order.

Also each release should have a dedicated 20YY.MM github milestone. All contributors are encouraged to use it for organizing their work and to communicate clearly with the Release Manager about their schedules, as well as any potential risks or regressions after merging each feature.

Any regressions discovered must be filed as blocking bug tickets and added to the milestone with a clear and understandable description. Contributors are encouraged to proactively revert their changes if it is unrealistic to stabilize them before the cut-off date.

Any decision made must be documented in writing within the Release Tracking Issue, ensuring that the motivation and reasoning are clear and understandable to all.

## Process

TO BE DOCUMENTED HERE

## Recipies

Below is a list of useful snippets for any ad-hoc tasks that may arise during the execution of the process.

### AppStore: uploading metadata and screenshots

Use [GitHub Actions](../.github/workflows/ios-release.yaml).

### AppStore: checking metadata

Use [GitHub Actions](../.github/workflows/ios-check.yaml).

Local check:

```bash
./tools/python/check_store_metadata.py ios
```

### AppStore: downloading screenshots

Get xcode/keys/appstore.json - App Store API Key.

Get screenshots/ - a repository with screenshots.

Download metadata:

```bash
cd xcode
./fastlane download_metadata
```

Download screenshots:

```bash
cd xcode
./fastlane download_screenshots
```

### Google Play: uploading metadata and screenshots

Use [GitHub Actions](../.github/workflows/android-release-metadata.yaml).

### Google Play: uploading a new version

Use [GitHub Actions](../.github/workflows/android-release.yaml).

Promote version to "Production" manually in Google Play Console.

### Huawei AppGallery: uploading a new version

Use [GitHub Actions](../.github/workflows/android-release.yaml).

### Google Play: checking metadata

Use [GitHub Actions](../.github/workflows/android-check.yaml).

Checking locally:

```bash
./tools/python/check_store_metadata.py android
```

### Google Play: downloading metadata and screenshots

Get `android/google-play.json` - Google Play API Key.

Get `screenshots/` - a repository with screenshots.

Download metadata:

```bash
./tools/android/download_googleplay_metadata.sh
```
