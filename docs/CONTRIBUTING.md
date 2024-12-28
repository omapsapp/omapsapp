# Contributing

Thank you for your interest in contributing to Organic Maps (OM)!

## How Can I Contribute?

- [Donate](https://organicmaps.app/donate/)
- [Submit a bug report or a feature request](#bug-reports-and-feature-requests)

There are things to do for everyone:
- [For translators](#translations)
- [For UI/UX and graphic designers](#uiux-map-styling-and-icons)
- [For developers](#code-contributions)
- [Day-to-day activities](#day-to-day-activities) like user support, testing, issue management, community relations, etc.
- [Submitting your work](#submitting-your-changes)

If you'd like to help in any other way or if there are any related questions - please [contact us](COMMUNICATION.md).

### Bug Reports and Feature Requests

[Submit an issue](https://github.com/organicmaps/organicmaps/issues) and describe your feature idea or report a bug.
Please check if there are no similar issues already submitted by someone else.

When reporting a bug please provide as much information as possible: OS and application versions,
list of actions leading to a bug, a log file produced by the app.

When using Organic Maps app on a device, use the built-in "Report a bug" option:
on Android it creates a new e-mail with a log file attached. Your issue will be processed much
faster if you send it to <bugs@organicmaps.app>. Enabling logs in Organic Maps settings on Android
before sending the bug report also helps us a lot with debugging.

If your idea is very broad or raw then instead of a specific feature request consider [starting a discussion thread](https://github.com/organicmaps/organicmaps/discussions/categories/ideas).

### Translations

OM is available in 35 languages already, but some of them are incomplete and existing translations need regular updates as the app evolves.
See [translations instructions](TRANSLATIONS.md) for details.

### UI/UX, map styling and icons

Organic Maps has a strong focus on easy to use UI and smooth user experience. Feel free to join [UI/UX discussions](https://github.com/organicmaps/organicmaps/issues?q=is%3Aopen+is%3Aissue+label%3AUX) in relevant issues. Mockups are very welcome! Check some [existing designs](https://github.com/organicmaps/organicmaps/wiki/Design-Index).

If you're into graphic design then OM needs good, clear and free-to-use icons for hundreds of map features / POIs.
Check OM's [graphic resources and design guidelines](https://github.com/organicmaps/organicmaps/wiki#design) and existing [requests for icons](https://github.com/organicmaps/organicmaps/issues?q=is%3Aopen+is%3Aissue+label%3AIcons). Post your icons onto relevant issues or take a next step and [integrate them](STYLES.md) yourself.

Check the [map styling instructions](STYLES.md) and work on adding new map features and other open [styles requests](https://github.com/organicmaps/organicmaps/issues?q=is%3Aopen+is%3Aissue+label%3AStyles).

### Code Contributions

Please follow instructions in [INSTALL.md](INSTALL.md) to set up your development environment.
You will find a list of issues for new contributors [here](https://github.com/organicmaps/organicmaps/labels/Good%20first%20issue) to help you get started with simple tasks. If you want to focus on the most important issues, please check [this label](https://github.com/organicmaps/organicmaps/labels/Frequently%20Reported%20by%20Users) or our [Milestones](https://github.com/organicmaps/organicmaps/milestones).

**We do not assign issues to first-time contributors.** Any such request notifies our contributors and the development team, and creates unnecessary noise that distracts us from the work. Just make a PR - and it will be reviewed.

Sometimes it's better to discuss and confirm your vision of the fix or implementation before working on an issue. Our main focus is on simplicity and convenience for everyone, not only for geeks.

Please [learn how to use `git rebase`](https://git-scm.com/book/en/v2/Git-Branching-Rebasing) (or rebase via any git tool with a graphical interface, e.g. [Fork for Mac](https://git-fork.com/) is quite good) to amend your commits in the PR and maintain a clean logical commit history for your changes/branches.

While we strive to help onboard new developers we don't have enough time to guide everyone step-by-step and explain in detail how everything works (in many cases we have to study the code ourselves). You'll need to be largely self-sufficient, expect to read a lot of code and documentation.

- [Pull Request Guide](PR_GUIDE.md).
- [How to write a commit message](COMMIT_MESSAGES.md).
- [Directories structure](STRUCTURE.md)
- [C++ Style Guide](CPP_STYLE.md).
- [Java Style Guide](JAVA_STYLE.md).
- [Objective-C Style Guide](OBJC_STYLE.md).

...and more in the [docs folder](./) of the repository.

### Day-to-day Activities

Please help us:
- processing users questions and feedback in chats, app stores, email and social media and creating follow-up issues or updating existing ones
- reproducing and triaging reported bugs
- testing upcoming features and bug fixes for Android, iOS and desktop versions
- keeping [github issues](https://github.com/organicmaps/organicmaps/issues) in order (check for duplicates, organize, assign labels, link related issues, etc.)
- composing nice user-centric release notes and news items
- etc.

## Submitting your changes

All contributions to Organic Maps repositories should be submitted via
[Github pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork)
and signed-off with the [Developers Certificate of Origin](#legal-requirements).

Each pull request is reviewed by OM maintainers to ensure its quality.
Sometimes the review process even for smallest commits can be
very thorough.

### Legal Requirements

When contributing to this project, you must agree that you have authored 100%
of the content, that you have the necessary rights to the content and that
the content you contribute may be provided under the project license.

To contribute you must assure that you have read and are following the rules
stated in the [Developers Certificate of Origin](DCO.md) (DCO). We have
borrowed this procedure from the Linux kernel project to improve tracking of
who did what, and for legal reasons.

To sign-off a patch, just add a line in the commit message saying:

    Signed-off-by: Some Developer <somedev@example.com>

Git has a flag that can sign a commit for you. An example using it is:

    git commit -s -m 'An example commit message'

Use your real name or on some rare cases a company email address, but we
disallow pseudonyms or anonymous contributions.

### Code of Conduct

The OM community abides by the [CNCF code of conduct](CODE_OF_CONDUCT.md).
